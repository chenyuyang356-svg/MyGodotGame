#pragma once

#include "building_manager.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

BuildingManager::BuildingManager() {}
BuildingManager::~BuildingManager() {}

void BuildingManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void BuildingManager::set_unit_manager(Node* p_node) {
    unit_manager = Object::cast_to<UnitManager>(p_node);
}

void BuildingManager::update(double p_delta) {
    update_multimesh_buffer(p_delta);
}

void BuildingManager::update_multimesh_buffer(double p_delta) {
    if (type_renderers.empty()) return;

    // 1. 分组
    for (auto& pair : type_grouping_cache) pair.second.clear();
    for (auto const& [id, data] : buildings) {
        type_grouping_cache[data.stats.ptr()].push_back(id);
    }

    // 假设默认单元格大小
    Vector2 cell_sz = Vector2(32, 32);
    if (flow_field_manager) {
        cell_sz = Vector2(flow_field_manager->get_cell_size());
    }

    // 2. 遍历渲染
    for (auto& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& ids = type_grouping_cache[s_ptr];
        int count = (int)ids.size();

        // 获取主体和影子 MultiMesh
        Ref<MultiMesh> mm = mmi->get_multimesh();
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();

        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
            s_mm->set_instance_count(count);
        }

        for (int i = 0; i < count; ++i) {
            BuildingData& b = buildings[ids[i]];

            // --- A. 主体位置 ---
            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
            float fake_depth_offset = center.y * 0.0001f;

            Transform3D xform;
            xform.origin = Vector3(center.x, fake_depth_offset, center.y);
            xform.basis = Basis().rotated(Vector3(1, 0, 0), -Math_PI / 2.0);
            mm->set_instance_transform(i, xform);

            // --- B. 影子位置 ---
            // 影子偏移量：X+4, Z+4 (仿照 UnitManager)
            // 影子高度稍微降低 (-0.1) 以免和主体重叠
            Transform3D s_xform;
            s_xform.origin = Vector3(center.x + 4.0f, fake_depth_offset - 0.1f, center.y + 4.0f);
            s_xform.basis = xform.basis; // 旋转角度一致
            s_mm->set_instance_transform(i, s_xform);

            // --- C. 动画同步 ---
            int frames = (b.state == BuildingState::WORKING) ? s_ptr->get_working_frames() : s_ptr->get_idle_frames();
            int row = (b.state == BuildingState::WORKING) ? s_ptr->get_working_row() : s_ptr->get_idle_row();
            float duration = (float)frames / s_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(b.anim_time, duration) * s_ptr->get_anim_fps());

            float modulate = 1.0f;
            
            Color anim_data = Color((float)frame_idx, (float)row, modulate, 0);
            mm->set_instance_custom_data(i, anim_data);
            s_mm->set_instance_custom_data(i, anim_data); // 影子也播放同样动作

            mm->set_instance_color(i, get_team_color(b.team_id));

            b.anim_time += (float)p_delta;
        }
    }
}

void BuildingManager::register_building_type(String p_name, String p_path) {
    Ref<BuildingStats> stats = BuildingLoader::load_from_txt(p_path);
    if (stats.is_null()) return;

    building_types_cache[p_name] = stats;
    BuildingStats* s_ptr = stats.ptr();

    if (type_renderers.count(s_ptr)) return;

    // --- A. 初始化建筑主体渲染器 ---
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_name + "_Renderer");
    add_child(mmi);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();

    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    // 设置主体材质
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (building_shader.is_null()) {
        building_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shader.gdshader");
    }
    mat->set_shader(building_shader);
    mat->set_shader_parameter("albedo_texture", tex);
    mat->set_shader_parameter("h_frames", stats->get_h_frames());
    mat->set_shader_parameter("v_frames", stats->get_v_frames());
    mmi->set_material_override(mat);
    type_renderers[s_ptr] = mmi;

    // --- B. 初始化影子渲染器 ---
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadow");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true); // 影子也要同步动画帧

    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    s_qmesh->set_size(qmesh->get_size()); // 影子大小与建筑一致
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    // 设置影子材质
    Ref<ShaderMaterial> s_mat;
    s_mat.instantiate();
    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shadow.gdshader");
    }
    s_mat->set_shader(shadow_shader);
    s_mat->set_shader_parameter("albedo_texture", tex);
    s_mat->set_shader_parameter("h_frames", stats->get_h_frames());
    s_mat->set_shader_parameter("v_frames", stats->get_v_frames());
    s_mmi->set_material_override(s_mat);
    shadow_renderers[s_ptr] = s_mmi;
}

