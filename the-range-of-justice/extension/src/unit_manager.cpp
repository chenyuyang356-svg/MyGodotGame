#pragma once
#include "unit_manager.h"
#include "attack_manager.h"
#include "selection_manager.h"

#include <queue>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include "game_manager.h"
#include "building_manager.h"

using namespace godot;

UnitManager::UnitManager() {
    units.reserve(1000);
}

UnitManager::~UnitManager() {}

// 1.初始化管理器
void UnitManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
    if (!flow_field_manager) {
        flow_field_manager = get_node<FlowFieldManager>("../FlowFieldManager");
        if (!flow_field_manager) return;
    }
    
    flow_field_manager->setup_grid(p_width, p_height, p_origin, p_cell_size);

    unit_grid_width = p_width / 2;
    unit_grid_height = p_height / 2;
    unit_grid_size = unit_grid_width * unit_grid_height;
    unit_grid_cell_size = p_cell_size * 2;

    unit_grid.resize(unit_grid_size);

    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].reserve(10);
    }

    _setup_hp_bar_system();
    _setup_minimap_renderer(p_width, p_height, p_cell_size);

    is_setup = true;
}

void UnitManager::_setup_hp_bar_system() {
    if (global_hp_bar_renderer) return;

    global_hp_bar_renderer = memnew(MultiMeshInstance3D);
    global_hp_bar_renderer->set_name("GlobalHPBars");
    add_child(global_hp_bar_renderer);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_custom_data(true); // X: HP比例, Y: 团队颜色/可见性
    mm->set_use_colors(true);      // 也可以用颜色存团队色

    Ref<QuadMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector2(1.0, 1.0)); // 初始尺寸设为1，后面在Transform里缩放
    mm->set_mesh(mesh);

    global_hp_bar_renderer->set_multimesh(mm);

    // 材质设置
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (hp_bar_shader.is_null()) {
        hp_bar_shader = ResourceLoader::get_singleton()->load("res://shader/hp_bar.gdshader");
    }
    // 传入实时视野贴图
    mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
    // 传入地图尺寸
    mat->set_shader_parameter("map_size", fog_manager->get_map_size());
    // 传入地图位置
    mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    mat->set_shader(hp_bar_shader);

    global_hp_bar_renderer->set_material_override(mat);
}

void UnitManager::_setup_minimap_renderer(int p_width, int p_height, Vector2i p_cell_size) {
    minimap_dot_renderer = memnew(MultiMeshInstance3D);
    minimap_dot_renderer->set_name("MinimapDots");
    // 重要：只在 Layer 2 显示（小地图相机），不在主相机 Layer 1 显示
    minimap_dot_renderer->set_layer_mask(2);
    add_child(minimap_dot_renderer);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true); // 用于传递队伍颜色

    Ref<QuadMesh> qm;
    qm.instantiate();

    Vector2 dot_size = Vector2(1.0f, 1.0f);
    float dot_scale = float(std::max(p_width * p_cell_size.x, p_height * p_cell_size.y)) * MINIMAP_DOT_SCALE;
    dot_size *= dot_scale;
    
    qm->set_size(dot_size); // 小地图点的大小（世界坐标单位）
    mm->set_mesh(qm);

    minimap_dot_renderer->set_multimesh(mm);

    // 简单材质：不接受光照，只显示颜色
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (minimap_dot_shader.is_null()) {
        // 简单 Shader： COLOR = INSTANCE_CUSTOM 或是直接用 Vertex Color
        minimap_dot_shader = ResourceLoader::get_singleton()->load("res://shader/minimap_dot.gdshader");
    }
    mat->set_shader(minimap_dot_shader);
    // 传入实时视野贴图
    mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
    // 传入地图尺寸
    mat->set_shader_parameter("map_size", fog_manager->get_map_size());
    // 传入地图位置
    mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    minimap_dot_renderer->set_material_override(mat);
}

// 2.单位生命周期（生成，死亡判定，内存回收）
int UnitManager::spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id, int p_forced_id) {
    if (p_stats.is_null()) return -1;

    UnitData new_unit;
    // 如果传入了有效的 forced_id (客户端模式)，则使用它；否则使用自增 ID (服务器模式)
    if (p_forced_id != -1) {
        new_unit.id = p_forced_id;
        // 更新 next_unit_id 以防冲突（可选，但安全）
        if (p_forced_id >= next_unit_id) next_unit_id = p_forced_id + 1;
    }
    else {
        new_unit.id = next_unit_id++;
    }

    new_unit.position = p_world_pos;
    new_unit.prev_position = p_world_pos;
    new_unit.next_position = p_world_pos;
    new_unit.height = p_stats->base_height;
    new_unit.stats = p_stats; 
    new_unit.weapon_cooldowns.resize(p_stats->weapons.size(), 0.0f);
    new_unit.path_recheck_timer = (float)(new_unit.id % 60) / 60.0f;
    new_unit.last_visual_pos = p_world_pos;
    new_unit.dust_accumulator = 0.0f;

    new_unit.weapons.clear();
    for (const auto& mount : p_stats->weapon_mounts) {
        WeaponData wd;
        wd.stats = mount.weapon_resource;
        wd.local_position = mount.local_position;
        new_unit.weapons.push_back(wd);
    }


    new_unit.team_id = p_team_id; 

 
    new_unit.current_health = p_stats->get_health_max();
    new_unit.state = IDLE;

    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;
    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id, SelectionManager* p_selection_manager) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

    UnitData& unit_to_remove = units[index_to_remove];
    group_manager->handle_unit_death(unit_to_remove);

    if (index_to_remove != last_unit_idx) {
        // 1.   ȡ   һ    λ      
        UnitData& last_unit = units.back();

        // 2.      һ    λ ƶ   Ҫɾ    λ  
        units[index_to_remove] = last_unit;

        // 3.    ±  ƶ   λ ڹ ϣ   е     
        id_to_index[last_unit.id] = index_to_remove;
    }

   
    units.pop_back();
    id_to_index.erase(p_unit_id);
    p_selection_manager->on_unit_despawned(p_unit_id);
}

void UnitManager::handle_dead_unit(double p_delta) {
    for (int unit_idx = units.size() - 1; unit_idx >= 0; --unit_idx) {
        UnitData& unit = units[unit_idx];
        if (unit.current_health <= 0) {
            unit.state = DYING;
        }

        if (unit.state == DYING) {
            unit.current_dying_time += p_delta;
            if (unit.current_dying_time >= unit.stats->dying_time) {
                emit_signal("despawn_unit_requested", unit.id);
            }
        }
    }
}

// 3.指令下发
void UnitManager::command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos) {
    if (!flow_field_manager || p_unit_ids.is_empty()) return;

    Vector2i original_target_grid = flow_field_manager->world_to_grid(p_target_world_pos);
    if (!(flow_field_manager->is_in_grid(original_target_grid))) return;

    // 创建一个新的临时组 ID
    int temp_gid = group_manager->create_temporary_group(p_target_world_pos);

    // 1. 将选中的单位按高度分类
    std::vector<int> ground_indices;
    std::vector<int> air_indices;

    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            int unit_internal_idx = it->second;
            if (units[unit_internal_idx].height > AIR_HEIGHT_THRESHOLD) {
                air_indices.push_back(unit_internal_idx);
            }
            else {
                ground_indices.push_back(unit_internal_idx);
            }
        }
    }

    // 记录本批指令中已请求过的导航类型，避免重复触发流场计算
    bool requested_types[NAV_MAX] = { false };

    // 我们需要记录每种 Navigation Type 修正后的目标点
    // 因为地面单位可能需要修正，而飞行单位（NAV_AIR）不需要
    Vector2 corrected_world_targets[NAV_MAX];
    Vector2i corrected_grid_targets[NAV_MAX];
    bool type_initialized[NAV_MAX] = { false };

    // 2. 定义处理阵型分配的 Lambda 闭包
    auto process_sub_group = [&](const std::vector<int>& indices) {
        int sub_count = indices.size();
        if (sub_count == 0) return;

        // 获取该组单位的导航类型
        int nav_type = units[indices[0]].get_nav_type();

        // --- 核心修改：目标点投影 ---
        if (!type_initialized[nav_type]) {
            // 检查原始点击的格子是否可行走 (Cost < 255)
            if (flow_field_manager->get_cost(original_target_grid, nav_type) < 255) {
                // 如果点击位置本身就是合法的，直接使用精确的鼠标位置
                corrected_grid_targets[nav_type] = original_target_grid;
                corrected_world_targets[nav_type] = p_target_world_pos;
            }
            else {
                // 如果点击了障碍物，寻找最近的可达格子
                Vector2i best_grid = flow_field_manager->find_nearest_walkable_cell(original_target_grid, nav_type);
                corrected_grid_targets[nav_type] = best_grid;

                // 计算该格子的 AABB 范围
                Vector2i cell_sz = flow_field_manager->get_cell_size();
                Vector2 cell_min = Vector2(best_grid.x * cell_sz.x, best_grid.y * cell_sz.y);
                Vector2 cell_max = cell_min + Vector2(cell_sz.x, cell_sz.y);

                // 将原始鼠标位置投影到该合法格子的 AABB 内 (保留 1.0f 的安全边距)
                corrected_world_targets[nav_type] = Vector2(
                    Math::clamp(p_target_world_pos.x, cell_min.x + 1.0f, cell_max.x - 1.0f),
                    Math::clamp(p_target_world_pos.y, cell_min.y + 1.0f, cell_max.y - 1.0f)
                );
            }
            type_initialized[nav_type] = true;
        }

        Vector2i final_grid_pos = corrected_grid_targets[nav_type];
        Vector2 final_world_pos = corrected_world_targets[nav_type];

        // 阵型计算逻辑
        int cols = (int)Math::ceil(Math::sqrt((float)sub_count));
        int rows = (sub_count + cols - 1) / cols;

        for (int i = 0; i < sub_count; i++) {
            UnitData& unit = units[indices[i]];

            // 使用修正后的 final_world_pos 计算偏移
            float spacing = unit.stats->get_collision_radius();
            int r = i / cols;
            int c = i % cols;
            Vector2 offset;
            offset.x = (c - (cols - 1) * 0.5f) * spacing;
            offset.y = (r - (rows - 1) * 0.5f) * spacing;

            // 检查偏移点是否在墙里（如果是地面单位）
            if (unit.height <= AIR_HEIGHT_THRESHOLD) {
                Vector2 pot_pos = final_world_pos + offset;
                Vector2i pot_grid = flow_field_manager->world_to_grid(pot_pos);
                if (flow_field_manager->get_cost(pot_grid, nav_type) == 255) {
                    offset = Vector2(0, 0);
                }
            }
            unit.target_pos_offset = offset;

            // --- 导航与流场逻辑 ---
            // 使用修正后的 final_world_pos
            unit.target_pos = final_world_pos;
            unit.target_grid = final_grid_pos;

            bool is_traversable = flow_field_manager->is_path_traversable(
                unit.position, unit.target_pos, nav_type, density_limit
            );
            unit.use_direct_path = is_traversable;

            if (!is_traversable && !requested_types[nav_type]) {
                // 为修正后的坐标创建流场
                flow_field_manager->create_flow_field(final_grid_pos, nav_type, false);
                requested_types[nav_type] = true;
            }

            // --- 更新组管理逻辑 ---
            if (unit.temp_group_id != -1) {
                group_manager->remove_unit_from_temp_group(unit.temp_group_id, unit);
            }
            group_manager->add_unit_to_temp_group(temp_gid, unit.id);
            unit.temp_group_id = temp_gid;

            // --- 更新单位状态属性 ---
            unit.state = MOVING;
            unit.target_id = -1;
            unit.is_patrolling = false;
            unit.is_manual_target = false;
            unit.stuck_timer = 0.0f;
            unit.last_stuck_check_pos = unit.position;
        }
        };

    // 3. 分别对地面和空中组执行逻辑
    process_sub_group(ground_indices);
    process_sub_group(air_indices);
}

void UnitManager::command_units_to_patrol(Array p_unit_ids, Array p_waypoints) {
    if (p_unit_ids.is_empty()) return;
    
    std::vector<Vector2> waypoints;
    for (int i = 0; i < p_waypoints.size(); ++i) {
        waypoints.push_back(p_waypoints[i]);
    }
    if (waypoints.empty()) return;

    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];

            if (unit.temp_group_id != -1) {
                group_manager->remove_unit_from_temp_group(unit.temp_group_id, unit);
            }

            unit.is_patrolling = true;
            unit.patrol_waypoints = waypoints;
            unit.current_waypoint_idx = 0;
            unit.state = PATROLLING;
            unit.target_id = -1; 
            unit.is_patrolling = true;
            unit.is_manual_target = false; 
            unit.target_id = -1;
            unit.stuck_timer = 0.0f;
            unit.last_stuck_check_pos = unit.position;
        }
       
    }
}

void UnitManager::command_units_to_attack_target(Array p_unit_ids, int p_target_id, bool p_target_is_building, BuildingManager* p_building_manager) {
    // 1. 获取目标所属队伍
    int target_team = -1;
    if (p_target_is_building) {
        target_team = p_building_manager->get_building_team_id(p_target_id);
    }
    else {
        int t_idx = get_unit_index_by_id(p_target_id);
        if (t_idx != -1) target_team = units[t_idx].team_id;
    }

    for (int i = 0; i < p_unit_ids.size(); ++i) {
        int idx = get_unit_index_by_id(p_unit_ids[i]);
        if (idx == -1) continue;

        UnitData& u = units[idx];
        bool is_builder = (u.stats->get_unit_tags() & TAG_BUILDER);
        bool target_is_ally = (target_team == u.team_id);

        // A. 指令目标是【敌方】：建造者直接跳过，不执行任何操作
        if (!target_is_ally && is_builder) continue;

        // B. 指令目标是【己方】：非建造者单位跳过（只有建造者能“攻击”己方执行修复/建造）
        if (target_is_ally && !is_builder) continue;

        // 执行追击/建造指令
        u.target_id = p_target_id;
        u.target_is_building = p_target_is_building;
        u.is_manual_target = true;
        u.stuck_timer = 0.0f;
        u.last_stuck_check_pos = u.position;
        u.state = CHASING;

        // 停止之前的移动
        u.use_direct_path = false;
        u.target_pos_offset = Vector2(0, 0);
    }
}

// 4.空间划分
void UnitManager::update_spatial_grid() {
    // 清空上一帧网格数据并即时更新
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].clear();
    }
    for (int i = 0; i < units.size(); ++i) {
        Vector2i rel_pos = flow_field_manager->world_to_relative(units[i].position);

        int ux = rel_pos.x / 2;
        int uy = rel_pos.y / 2;

        if (ux >= 0 && ux < unit_grid_width && uy >= 0 && uy < unit_grid_height) {
            int grid_idx = uy * unit_grid_width + ux;
            unit_grid[grid_idx].push_back(i);
        }
    }
}

std::vector<int> UnitManager::get_nearby_units(Vector2 p_world_pos, float p_radius) {
    std::vector<int> nearby_indices;
    Vector2i rel_pos = flow_field_manager->world_to_relative(p_world_pos);
    int ux = rel_pos.x / 2;
    int uy = rel_pos.y / 2;
    int dx = int(p_radius / unit_grid_cell_size.x) + 1;
    int dy = int(p_radius / unit_grid_cell_size.y) + 1;

    // 获取指定半径内的单位索引
    for (int nx = ux - dx; nx <= ux + dx; ++nx) {
        for (int ny = uy - dy; ny <= uy + dy; ++ny) {
            if (nx >= 0 && nx < unit_grid_width && ny >= 0 && ny < unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    if (p_world_pos.distance_squared_to(units[unit_idx].position) < p_radius * p_radius) {
                        nearby_indices.push_back(unit_idx);
                    }
                }
            }
        }
    }
    return nearby_indices;
}

int UnitManager::get_unit_at_position(Vector2 p_world_pos) {
    int best_id = -1;
    float max_height = -100.0f;
    Vector2i rel_pos = flow_field_manager->world_to_relative(p_world_pos);
    int ux = rel_pos.x / 2;
    int uy = rel_pos.y / 2;

    // 加入大型单位后需要单独处理
    for (int nx = ux - 1; nx <= ux + 1; ++nx) {
        for (int ny = uy - 1; ny <= uy + 1; ++ny) {
            if (nx >= 0 && nx < unit_grid_width && ny >= 0 && ny < unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    UnitData& unit = units[unit_idx];
                    if (p_world_pos.distance_squared_to(unit.position) < unit.get_squared_radius()) {
                        if (unit.height > max_height) {
                            max_height = unit.height;
                            best_id = unit.id;
                        }
                    }
                }
            }
        }
    }

    return best_id;
}