bool BuildingManager::is_area_clear(Vector2i p_grid_pos, Ref<BuildingStats> p_stats) {
    if (!flow_field_manager || p_stats.is_null()) return false;

    Vector2i footprint = p_stats->get_footprint();
    Vector2i clearance = p_stats->get_clearance_size();
    uint32_t req = p_stats->get_placement_requirement();

    // 1. 计算 Clearance 检查的起始点
    // 假设 p_grid_pos 是建筑实际占用(footprint)的左上角
    // 偏移量 = (Clearance尺寸 - Footprint尺寸) / 2
    Vector2i offset = (clearance - footprint) / 2;
    Vector2i check_start = p_grid_pos - offset;

    // --- 逻辑检查 A: 地形与 Clearance 范围 ---
    // 这个范围内不能有任何已存在的建筑 (Cost 255)
    for (int x = 0; x < clearance.x; ++x) {
        for (int y = 0; y < clearance.y; ++y) {
            Vector2i current_cell = check_start + Vector2i(x, y);

            if (!flow_field_manager->is_in_grid(current_cell)) return false;

            // 如果要求在陆地：陆地层不能是墙 (255)
            if (req & PLACE_LAND) {
                if (flow_field_manager->get_cost(current_cell, NAV_LAND) == 255) { return false; }
            }

            // 如果要求在水上：检查 NAV_SEA 层
            if (req & PLACE_WATER) {
                if (flow_field_manager->get_cost(current_cell, NAV_SEA) == 255) { return false; }
            }
        }
    }

    // --- 逻辑检查 B: 单位阻挡 (仅检查 Footprint 范围) ---
    if (unit_manager) {
        Vector2i cell_size = flow_field_manager->get_cell_size();
        Vector2 world_pos = Vector2(p_grid_pos.x * cell_size.x, p_grid_pos.y * cell_size.y);
        Vector2 world_size = Vector2(footprint.x * cell_size.x, footprint.y * cell_size.y);
        Rect2 building_rect(world_pos, world_size);

        Vector2 center = world_pos + world_size * 0.5;
        float query_radius = world_size.length() * 0.5;

        std::vector<int> nearby_units = unit_manager->get_nearby_units(center, query_radius);
        for (int unit_idx : nearby_units) {
            if (building_rect.has_point(unit_manager->units[unit_idx].position)) {
                return false;
            }
        }
    }

    return true;
}

int BuildingManager::place_building_by_type(String p_type_name, Vector2i p_grid_pos, int p_team_id) {
    if (!building_types_cache.has(p_type_name)) return -1;

    Ref<BuildingStats> stats = building_types_cache[p_type_name];
    uint32_t req = stats->get_placement_requirement();

    if (!is_area_clear(p_grid_pos, stats)) return -1;

    // 1. 创建建筑
    int b_id = next_building_id++;
    BuildingData b;
    b.id = b_id;
    b.grid_pos = p_grid_pos;
    b.stats = stats;
    b.team_id = p_team_id;
    b.current_health = stats->get_health_max();
    buildings[b_id] = b;

    // 2. 修改代价地图：注意这里只设置 Footprint 的格子
    Vector2i footprint = stats->get_footprint();
    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            if (req & PLACE_LAND) {
                flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255, NAV_LAND);
            }
            if (req & PLACE_WATER) {
                flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255, NAV_SEA);
            }
        }
    }

    flow_field_manager->make_all_dirty();
    return b_id;
}

void BuildingManager::remove_building(int p_building_id) {
    auto it = buildings.find(p_building_id);
    if (it == buildings.end()) return;

    BuildingData& b = it->second;
    Vector2i footprint = b.stats->get_footprint();
    uint32_t req = b.stats->get_placement_requirement();

    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            if (req & PLACE_LAND) {
                flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1, NAV_LAND);
            }
            if (req & PLACE_WATER) {
                flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1, NAV_SEA);
            }
        }
    }

    buildings.erase(it);
    flow_field_manager->make_all_dirty();
}

Vector2i BuildingManager::get_building_grid_pos(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.grid_pos;
    return Vector2i(-1, -1);
}

Ref<BuildingStats> BuildingManager::get_building_stats(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.stats;
    return nullptr;
}

PackedStringArray BuildingManager::get_registered_building_types() const {
    PackedStringArray list;
    // 遍历 HashMap 的所有键
    for (const auto& E : building_types_cache) {
        list.append(E.key);
    }
    return list;
}

void BuildingManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_building_type", "name", "path"), &BuildingManager::register_building_type);
    ClassDB::bind_method(D_METHOD("place_building_by_type", "type_name", "grid_pos", "team_id"), &BuildingManager::place_building_by_type);
    ClassDB::bind_method(D_METHOD("is_area_clear", "grid_pos", "building_stats"), &BuildingManager::is_area_clear);

    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &BuildingManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &BuildingManager::set_unit_manager);
    ClassDB::bind_method(D_METHOD("remove_building", "building_id"), &BuildingManager::remove_building);
    ClassDB::bind_method(D_METHOD("get_building_grid_pos", "building_id"), &BuildingManager::get_building_grid_pos);
    ClassDB::bind_method(D_METHOD("get_building_stats", "building_id"), &BuildingManager::get_building_stats);

    ClassDB::bind_method(D_METHOD("get_registered_building_types"), &BuildingManager::get_registered_building_types);
    ClassDB::bind_method(D_METHOD("get_building_stats_by_type", "type_name"), &BuildingManager::get_building_stats_by_type);
}