// 5.框选相关
std::vector<int> UnitManager::get_units_of_type_in_area(Ref<UnitStats> p_stats, Rect2 p_rect, int p_team_id) {
    std::vector<int> result;
    Vector2i unit_grid_rect_pos = flow_field_manager->world_to_relative(p_rect.position) / 2;
    Vector2i unit_grid_rect_end = flow_field_manager->world_to_relative(p_rect.get_end()) / 2 + Vector2i(1, 1);
    
    for (int nx = unit_grid_rect_pos.x; nx <= unit_grid_rect_end.x; ++nx) {
        for (int ny = unit_grid_rect_pos.y; ny <= unit_grid_rect_end.y; ++ny) {
            if (nx >= 0 && nx <= unit_grid_width && ny >= 0 && ny <= unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    UnitData& unit = units[unit_idx];
                    if (p_rect.has_point(unit.position)) {
                        if (unit.stats == p_stats && unit.team_id == p_team_id) {
                            result.push_back(unit.id);
                        }
                    }
                }
            }
        }
    }

    return result;
}

std::vector<int> UnitManager::get_units_in_box(Rect2 p_box, int p_team_id) {
    std::vector<int> result;
    Vector2i unit_grid_rect_pos = flow_field_manager->world_to_relative(p_box.position) / 2;
    Vector2i unit_grid_rect_end = flow_field_manager->world_to_relative(p_box.get_end()) / 2 + Vector2i(1, 1);

    for (int nx = unit_grid_rect_pos.x; nx <= unit_grid_rect_end.x; ++nx) {
        for (int ny = unit_grid_rect_pos.y; ny <= unit_grid_rect_end.y; ++ny) {
            if (nx >= 0 && nx <= unit_grid_width && ny >= 0 && ny <= unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    UnitData& unit = units[unit_idx];
                    if (p_box.has_point(unit.position)) {
                        if (unit.team_id == p_team_id) {
                            result.push_back(unit.id);
                        }
                    }
                }
            }
        }
    }

    return result;
}

// 核心更新循环
void UnitManager::update(double p_delta) {
    if (!is_setup || !flow_field_manager) { return; }

    handle_dead_unit(p_delta);
    update_spatial_grid();

    // --- 动态密度图：加细采样 ---
    density_update_timer += p_delta;
    if (density_update_timer >= 0.5) { // 每0.5秒更新一次精细密度
        density_update_timer = 0.0;

        // 创建两个与流场分辨率完全一致的空 Map
        int f_width = flow_field_manager->get_width();
        int f_height = flow_field_manager->get_height();

        std::vector<float> ground_buffer(f_width * f_height, 0.0f);
        std::vector<float> air_buffer(f_width * f_height, 0.0f);

        Vector2i origin = flow_field_manager->get_grid_origin();

        for (const auto& unit : units) {
            if (unit.state == DYING) continue;

            Vector2i f_grid = flow_field_manager->world_to_grid(unit.position) - origin;
            Vector2i next_f_grid = flow_field_manager->world_to_grid(unit.position + (unit.velocity).normalized() *
                (float)((flow_field_manager->get_cell_size()).x)) - origin;
            float k = (unit.state == IDLE) ? 5.0f : 1.0f;
            float radius = unit.stats->get_collision_radius() / 100.0f;

            if (f_grid.x >= 0 && f_grid.x < f_width && f_grid.y >= 0 && f_grid.y < f_height) {
                // 根据高度分流
                if (unit.height > AIR_HEIGHT_THRESHOLD) {
                    air_buffer[f_grid.y * f_width + f_grid.x] += radius * radius * k;
                    if ((next_f_grid.x >= 0 && next_f_grid.x < f_width && next_f_grid.y >= 0 && next_f_grid.y < f_height) &&
                        (next_f_grid != f_grid)) {
                        air_buffer[next_f_grid.y * f_width + next_f_grid.x] += 0.8 * radius * radius * k;
                    }
                }
                else {
                    ground_buffer[f_grid.y * f_width + f_grid.x] += radius * radius * k;
                    if ((next_f_grid.x >= 0 && next_f_grid.x < f_width && next_f_grid.y >= 0 && next_f_grid.y < f_height) &&
                        (next_f_grid != f_grid)) {
                        ground_buffer[next_f_grid.y * f_width + next_f_grid.x] += 0.8 * radius * radius * k;
                    }
                }
            }
        }

        // 分别注入
        flow_field_manager->inject_density_and_blur(0, ground_buffer);
        flow_field_manager->inject_density_and_blur(1, air_buffer);

        flow_field_manager->make_all_dirty();
    }
}

void UnitManager::physics_update(double p_delta) {
    

    if (attack_manager) {
        attack_manager->update_units(p_delta);
    }

    flow_field_manager->update(p_delta);

    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];

        unit.prev_position = unit.position;
        unit.prev_height = unit.height;
        unit.prev_rotation = unit.rotation;

        for (auto& weapon : unit.weapons) {
            weapon.prev_rotation = weapon.rotation;
        }

        update_state(unit, p_delta);
        update_velocity(unit, p_delta);
        move(unit, p_delta);

        unit.next_position = unit.position;
        unit.next_height = unit.height;
        unit.next_rotation = unit.rotation;

        for (auto& weapon : unit.weapons) {
            weapon.update(unit.rotation, p_delta);
            weapon.next_rotation = weapon.rotation;
        }
    }
}

Vector2 UnitManager::get_flow(UnitData& p_unit) {
    // 无论是否直接行进，都更新流场的使用时间，防止被清理
    flow_field_manager->touch_field(p_unit.target_grid, p_unit.get_nav_type());

    // 最终目的地（带偏移）
    Vector2 final_target = p_unit.target_pos + p_unit.target_pos_offset;
    Vector2 to_target = final_target - p_unit.position;
    float dist_sq = to_target.length_squared();

    // 如果已经到达目的地（极近距离），不再受密度影响，防止最后一点路程反复横跳
    if (dist_sq < 10.0f) return Vector2(0, 0);

    Vector2 direct_dir = to_target.normalized();

    // --- 密度避让逻辑 ---
    int d_idx = (p_unit.height > 20.0f) ? 1 : 0; // 高度阈值判断

    // 获取当前位置的密度梯度
    Vector2 density_grad = flow_field_manager->get_density_gradient(p_unit.position, d_idx);
    
    // 计算避让向量：避让强度受 density_weight 控制
    // 我们减去梯度向量（即沿着密度下降的方向走）
    // 为了防止避让力太大导致单位乱跑，我们只取垂直于移动方向的分量（Steering）
    Vector2 avoidance = Vector2(0, 0);
    if (!(p_unit.state == MOVING && p_unit.is_in_critial_area)) {
        avoidance = -(density_grad.limit_length(1.0f));
    }

    // 权衡参数：你可以根据需要调整避让强度（建议 5.0 - 20.0 之间测试）
    float avoidance_strength = 0.1f;

    // 混合方向：原始方向 + 避让方向
    Vector2 blended_dir = direct_dir + avoidance * avoidance_strength;

    // 如果是飞行单位或者已经进入直线冲刺阶段
    if (p_unit.use_direct_path) {
        return blended_dir.normalized();
    }

    // 否则，使用流场方向
    Vector2 flow_dir = flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos, p_unit.get_nav_type());

    // 即便是流场模式，也可以叠加一层轻微的即时避让，增强单位间的动态绕行感
    return (flow_dir + avoidance * 0.02f).normalized();
}

Vector2 UnitManager::get_separation(UnitData& p_unit) {
    bool is_IDLE = (p_unit.state == IDLE);
    Vector2 separation = Vector2(0, 0);

    if (p_unit.state == MOVING && p_unit.is_in_critial_area) {
        return separation;
    }

    float collision_radius = p_unit.stats->get_collision_radius();
    float search_radius = collision_radius * separation_radius_factor;
    // 关键改进：确保搜索半径足以发现附近的大型单位
    search_radius = Math::max(search_radius, collision_radius + 60.0f);

    // 扫描附近的单位，产生一个相反的推力。IDLE 状态的推力系数通常更高，以保持阵型。
    for (int unit_idx : get_nearby_units(p_unit.position, search_radius)) {
        const UnitData& nearby_unit = units[unit_idx];

        if ((p_unit.stats->move_type == MOVE_AIR) && (nearby_unit.stats->move_type != MOVE_AIR) ||
            (p_unit.stats->move_type != MOVE_AIR) && (nearby_unit.stats->move_type == MOVE_AIR)) {
            continue;
        }

        Vector2 radius_vector = nearby_unit.position - p_unit.position;
        float dist_sq = radius_vector.length_squared();
        float dist = Math::sqrt(dist_sq);

        // 计算两个单位边缘之间的理想距离
        float min_dist = (collision_radius + nearby_unit.stats->get_collision_radius()) * 1.1f;

        if (dist < min_dist && dist > 0.001f) {
            // 越拥挤，力越大
            float k = 1.0f;
            if (is_IDLE) {
                if (nearby_unit.state != IDLE) {
                    k = 2.0f;
                }
                else {
                    k = 0.5f;
                }
            }
            else {
                if (nearby_unit.state == IDLE) {
                    k = 0.1f;
                }
            }

            float push_strength = (min_dist - dist) / min_dist;
            separation -= (radius_vector / dist) * push_strength * separation_factor * k;
        }
    }

    separation = separation.limit_length(separation_limit);
    return separation;

}

Vector2 UnitManager::get_friction(UnitData& p_unit) {
    return (-p_unit.velocity);
}

// 返回外部环境产生的“原始力”(目前被弃用)
Vector2 UnitManager::get_force(UnitData& p_unit) {
    Vector2 external_force = Vector2(0, 0);

    // 1. 获取单位间的排斥力 (Separation)
    // 注意：这里的 get_separation 内部不再除以质量，返回原始推力
    external_force += get_separation(p_unit) * separation_factor;

    // 2. 战斗控制力（如果正在冲锋或被击退）
    if (attack_manager) {
        Vector2 combat_force;
        if (attack_manager->try_get_combat_force(p_unit, combat_force)) {
            external_force += combat_force;
        }
    }

    return external_force;
}

void UnitManager::stop_unit(UnitData& p_unit) {
    p_unit.state = IDLE;
    p_unit.velocity = Vector2(0, 0);
    p_unit.is_in_critial_area = false;
    if (p_unit.temp_group_id != -1) {
        group_manager->decrement_moving_count(p_unit.temp_group_id, p_unit);
    }
}

void UnitManager::update_state(UnitData& p_unit, double p_delta) {
    switch (p_unit.state) {
    case IDLE: {
        break;
    }
    case MOVING: {
        UnitGroup* group = group_manager->get_temp_group(p_unit.temp_group_id);
        if (!group) {
            // 如果组已经不存在了（被清理了），尝试停下或者重新寻找逻辑
            stop_unit(p_unit);
            break;
        }

        float radius = p_unit.stats->get_collision_radius();
        bool is_air = (p_unit.height > AIR_HEIGHT_THRESHOLD);

        float current_int = flow_field_manager->get_integration(p_unit.position, p_unit.target_pos, p_unit.get_nav_type());
        float limit_int = ((is_air) ? group->air_target_integration : group->ground_target_integration) * 1.1f +
            (radius / (float)(flow_field_manager->get_cell_size().x) *
                (flow_field_manager->get_cost(flow_field_manager->world_to_grid(p_unit.position), p_unit.get_nav_type())));

        if (current_int < limit_int + 3.0f) {
            p_unit.is_in_critial_area = true;
            p_unit.target_pos_offset = Vector2(0, 0);
        }

        float desired_distance = 1.5f * p_unit.stats->collision_radius;
        float soft_arrival_distance_squared = is_air ? group->air_idle_radius_sq_sum : group->ground_idle_radius_sq_sum;
        float soft_arrivel_distance = std::sqrtf(soft_arrival_distance_squared) * 1.1f + radius;
        soft_arrival_distance_squared = soft_arrivel_distance * soft_arrivel_distance;

        float distance_squared = (p_unit.position).distance_squared_to(p_unit.target_pos);
        float distance_squared_with_offset = (p_unit.position).distance_squared_to(p_unit.target_pos + p_unit.target_pos_offset); // 这里加上了offset
        if (distance_squared_with_offset <= desired_distance * desired_distance) {
            stop_unit(p_unit);
            break;
        }

        // --- 新增逻辑：位移判定（解决被墙或人墙堵死的情况） ---
        p_unit.stuck_timer += (float)p_delta;

        // 每 0.5 秒检查一次位移（检查频率不宜太高，要给单位挤过去的时间）
        const float STUCK_CHECK_INTERVAL = 0.5f;
        if (p_unit.stuck_timer >= STUCK_CHECK_INTERVAL) {
            // 计算这段时间内的实际位移
            float move_dist_sq = p_unit.position.distance_squared_to(p_unit.last_stuck_check_pos);
            float rotation = UtilityFunctions::angle_difference(p_unit.rotation, p_unit.last_stuck_check_rot);

            // 阈值设定
            float stuck_threshold = std::min(0.025f * (p_unit.stats->move_speed), 0.5f);
            float stuck_rotation_threshold = 0.08f * (p_unit.stats->turn_speed);

            if (move_dist_sq >= (stuck_threshold * stuck_threshold) || 
                rotation >= stuck_rotation_threshold) {
                p_unit.stuck_timer = 0.0f;
            }

            if (p_unit.stuck_timer >= 8.0f) {
                stop_unit(p_unit);
                p_unit.stuck_timer = 0.0f;
            }

            // 更新记录点
            p_unit.last_stuck_check_pos = p_unit.position;
            p_unit.last_stuck_check_rot = p_unit.rotation;
        }

        if (distance_squared <= soft_arrival_distance_squared ||
            (!is_air && current_int <= limit_int && current_int >= 0)) {
            std::vector<int> ahead_units = get_nearby_units(p_unit.position, 3.0f * p_unit.stats->collision_radius);
            for (int neighbor_idx : ahead_units) {
                UnitData& neighbor = units[neighbor_idx];
                if (neighbor.id != p_unit.id && neighbor.state == IDLE && neighbor.target_grid == p_unit.target_grid) {
                    // 如果邻居已经停在目的地附近，且离我也很近
                    if (neighbor.position.distance_squared_to(p_unit.target_pos) < distance_squared) {
                        // 这里的逻辑：如果我的邻居比我更靠近目的地且它停下了，我也考虑停下
                        stop_unit(p_unit);
                        break;
                    }
                }
            }
        }

        p_unit.path_recheck_timer += (float)p_delta;
        if (p_unit.path_recheck_timer >= 0.8) { // 每0.8秒检查一次
            p_unit.path_recheck_timer = 0.0;

            // 调用新的探测函数
            bool is_traversable = flow_field_manager->is_path_traversable(
                p_unit.position, p_unit.target_pos, p_unit.get_nav_type(), density_limit, false
            );

            if (is_traversable) {
                // 路径畅通，可以走直线
                p_unit.use_direct_path = true;
            }
            else {
                // 路径被墙或“人墙”挡住了！切回流场模式
                p_unit.use_direct_path = false;

                // 确保流场已请求（如果是空军，此处也会触发 NAV_AIR 的流场生成）
                flow_field_manager->create_flow_field(p_unit.target_grid, p_unit.get_nav_type(), false);
            }
        }

        break;
    }
    case CHASING: {
        int nav_type = p_unit.get_nav_type();
        // 1. 获取目标真实位置（可能在建筑内或墙里）
        Vector2 real_target_pos = p_unit.target_pos;
        Vector2i raw_grid = flow_field_manager->world_to_grid(real_target_pos);

        // 2. 找到最近的可达格子
        Vector2i best_grid = flow_field_manager->find_nearest_walkable_cell(raw_grid, nav_type);

        // 3. 计算该格子的物理矩形范围 (AABB)
        Vector2i cell_sz = flow_field_manager->get_cell_size();
        Vector2 cell_min = Vector2(best_grid.x * cell_sz.x, best_grid.y * cell_sz.y);
        Vector2 cell_max = cell_min + Vector2(cell_sz.x, cell_sz.y);

        // 核心修正：将目标点限制在格子的范围内（投影到边缘）
        // 这样 target_pos 就会位于格子离敌人最近的那条边上
        p_unit.target_pos = Vector2(
            Math::clamp(real_target_pos.x, cell_min.x + 1.0f, cell_max.x - 1.0f),
            Math::clamp(real_target_pos.y, cell_min.y + 1.0f, cell_max.y - 1.0f)
        );

        bool target_grid_changed = (best_grid != p_unit.target_grid);
        p_unit.target_grid = best_grid;

        p_unit.path_recheck_timer += (float)p_delta;

        // 4. 定时或在目标剧烈移动时更新路径状态
        if (target_grid_changed || p_unit.path_recheck_timer >= 0.5f) {
            p_unit.path_recheck_timer = 0.0f;

            // 检查从当前位置到目标的“直线”是否畅通
            // 注意：这里探测的是原始 target_pos，增强追击的“侵略性”
            bool is_traversable = flow_field_manager->is_path_traversable(
                p_unit.position, p_unit.target_pos, nav_type, density_limit
            );

            if (is_traversable) {
                // 如果能看到目标且没有厚重的“人墙”或障碍物，直接冲锋
                p_unit.use_direct_path = true;
            }
            else {
                // 如果被挡住了，切换到流场模式
                p_unit.use_direct_path = false;

                // 只有目标网格变了才重算流场，节省性能
                if (target_grid_changed) {
                    flow_field_manager->create_flow_field(p_unit.target_grid, nav_type, false);
                }
            }
        }
        break;
    }
    }
}

void UnitManager::update_velocity(UnitData& p_unit, double p_delta) {
    // --- A. 基础属性准备 ---
    float mass = p_unit.stats->get_mass();
    float stat_accel = p_unit.stats->get_acceleration(); // 现在它代表引擎功率
    float base_max_speed = p_unit.stats->get_move_speed();
    float final_max_speed = base_max_speed;

    // --- B. 速度限制计算 (Arrival & Grouping) ---
    if (p_unit.state != IDLE) {
        // 1. 到达减速 (Arrival)
        if (p_unit.state == MOVING) {
            Vector2 target_with_offset = p_unit.target_pos + p_unit.target_pos_offset;
            float distance_squared = p_unit.position.distance_squared_to(target_with_offset);

            // 定义开始减速的半径（通常是碰撞半径的 3~5 倍）
            float arrival_radius = p_unit.stats->get_collision_radius() * 5.0f;
            float arrival_radius_squared = arrival_radius * arrival_radius;

            if (distance_squared < arrival_radius_squared) {
                // 计算减速因子 (0.0 到 1.0)
                // 为了防止单位完全停不下来，给一个最小速度百分比 (比如 0.2)
                float arrival_modifier = Math::max(0.2f, distance_squared / arrival_radius_squared);
                final_max_speed *= arrival_modifier;
            }
        }
        else if (p_unit.state == CHASING) {
            // 追击模式：
            float dist_to_target = p_unit.position.distance_to(p_unit.target_pos);
            float atk_range = get_unit_attack_range(p_unit.id);
            float stop_dist = atk_range + p_unit.stats->get_collision_radius();

            // 只有当真正快进入“射程”时才减速，而不是快接近“格子边缘”时减速
            // 如果 dist_to_target 很大，即使接近了 target_pos (投影点)，也不要大幅减速
            float slow_down_start = stop_dist * 1.5f;

            if (dist_to_target < slow_down_start) {
                float factor = (dist_to_target - stop_dist) / (slow_down_start - stop_dist);
                final_max_speed *= Math::clamp(factor, 0.5f, 1.0f); // 保持至少 50% 速度冲刺
            }
            // 注意：如果目标不可达，投影点 target_pos 会挡住单位，
            // 物理系统的 move() 函数自然会处理碰撞，不需要这里提前减速到 0
        }


        // 2. 软约束 (Rubber-banding)
        if (p_unit.temp_group_id != -1 && p_unit.state == MOVING && p_unit.stats->get_move_type() != MOVE_AIR) {
            UnitGroup* temp_group = group_manager->get_temp_group(p_unit.temp_group_id);
            if (temp_group) {
                float average_integration = temp_group->average_integration;
                float my_integration = flow_field_manager->get_integration(p_unit.position, p_unit.target_pos, p_unit.get_nav_type());

                // diff > 0 表示我离目标更远（落后了）
                // diff < 0 表示我离目标更近（领先了）
                float diff = my_integration - average_integration;

                // 修正系数：每落后 100 像素，提速 20%
                // 修正范围限制在 80% 到 120% 之间，防止速度过快或停下
                float k = 0.02f; // 调节灵敏度
                float speed_modifier = 1.0f + (diff * k);
                speed_modifier = Math::clamp(speed_modifier, 0.8f, 1.2f);

                final_max_speed *= speed_modifier;
            }
        }
    }

    // --- C. 计算受力 ---

    // 1. 获取各个力分量
    Vector2 flow_vec = get_flow(p_unit) * flow_factor;
    Vector2 sep_force = get_separation(p_unit) * separation_factor;

    // 2. 核心修改：将分离力整合进“动力意图”
    // 现在的 steering_vec 不仅代表“我想去哪”，还代表“我想怎么绕开障碍”
    Vector2 steering_vec = flow_vec + sep_force;

    // 如果没有任何引导力且处于 IDLE，则不产生动力
    if ((p_unit.state == IDLE || p_unit.state == ATTACKING) && steering_vec.length_squared() < 1.0f) {
        steering_vec = Vector2(0, 0);
    }

    Vector2 desired_dir = (steering_vec.length_squared() > 0.001f) ? steering_vec.normalized() : Vector2(0, 0);

    // --- 计算动力 (Propulsion) ---
    Vector2 propulsion_force = Vector2(0, 0);
    if ((p_unit.state != IDLE && p_unit.state != ATTACKING) && (desired_dir.length_squared()) > 0) {
        // 单位当前的物理朝向
        Vector2 forward_vec = Vector2(Math::cos(p_unit.rotation), Math::sin(p_unit.rotation));

        // 计算当前朝向与“修正后期望方向”的对齐程度
        float alignment = Math::max(0.0f, forward_vec.dot(desired_dir));

        // 动力方向修正：
        // 引擎主要向 forward_vec 推，但我们允许一部分动力直接作用于 desired_dir 分量上
        // 这样重型单位在转向时也会有一定的侧向位移，显得更自然
        Vector2 engine_dir = (forward_vec * 0.7f + desired_dir * 0.3f).normalized();

        // 推进力 = 引擎功率 * 朝向修正系数
        // (0.1f 是基础动力，保证单位在原地转身时也能缓慢挪动)
        propulsion_force = engine_dir * (stat_accel * mass * (0.1f + 0.9f * alignment)) * 1000.0f;
    }

    // --- 综合所有力并计算加速度 ---
    // 这里的外部力只包含战斗击退等非转向意图的力
    Vector2 external_physics_force = Vector2(0, 0);
    if (attack_manager) {
        Vector2 combat_force;
        if (attack_manager->try_get_combat_force(p_unit, combat_force)) {
            external_physics_force += combat_force;
        }
    }

    // 总力 = 修正后的动力 + 物理推力 + 修正后的分离力(作为物理补偿)
    Vector2 total_force = propulsion_force + external_physics_force + sep_force * 0.5f;

    // --- D. 计算加速度并应用摩擦力 ---
    // a = F / m
    Vector2 acceleration_vec = total_force / mass;

    // 阻尼/摩擦力 (Friction)
    // 这里的摩擦力与速度成正比，防止单位无限滑行
    // 质量越大，摩擦力（阻力）也越大，这样重型单位停下来也需要更久
    float current_friction = (p_unit.state == IDLE || p_unit.state == ATTACKING) ? friction_factor * 3.0f : friction_factor;
    acceleration_vec -= p_unit.velocity * current_friction;

    // --- E. 更新速度 ---
    p_unit.velocity += acceleration_vec * p_delta;

    // 限制最大速度
    if ((p_unit.velocity).length_squared() > final_max_speed * final_max_speed) {
        p_unit.velocity = p_unit.velocity.normalized() * final_max_speed;
    }

    // --- F. 旋转逻辑 ---
    if (p_unit.state != IDLE) {
        float target_angle = desired_dir.angle();
        float angle_diff = UtilityFunctions::angle_difference(p_unit.rotation, target_angle);

        float turn_speed = p_unit.stats->get_turn_speed();
        float turn_accel = p_unit.stats->get_turn_acceleration();

        // 质量感：重型单位转向加速度也除以质量（可选）
        // turn_accel /= (mass * 0.5f); 

        float target_angular_v = Math::sign(angle_diff) * turn_speed;
        if (Math::abs(angle_diff) < 0.5f) {
            target_angular_v = (angle_diff / 0.5f) * turn_speed;
        }

        p_unit.angular_velocity = UtilityFunctions::move_toward(
            p_unit.angular_velocity, target_angular_v, turn_accel * p_delta);
    }
    else {
        p_unit.angular_velocity = UtilityFunctions::move_toward(p_unit.angular_velocity, 0.0f, p_unit.stats->get_turn_acceleration() * p_delta);
    }

    p_unit.rotation += p_unit.angular_velocity * p_delta;

    // 停止微小移动
    if ((p_unit.state == IDLE || p_unit.state == ATTACKING) && (p_unit.velocity).length_squared() < 1.0f) {
        p_unit.velocity = Vector2(0, 0);
    }
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    if (!flow_field_manager) return; 
    if (p_unit.state == DYING) {
        p_unit.height = UtilityFunctions::max(p_unit.height - 50.0 * p_delta, 0.0f);
        return;
    }

    Vector2 next_pos = p_unit.position + p_unit.velocity * p_delta;
    float radius = (p_unit.stats)->get_collision_radius();
    Vector2i cell_size = flow_field_manager->get_cell_size();
     
    if (1 || p_unit.state != IDLE || p_unit.temp_group_id == -1) {
        std::vector<int> nearby = get_nearby_units(next_pos, radius * 2.0f);
        for (int other_idx : nearby) {
            UnitData& other = units[other_idx];
            if (other.id == p_unit.id) continue;
            if (0 && other.state == IDLE && p_unit.temp_group_id != -1) continue;
            if ((p_unit.stats->move_type == MOVE_AIR) && (other.stats->move_type != MOVE_AIR) ||
                (p_unit.stats->move_type != MOVE_AIR) && (other.stats->move_type == MOVE_AIR)) {
                continue;
            }

            Vector2 to_other = other.position - next_pos;
            float dist_sq = to_other.length_squared();
            float min_dist = radius + (other.stats)->get_collision_radius();

            if (dist_sq < min_dist * min_dist && dist_sq > 0.001f) {
                float dist = Math::sqrt(dist_sq);
                float overlap = min_dist - dist;
                Vector2 resolve_dir = to_other / dist;
                float push_strength = 0.4f;
                Vector2 push_vector = resolve_dir * (overlap * push_strength);

                next_pos -= push_vector;    
            }
        }
    }
    
    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        p_unit.position = next_pos;
        return;
    }

    Vector2i min_grid = flow_field_manager->world_to_grid(next_pos - Vector2(radius, radius));
    Vector2i max_grid = flow_field_manager->world_to_grid(next_pos + Vector2(radius, radius));

    // 静态碰撞 (墙壁)：如果是地面单位，且即将进入 FlowField 标记为 255 (不可通行) 的格子，则将其限制在边缘 AABB 内
    for (int gx = min_grid.x; gx <= max_grid.x; ++gx) {
        for (int gy = min_grid.y; gy <= max_grid.y; ++gy) {
            Vector2i check_grid(gx, gy);

            if (flow_field_manager->get_cost(check_grid, p_unit.get_nav_type()) == 255) {
  
                float rect_left = (float)gx * cell_size.x;
                float rect_top = (float)gy * cell_size.y;
                float rect_right = rect_left + cell_size.x;
                float rect_bottom = rect_top + cell_size.y;

                float closest_x = Math::clamp(next_pos.x, rect_left, rect_right);
                float closest_y = Math::clamp(next_pos.y, rect_top, rect_bottom);
                Vector2 closest_point(closest_x, closest_y);
        
                Vector2 diff = next_pos - closest_point;
                float distance_squared = diff.length_squared();

                if (distance_squared < radius * radius && distance_squared > 0.00001f) {
                    float factor = 0.5;
                    if (p_unit.state == IDLE) {
                        factor = 1.0;
                    }

                    float distance = Math::sqrt(distance_squared);
                    float overlap = radius - distance;
 
                    next_pos += (diff / distance) * overlap * factor;
   
                    Vector2 normal = diff / distance;
                    if (p_unit.velocity.dot(normal) < 0) {
                        p_unit.velocity -= normal * p_unit.velocity.dot(normal);
                    }
                }
                else if (distance_squared <= 0.00001f) {     
                    Vector2 cell_center(rect_left + cell_size.x * 0.5f, rect_top + cell_size.y * 0.5f);
                    Vector2 push_dir = (next_pos - cell_center).normalized();
                    next_pos += push_dir * radius;
                }
            }
        }
    }  
    p_unit.position = next_pos;
}

void UnitManager::update_multimesh_buffer(double p_delta, float p_alpha, SelectionManager* p_selection_manager) {
    if (type_renderers.empty()) return;

    for (auto& pair : type_grouping_cache) {
        pair.second.clear();
    }
    
    for (int i = 0; i < units.size(); ++i) { 
        UnitStats* s_ptr = units[i].stats.ptr();
        type_grouping_cache[s_ptr].push_back(i);
    }
        
    for (auto const& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& indices = type_grouping_cache[s_ptr];
        int count = indices.size();

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
        }
  
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        s_mm->set_instance_count(count);

        if (count == 0) continue;
          
        for (int i = 0; i < count; ++i) {
            int u_idx = indices[i];
            UnitData& unit = units[u_idx];

            // --- 更新单位渲染 ---
            Transform3D xform;
            Vector2 visual_position = UtilityFunctions::lerp(unit.prev_position, unit.next_position, p_alpha);
            float visual_height = UtilityFunctions::lerp(unit.prev_height, unit.next_height, p_alpha);
            float visual_rotation = UtilityFunctions::lerp_angle(unit.prev_rotation, unit.next_rotation, p_alpha);

            float fake_depth_offset = visual_position.y * 0.0001f;
            Vector3 pos_3d = Vector3(visual_position.x, visual_height + fake_depth_offset, visual_position.y - visual_height);
            xform.origin = pos_3d; 
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (visual_rotation + Math_PI / 2.0f));

            mm->set_instance_transform(i, xform);

            int frames = (unit.state == MOVING) ? s_ptr->get_move_frames() : s_ptr->get_idle_frames();
            int row = (unit.state == MOVING) ? s_ptr->get_move_row() : s_ptr->get_idle_row();
            float duration = (float)frames / s_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(unit.anim_time, duration) * s_ptr->get_anim_fps());

            float modulate = 1.0;
            if (p_selection_manager->is_unit_selected(unit.id)) {
                modulate = 1.5f;
            }
            else if (p_selection_manager->is_unit_hovered(unit.id)) {
                modulate = 1.2f;
            }

            if (unit.state == DYING) {
                modulate = 1.0f - (unit.current_dying_time / unit.stats->get_dying_time()) * 0.5f;
            }

            mm->set_instance_custom_data(i, Color(frame_idx, row, modulate, 0));
            mm->set_instance_color(i, get_team_color(unit.team_id));


            // ---  设置粒子效果  ---
            // --- 计算位移 ---
            float move_dist = visual_position.distance_to(unit.last_visual_pos);
            unit.last_visual_pos = visual_position;

            if (unit.state != IDLE && unit.height < AIR_HEIGHT_THRESHOLD) {
                unit.dust_accumulator += move_dist;

                float radius = unit.stats->get_collision_radius();

                // 1. 确定发射阈值
                float threshold = unit.stats->emit_threshold_override > 0 ? unit.stats->emit_threshold_override :
                    ((unit.get_nav_type() == NAV_SEA) ? radius * 0.5f : radius * 1.2f);

                if (unit.dust_accumulator >= threshold) {
                    unit.dust_accumulator = 0.0f;
                    unit.emit_count += 1;

                    Vector2 dir = Vector2(Math::cos(visual_rotation), Math::sin(visual_rotation));
                    Vector2 side = dir.rotated(Math_PI / 2.0);

                    // 这是一个 Lambda 函数，用来处理“覆盖或默认”逻辑
                    auto emit_logic = [&](String type, Vector2 default_local_pos, Vector3 vel, float def_scale, float def_life, float y_offset, bool is_ripple, bool check_freq = false) {
                        if (check_freq && (unit.emit_count % 2 == 0)) return; // 频率过滤（波纹用）

                        // 查找配置中是否有这个类型的点
                        std::vector<EffectPoint*> custom_points;
                        for (auto& ep : unit.stats->effect_points) {
                            if (ep.effect_type == type) custom_points.push_back(&ep);
                        }

                        if (!custom_points.empty()) {
                            // 情况 A: 使用配置的点
                            for (auto* cp : custom_points) {
                                Vector2 world_pos_2d = visual_position + (dir * cp->local_position.y) + (side * cp->local_position.x);
                                Vector3 final_pos = Vector3(world_pos_2d.x, visual_height + y_offset, world_pos_2d.y);

                                float final_scale = (cp->scale_override > 0) ? cp->scale_override : def_scale;
                                float final_life = (cp->life_override > 0) ? cp->life_override : def_life;

                                effect_manager->emit_particle(type, final_pos, vel, final_scale, final_life, cp->is_ripple);
                            }
                        }
                        else {
                            // 情况 B: 使用原先硬编码的默认点（仅当不是陆地单位在海上/反之时）
                            Vector2 world_pos_2d = visual_position + (dir * default_local_pos.y) + (side * default_local_pos.x);
                            Vector3 final_pos = Vector3(world_pos_2d.x, visual_height + y_offset, world_pos_2d.y);
                            effect_manager->emit_particle(type, final_pos, vel, def_scale, def_life, is_ripple);
                        }
                        };

                    // --- 执行具体的粒子逻辑 ---

                    if (unit.get_nav_type() == NAV_SEA) {
                        float base_s = (unit.stats->particle_scale_override > 0) ? unit.stats->particle_scale_override : (radius / 32.0f);

                        // 1. WaterFoam: 硬编码参数是 vel=0, life=2.0, y_offset=-0.04
                        emit_logic("WaterFoam", Vector2(0, -radius), Vector3(0, 0, 0), std::min(2.0f * base_s, 2.5f), 2.0f, -0.04f, false);

                        // 2. WaterRipple: 硬编码参数是 vel=0, life=1.5, y_offset=-0.05, 频率%2
                        emit_logic("WaterRipple", Vector2(0, radius * 0.8f), Vector3(0, 0, 0), 1.6f * base_s, 1.5f, -0.05f, true, true);
                    }
                    else if (flow_field_manager->get_cost(flow_field_manager->world_to_grid(unit.position), NAV_LAND) < 255) {
                        // 3. Dust: 硬编码参数比较复杂（随机速度）
                        float base_s = (unit.stats->particle_scale_override > 0) ? unit.stats->particle_scale_override : (radius / 48.0f);
                        Vector3 dust_vel = Vector3(-dir.x * 40.0f + UtilityFunctions::randf_range(-10, 10), 0.0f, -dir.y * 40.0f + UtilityFunctions::randf_range(-10, 10));

                        emit_logic("Dust", Vector2(0, -radius * 1.1f), dust_vel, 5.0f * base_s, 0.75f, -0.05f, false);
                    }
                }
            }
            else {
                unit.dust_accumulator = 0.0f;
            }


            // --- 更新影子渲染 ---
            Transform3D shadow_xform;

            float shadow_offset_x = 4.0f;  
            float shadow_offset_z = 4.0f;
            shadow_xform.origin = Vector3(visual_position.x + shadow_offset_x,
                visual_height + fake_depth_offset - 0.1f,
                visual_position.y + shadow_offset_z);
            shadow_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            shadow_xform.basis = (shadow_xform.basis).rotated(Vector3(0, -1, 0), (visual_rotation + Math_PI / 2.0f));

            s_mm->set_instance_transform(i, shadow_xform);
            s_mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            unit.anim_time += p_delta;
        }
    }

    // --- 更新全局血条 ---
    int total_units = units.size();
    Ref<MultiMesh> hp_mm = global_hp_bar_renderer->get_multimesh();

    if (hp_mm->get_instance_count() != total_units) {
        hp_mm->set_instance_count(total_units);
    }

    for (int i = 0; i < total_units; ++i) {
        UnitData& unit = units[i];

        // 1. 获取基础视觉数据
        Vector2 visual_pos = UtilityFunctions::lerp(unit.prev_position, unit.next_position, p_alpha);
        float visual_height = UtilityFunctions::lerp(unit.prev_height, unit.next_height, p_alpha);

        // 2. 计算血条位置和大小
        float offset_y = unit.stats->get_collision_radius() * 1.5f; // 每个兵种高度不同
        float bar_width = unit.stats->get_collision_radius() * 1.7f;

        Transform3D xform;
        // 这里的 Y 坐标加上了 offset_y
        Vector3 pos_3d = Vector3(visual_pos.x,
            visual_height + 10.0f,
            visual_pos.y - visual_height + offset_y);
        xform.origin = pos_3d;

        xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);
        // 根据兵种设定的宽度进行缩放 (高度固定比如 4.0)
        xform.basis = xform.basis.scaled(Vector3(bar_width, 4.0, 4.0));

        hp_mm->set_instance_transform(i, xform);

        // 3. 计算血量百分比
        float hp_ratio = (float)unit.current_health / (float)unit.stats->get_health_max();

        // 4. 控制显示逻辑 (比如：受伤显示，或者选中显示)
        bool is_selected = p_selection_manager->is_unit_selected(unit.id);
        bool is_damaged = hp_ratio < 0.99f;

        // 如果不需要显示，直接把缩放设为 0 或者在 Shader 里 discard
        if (!(is_selected || is_damaged)) {
            hp_mm->set_instance_transform(i, Transform3D().scaled(Vector3(0, 0, 0)));
            continue;
        }

        // 5. 传递数据给 Shader
        // Custom Data: X = 比例, Y = 状态
        hp_mm->set_instance_custom_data(i, Color(hp_ratio, 0.0, 0.0, 0.0));

        // Color: 设置团队颜色（友绿敌红）
        Color team_color = Color(1.0, 0.0, 0.0);
        hp_mm->set_instance_color(i, team_color);
    }

    // 更新小地图点
    Ref<MultiMesh> mmm = minimap_dot_renderer->get_multimesh();
    if (mmm->get_instance_count() != units.size()) {
        mmm->set_instance_count(units.size());
    }

    for (int i = 0; i < units.size(); ++i) {
        UnitData& unit = units[i];

        Transform3D xform;
        // 关键点：将单位点放在 Y = -50，这样它会被 Y = 0 的迷雾遮挡
        Vector2 pos = unit.position;
        xform.origin = Vector3(pos.x, -50.0f + unit.next_height, pos.y);
        xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

        mmm->set_instance_transform(i, xform);
        // 设置颜色为队伍色
        mmm->set_instance_color(i, get_team_color(unit.team_id));

        // 如果单位死亡，将缩放设为0
        if (unit.state == DYING) {
            mmm->set_instance_transform(i, Transform3D().scaled(Vector3(0, 0, 0)));
        }
    }
}

Vector2 UnitManager::get_unit_position(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    
    if (it != id_to_index.end()) {
        return units[it->second].position;
    }

    return Vector2(0, 0);
}

int UnitManager::get_unit_state(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);

    if (it != id_to_index.end()) {
        return (int)(units[it->second].state);
    }

    return (int)(IDLE);
}

Ref<UnitStats> godot::UnitManager::get_unit_stats_by_type(String p_type_name)
{
    if (unit_types_cache.has(p_type_name)) {
        return unit_types_cache[p_type_name];
    }
    return nullptr;
}

void UnitManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void UnitManager::set_group_manager(Node* p_node) {
    group_manager = Object::cast_to<GroupManager>(p_node);
}

void UnitManager::set_fog_manager(Node* p_node) {
    fog_manager = Object::cast_to<FogManager>(p_node);
}

void UnitManager::set_weapon_manager(Node* p_node) {
    weapon_manager = Object::cast_to<WeaponManager>(p_node);
}

void UnitManager::set_effect_manager(Node* p_node) {
    effect_manager = Object::cast_to<EffectManager>(p_node);
}

void UnitManager::_internal_register_stats(Ref<UnitStats> p_stats) {
    String p_name = p_stats->unit_name;
    UnitStats* stats_ptr = p_stats.ptr();

    // 存入缓存
    unit_types_cache[p_name] = p_stats;

    // 如果该类型的渲染器已存在则跳过（避免重复注册）
    if (type_renderers.find(stats_ptr) != type_renderers.end()) return;

    // --- 以下是原 register_unit_type 里的渲染初始化代码 ---

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

    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(p_stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(p_stats->get_h_frames(), p_stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (unit_shader.is_null()) {
        unit_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shader.gdshader");
    }
    mat->set_shader(unit_shader);
    mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    mat->set_shader_parameter("albedo_texture", tex);
    // 传入实时视野贴图
    mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
    // 传入地图尺寸
    mat->set_shader_parameter("map_size", fog_manager->get_map_size());
    // 传入地图位置
    mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());

    mmi->set_material_override(mat);

    type_renderers[stats_ptr] = mmi;

    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadows");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true);

    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(p_stats->get_h_frames(), p_stats->get_v_frames());
        s_qmesh->set_size(frame_size);
    }
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    Ref<ShaderMaterial> s_mat;
    s_mat.instantiate();
    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shadow.gdshader");
    }
    s_mat->set_shader(shadow_shader);
    s_mat->set_shader_parameter("albedo_texture", tex);
    s_mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    s_mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    // 传入实时视野贴图
    s_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
    // 传入地图尺寸
    s_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
    // 传入地图位置
    s_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());

    s_mmi->set_material_override(s_mat);

    shadow_renderers[stats_ptr] = s_mmi;
}


void UnitManager::register_unit_type(String p_name, String p_path) {
    Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(p_path, weapon_manager);
    if (stats.is_null()) return;

    // 如果文件中没写名字，则使用手动输入的 p_name
    if (stats->unit_name.is_empty()) {
        stats->unit_name = p_name;
    }

    _internal_register_stats(stats);
}

void UnitManager::register_units_from_dir(String p_dir_path) {
    Ref<DirAccess> dir = DirAccess::open(p_dir_path);
    if (dir.is_null()) {
        UtilityFunctions::print("[UnitManager] Error: Cannot open directory: ", p_dir_path);
        return;
    }

    dir->list_dir_begin();
    String file_name = dir->get_next();

    while (file_name != "") {
        // 只处理 .txt 配置文件
        if (!dir->current_is_dir() && file_name.ends_with(".txt")) {
            String full_path = p_dir_path.path_join(file_name);
            Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(full_path, weapon_manager);

            if (stats.is_valid() && !stats->unit_name.is_empty()) {
                _internal_register_stats(stats);
                UtilityFunctions::print("[UnitManager] Auto registered unit: ", stats->unit_name, " from ", file_name);
            }
            else {
                UtilityFunctions::print("[UnitManager] Warning: Failed to load unit from ", file_name, " (Missing unit_name?)");
            }
        }
        file_name = dir->get_next();
    }
}

int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id, int p_forced_id) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name], p_team_id, p_forced_id);
    }
    return -1;
}

void UnitManager::set_control_group(int p_index, const std::vector<int>& p_unit_ids) {
    if (p_index < 0 || p_index >= group_manager->MAX_CONTROL_GROUPS) return;

    for (int old_uid : group_manager->control_groups[p_index]) {
        int index = -1;
        auto it = id_to_index.find(old_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index]; 
        for (int i = 0; i < data.control_group_count; ++i) {
            if (data.control_group_indices[i] == p_index) {
                data.control_group_indices[i] = data.control_group_indices[data.control_group_count - 1];
                data.control_group_count--;
                break;
            }
        }
    }
   
    group_manager->control_groups[p_index] = p_unit_ids;

    for (int new_uid : p_unit_ids) { 
        int index = -1;
        auto it = id_to_index.find(new_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        if (data.control_group_count < 3) { 
            data.control_group_indices[data.control_group_count++] = p_index;
        }
    }
}

int UnitManager::get_unit_index_by_id(int p_id) {
    auto it = id_to_index.find(p_id);
    if (it != id_to_index.end()) {
        return it->second;
    }
    return -1;
}

void UnitManager::set_attack_manager(Node* p_node) {
    attack_manager = Object::cast_to<AttackManager>(p_node);
    if (attack_manager) {
        attack_manager->setup(this); 
    }
}

float UnitManager::get_unit_aggro_range(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) { 
        return units[it->second].stats->get_aggro_range();
    }
    return 0.0f;
}

float UnitManager::get_unit_attack_range(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) {
        const UnitData& unit = units[it->second];

        // 确保单位 stats 有效，并且该单位拥有至少一把武器
        if (unit.stats.is_valid() && !unit.stats->weapons.empty()) {
            float max_range = 0.0f;
            // 遍历单位挂载的所有武器，找出最大射程
            for (const auto& weapon : unit.stats->weapons) {
                if (weapon.attack_range > max_range) {
                    max_range = weapon.attack_range;
                }
            }
            return max_range;
        }
    }
    return 0.0f;
}

void UnitManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type", "team_id"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos", "team_id", "force_id"), &UnitManager::spawn_unit_by_type,
        DEFVAL(0),  // 对应 p_team_id 的默认值
        DEFVAL(-1)  // 对应 p_forced_id 的默认值
    );
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("command_units_to_patrol", "unit_ids", "waypoints"), &UnitManager::command_units_to_patrol);
    ClassDB::bind_method(D_METHOD("command_units_to_attack_target", "unit_ids", "target_id", "target_is_building", "building_manager"), &UnitManager::command_units_to_attack_target);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("get_unit_stats", "unit_id"), &UnitManager::get_unit_stats);
    ClassDB::bind_method(D_METHOD("get_unit_stats_by_type", "unit_type"), &UnitManager::get_unit_stats_by_type);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &UnitManager::set_group_manager);
    ClassDB::bind_method(D_METHOD("set_attack_manager", "node"), &UnitManager::set_attack_manager);
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
    ClassDB::bind_method(D_METHOD("register_units_from_dir", "dir_path"), &UnitManager::register_units_from_dir);
    ClassDB::bind_method(D_METHOD("get_unit_aggro_range", "unit_id"), &UnitManager::get_unit_aggro_range);
    ClassDB::bind_method(D_METHOD("get_unit_attack_range", "unit_id"), &UnitManager::get_unit_attack_range);
    

    ClassDB::bind_method(D_METHOD("get_flow_factor"), &UnitManager::get_flow_factor);
    ClassDB::bind_method(D_METHOD("set_flow_factor", "p_val"), &UnitManager::set_flow_factor);

    ClassDB::bind_method(D_METHOD("get_separation_factor"), &UnitManager::get_separation_factor);
    ClassDB::bind_method(D_METHOD("set_separation_factor", "p_val"), &UnitManager::set_separation_factor);

    ClassDB::bind_method(D_METHOD("get_separation_limit"), &UnitManager::get_separation_limit);
    ClassDB::bind_method(D_METHOD("set_separation_limit", "p_val"), &UnitManager::set_separation_limit);

    ClassDB::bind_method(D_METHOD("get_separation_radius_factor"), &UnitManager::get_separation_radius_factor);
    ClassDB::bind_method(D_METHOD("set_separation_radius_factor", "p_val"), &UnitManager::set_separation_radius_factor);

    ClassDB::bind_method(D_METHOD("get_lateral_separation_factor"), &UnitManager::get_lateral_separation_factor);
    ClassDB::bind_method(D_METHOD("set_lateral_separation_factor", "p_val"), &UnitManager::set_lateral_separation_factor);

    ClassDB::bind_method(D_METHOD("get_friction_factor"), &UnitManager::get_friction_factor);
    ClassDB::bind_method(D_METHOD("set_friction_factor", "p_val"), &UnitManager::set_friction_factor);

    ClassDB::bind_method(D_METHOD("get_force_threshold_squared"), &UnitManager::get_force_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_force_threshold_squared", "p_val"), &UnitManager::set_force_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_velocity_threshold_squared"), &UnitManager::get_velocity_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_velocity_threshold_squared", "p_val"), &UnitManager::set_velocity_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_desired_integration"), &UnitManager::get_desired_integration);
    ClassDB::bind_method(D_METHOD("set_desired_integration", "p_val"), &UnitManager::set_desired_integration);

    ADD_GROUP("Force Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flow_factor"), "set_flow_factor", "get_flow_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_factor"), "set_separation_factor", "get_separation_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_limit"), "set_separation_limit", "get_separation_limit");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_radius_factor"), "set_separation_radius_factor", "get_separation_radius_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lateral_separation_factor"), "set_lateral_separation_factor", "get_lateral_separation_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction_factor"), "set_friction_factor", "get_friction_factor");

    ADD_GROUP("Threshold Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "force_threshold_squared"), "set_force_threshold_squared", "get_force_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity_threshold_squared"), "set_velocity_threshold_squared", "get_velocity_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "desired_integration"), "set_desired_integration", "get_desired_integration");

    ADD_SIGNAL(MethodInfo("despawn_unit_requested", PropertyInfo(Variant::INT, "unit_id")));
}




