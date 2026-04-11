#include "weapon_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

#include "unit_manager.h"
#include "building_manager.h"
#include "selection_manager.h"
#include "fog_manager.h"

using namespace godot;

WeaponManager* WeaponManager::singleton = nullptr;

WeaponManager::WeaponManager() {
    if (singleton == nullptr) {
        singleton = this;
    }

    // 提前加载着色器资源
    if (unit_shader.is_null()) {
        unit_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shader.gdshader");
    }
    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shadow.gdshader");
    }
    if (ghost_shader.is_null()) {
        ghost_shader = ResourceLoader::get_singleton()->load("res://shader/ghost_building_shader.gdshader");
    }
}

WeaponManager::~WeaponManager() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

WeaponManager* WeaponManager::get_singleton() {
    return singleton;
}

void WeaponManager::setup_system(Node* p_fog_manager) {
    fog_manager = Object::cast_to<FogManager>(p_fog_manager);
    if (!fog_manager) return;

    // 针对已经注册的武器，更新迷雾 Shader 参数
    for (auto& pair : weapon_renderers) {
        Ref<ShaderMaterial> mat = pair.second->get_material_override();
        if (mat.is_valid()) {
            mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
            mat->set_shader_parameter("map_size", fog_manager->get_map_size());
            mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
        }
    }
    for (auto& pair : weapon_shadow_renderers) {
        Ref<ShaderMaterial> mat = pair.second->get_material_override();
        if (mat.is_valid()) {
            mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
            mat->set_shader_parameter("map_size", fog_manager->get_map_size());
            mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
        }
    }
    for (auto& pair : weapon_ghost_renderers) {
        Ref<ShaderMaterial> mat = pair.second->get_material_override();
        if (mat.is_valid()) {
            mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
            mat->set_shader_parameter("tex_history", fog_manager->get_history_texture());
            mat->set_shader_parameter("map_size", fog_manager->get_map_size());
            mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
        }
    }
}

void WeaponManager::update_multimesh_buffer(double p_delta, float p_alpha, UnitManager* p_unit_mgr, BuildingManager* p_bld_mgr, SelectionManager* p_sel_mgr) {
    if (weapon_renderers.empty()) return;

    for (auto& pair : weapon_grouping_cache) pair.second.clear();
    for (auto& pair : building_weapon_grouping_cache) pair.second.clear();
    for (auto& pair : ghost_weapon_grouping_cache) pair.second.clear();

    // 1. 扫描单位
    if (p_unit_mgr) {
        for (int i = 0; i < p_unit_mgr->units.size(); ++i) {
            UnitData& unit = p_unit_mgr->units[i];
            for (int w = 0; w < unit.weapons.size(); ++w) {
                WeaponStats* w_ptr = unit.weapons[w].stats.ptr();
                if (w_ptr) weapon_grouping_cache[w_ptr].push_back({ i, w });
            }
        }
    }

    // 2. 扫描建筑与残影
    if (p_bld_mgr) {
        for (auto const& [b_id, b_data] : p_bld_mgr->buildings) {
            if (b_data.state == BuildingState::BUILDING) { continue; }
            for (int w = 0; w < b_data.weapons.size(); ++w) {
                WeaponStats* w_ptr = b_data.weapons[w].stats.ptr();
                if (w_ptr) building_weapon_grouping_cache[w_ptr].push_back({ b_id, w });
            }
        }
        for (auto const& [b_id, g_data] : p_bld_mgr->ghost_buildings) {
            if (g_data.state == BuildingState::BUILDING) { continue; }
            for (int w = 0; w < g_data.weapons.size(); ++w) {
                WeaponStats* w_ptr = g_data.weapons[w].stats.ptr();
                if (w_ptr) ghost_weapon_grouping_cache[w_ptr].push_back({ b_id, w });
            }
        }
    }

    // 3. 执行主体与阴影渲染
    for (auto const& [w_ptr, mmi] : weapon_renderers) {
        const auto& u_indices = weapon_grouping_cache[w_ptr];
        const auto& b_indices = building_weapon_grouping_cache[w_ptr];
        int u_count = u_indices.size();
        int b_count = b_indices.size();
        int total_count = u_count + b_count;

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != total_count) mm->set_instance_count(total_count);

        MultiMeshInstance3D* s_mmi = weapon_shadow_renderers[w_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        if (s_mm->get_instance_count() != total_count) s_mm->set_instance_count(total_count);

        if (total_count == 0) continue;

        // A. 渲染单位的武器
        for (int i = 0; i < u_count; ++i) {
            int u_idx = u_indices[i].first;
            int w_idx = u_indices[i].second;
            UnitData& unit = p_unit_mgr->units[u_idx];
            WeaponData& weapon = unit.weapons[w_idx];
            weapon.anim_time += p_delta; // 【必须增加：驱动客户端动画】

            Vector2 u_pos = UtilityFunctions::lerp(unit.prev_position, unit.next_position, p_alpha);
            float u_h = UtilityFunctions::lerp(unit.prev_height, unit.next_height, p_alpha);
            float u_rot = UtilityFunctions::lerp_angle(unit.prev_rotation, unit.next_rotation, p_alpha);
            float w_rot = UtilityFunctions::lerp_angle(weapon.prev_rotation, weapon.next_rotation, p_alpha);

            Vector2 w_pos = u_pos + weapon.local_position.rotated(u_rot);

            // 1. 获取旋转中心偏移 (从像素空间转为世界空间，注意QuadMesh是1:1像素大小)
            Vector2 rot_offset = weapon.stats->get_rotation_center();

            // 2. 修改 Transform 计算逻辑：
            // 我们需要先将 QuadMesh 移动，使得旋转中心对齐坐标原点，旋转后再移动回来
            Transform3D xform;
            // 基础高度和深度排序偏移
            float fd = u_pos.y * 0.0001f;
            Vector3 final_origin = Vector3(w_pos.x, u_h + fd + 0.0004f, w_pos.y - u_h);

            xform.origin = final_origin;
            // 旋转矩阵
            Basis rot_basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0).rotated(Vector3(0, -1, 0), w_rot + Math_PI / 2.0f);
            xform.basis = rot_basis;

            // 关键：应用旋转中心偏移。偏移量需要跟随旋转矩阵旋转。
            // rot_offset.x 对应模型右方，rot_offset.y 对应模型下方（在3D平面中对应Z轴）
            Vector3 offset_3d = rot_basis.xform(Vector3(-rot_offset.x, rot_offset.y, 0.0));
            xform.origin += offset_3d;

            mm->set_instance_transform(i, xform);

            int fr = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_frames() : w_ptr->get_idle_frames();
            int row = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_row() : w_ptr->get_idle_row();
            float dur = (float)fr / w_ptr->get_anim_fps();
            int f_idx = (int)(Math::fmod(weapon.anim_time, dur) * w_ptr->get_anim_fps());
            float mod = 1.0;
            if (p_sel_mgr->is_unit_selected(unit.id)) {
                mod = 1.5f;
            }
            else if (p_sel_mgr->is_unit_hovered(unit.id)) {
                mod = 1.2f;
            }

            if (unit.state == DYING) {
                mod = 1.0f - (unit.current_dying_time / unit.stats->get_dying_time()) * 0.5f;
            }

            mm->set_instance_custom_data(i, Color(f_idx, row, mod, 0));
            mm->set_instance_color(i, get_team_color(unit.team_id));

            Transform3D s_xform = xform;
            s_xform.origin = xform.origin + Vector3(4.0f, - 0.0002f, 4.0f);
            s_mm->set_instance_transform(i, s_xform);
            s_mm->set_instance_custom_data(i, Color(f_idx, row, 0, 0));
        }

        // B. 渲染建筑的武器
        for (int i = 0; i < b_count; ++i) {
            int mm_idx = u_count + i;
            int b_id = b_indices[i].first;
            int w_idx = b_indices[i].second;

            BuildingData& b = p_bld_mgr->buildings[b_id];
            WeaponData& weapon = b.weapons[w_idx];
            weapon.anim_time += p_delta; // 【必须增加：驱动客户端动画】

            Vector2 cell_sz = Vector2(p_bld_mgr->get_cell_size());
            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;

            float w_rot = UtilityFunctions::lerp_angle(weapon.prev_rotation, weapon.next_rotation, p_alpha);
            Vector2 w_pos = center + weapon.local_position; // 建筑不旋转，直接偏移

            // 1. 获取旋转中心偏移 (从像素空间转为世界空间，注意QuadMesh是1:1像素大小)
            Vector2 rot_offset = weapon.stats->get_rotation_center();

            // 2. 修改 Transform 计算逻辑：
            // 我们需要先将 QuadMesh 移动，使得旋转中心对齐坐标原点，旋转后再移动回来
            Transform3D xform;
            // 基础高度和深度排序偏移
            float fd = center.y * 0.0001f;
            Vector3 final_origin = Vector3(w_pos.x, fd + 0.0004f, w_pos.y);

            xform.origin = final_origin;
            // 旋转矩阵
            Basis rot_basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0).rotated(Vector3(0, -1, 0), w_rot + Math_PI / 2.0f);
            xform.basis = rot_basis;

            // 关键：应用旋转中心偏移。偏移量需要跟随旋转矩阵旋转。
            // rot_offset.x 对应模型右方，rot_offset.y 对应模型下方（在3D平面中对应Z轴）
            Vector3 offset_3d = rot_basis.xform(Vector3(-rot_offset.x, rot_offset.y, 0.0));
            xform.origin += offset_3d;

            mm->set_instance_transform(mm_idx, xform);

            int fr = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_frames() : w_ptr->get_idle_frames();
            int row = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_row() : w_ptr->get_idle_row();
            float dur = (float)fr / w_ptr->get_anim_fps();
            int f_idx = (int)(Math::fmod(weapon.anim_time, dur) * w_ptr->get_anim_fps());
            float mod = 1.0;
            if (p_sel_mgr->is_building_selected(b.id)) {
                mod = 1.5f;
            }
            else if (p_sel_mgr->is_building_hovered(b.id)) {
                mod = 1.2f;
            }

            if (b.state == BuildingState::DYING) {
                mod = 1.0f - (b.current_dying_time / b.stats->get_dying_time()) * 0.5f;
            }

            mm->set_instance_custom_data(mm_idx, Color(f_idx, row, mod, 0));
            mm->set_instance_color(mm_idx, get_team_color(b.team_id));

            Transform3D s_xform = xform;
            s_xform.origin = xform.origin + Vector3(4.0f, - 0.0002f, 4.0f);
            s_mm->set_instance_transform(mm_idx, s_xform);
            s_mm->set_instance_custom_data(mm_idx, Color(f_idx, row, 0, 0));
        }
    }

    // 4. 执行残影渲染
    for (auto const& [w_ptr, g_mmi] : weapon_ghost_renderers) {
        const auto& g_indices = ghost_weapon_grouping_cache[w_ptr];
        int count = g_indices.size();

        Ref<MultiMesh> g_mm = g_mmi->get_multimesh();
        if (g_mm->get_instance_count() != count) g_mm->set_instance_count(count);

        for (int i = 0; i < count; ++i) {
            int b_id = g_indices[i].first;
            int w_idx = g_indices[i].second;

            const GhostBuildingData& g = p_bld_mgr->ghost_buildings[b_id];
            const WeaponData& weapon = g.weapons[w_idx];

            Vector2 cell_sz = Vector2(p_bld_mgr->get_cell_size());
            Vector2 fp_size = Vector2(g.stats->get_footprint()) * cell_sz;
            Vector2 center = Vector2(g.grid_pos) * cell_sz + fp_size * 0.5f;

            Vector2 w_pos = center + weapon.local_position;

            // 1. 获取旋转中心偏移 (从像素空间转为世界空间，注意QuadMesh是1:1像素大小)
            Vector2 rot_offset = weapon.stats->get_rotation_center();

            // 2. 修改 Transform 计算逻辑：
            // 我们需要先将 QuadMesh 移动，使得旋转中心对齐坐标原点，旋转后再移动回来
            Transform3D xform;
            // 基础高度和深度排序偏移
            float fd = center.y * 0.0001f;
            Vector3 final_origin = Vector3(w_pos.x, fd + 0.0001f, w_pos.y);

            xform.origin = final_origin;
            // 旋转矩阵
            Basis rot_basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0).rotated(Vector3(0, -1, 0), Math_PI / 2.0f);
            xform.basis = rot_basis;

            // 关键：应用旋转中心偏移。偏移量需要跟随旋转矩阵旋转。
            // rot_offset.x 对应模型右方，rot_offset.y 对应模型下方（在3D平面中对应Z轴）
            Vector3 offset_3d = rot_basis.xform(Vector3(-rot_offset.x, rot_offset.y, 0.0));
            xform.origin += offset_3d;

            g_mm->set_instance_transform(i, xform);
            g_mm->set_instance_color(i, get_team_color(g.team_id));
        }
    }
}

void WeaponManager::register_weapon(String p_name, String p_path) {
    Ref<WeaponStats> stats = WeaponLoader::load_stats_from_txt(p_path);
    if (stats.is_valid()) {
        if (stats->get_weapon_name() == "new_weapon" && !p_name.is_empty()) {
            stats->set_weapon_name(p_name);
        }
        weapons_cache[stats->get_weapon_name()] = stats;

        // --- 实例化渲染组件 ---
        WeaponStats* stats_ptr = stats.ptr();
        if (weapon_renderers.find(stats_ptr) == weapon_renderers.end()) {
            // 1. 本题 MultiMesh
            MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
            mmi->set_name(stats->get_weapon_name() + "_Renderer");
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

            Ref<ShaderMaterial> mat;
            mat.instantiate();
            mat->set_shader(unit_shader);
            mat->set_shader_parameter("h_frames", stats->get_h_frames());
            mat->set_shader_parameter("v_frames", stats->get_v_frames());
            mat->set_shader_parameter("albedo_texture", tex);
            if (fog_manager) {
                mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
            mmi->set_material_override(mat);
            weapon_renderers[stats_ptr] = mmi;

            // 2. 阴影 MultiMesh
            MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
            s_mmi->set_name(stats->get_weapon_name() + "_Shadow");
            add_child(s_mmi);

            Ref<MultiMesh> s_mm;
            s_mm.instantiate();
            s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
            s_mm->set_use_custom_data(true);

            Ref<QuadMesh> s_qmesh;
            s_qmesh.instantiate();
            if (tex.is_valid()) {
                Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
                s_qmesh->set_size(frame_size);
            }
            s_mm->set_mesh(s_qmesh);
            s_mmi->set_multimesh(s_mm);

            Ref<ShaderMaterial> s_mat;
            s_mat.instantiate();
            s_mat->set_shader(shadow_shader);
            s_mat->set_shader_parameter("h_frames", stats->get_h_frames());
            s_mat->set_shader_parameter("v_frames", stats->get_v_frames());
            s_mat->set_shader_parameter("albedo_texture", tex);
            if (fog_manager) {
                s_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                s_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                s_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
            s_mmi->set_material_override(s_mat);
            weapon_shadow_renderers[stats_ptr] = s_mmi;

            // 3. 残影 MultiMesh
            MultiMeshInstance3D* g_mmi = memnew(MultiMeshInstance3D);
            g_mmi->set_name(stats->get_weapon_name() + "_Ghost");
            add_child(g_mmi);

            Ref<MultiMesh> g_mm;
            g_mm.instantiate();
            g_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
            g_mm->set_use_colors(true);
            g_mm->set_use_custom_data(false); // 残影只用首帧，禁用以节省带宽

            Ref<QuadMesh> g_qmesh;
            g_qmesh.instantiate();
            if (tex.is_valid()) {
                Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
                g_qmesh->set_size(frame_size);
            }
            g_mm->set_mesh(g_qmesh);
            g_mmi->set_multimesh(g_mm);

            Ref<ShaderMaterial> g_mat;
            g_mat.instantiate();
            g_mat->set_shader(ghost_shader);
            g_mat->set_shader_parameter("h_frames", stats->get_h_frames());
            g_mat->set_shader_parameter("v_frames", stats->get_v_frames());
            g_mat->set_shader_parameter("albedo_texture", tex);
            if (fog_manager) {
                g_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                g_mat->set_shader_parameter("tex_history", fog_manager->get_history_texture());
                g_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                g_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
            g_mmi->set_material_override(g_mat);
            weapon_ghost_renderers[stats_ptr] = g_mmi;
        }

        UtilityFunctions::print("[WeaponManager] Registered weapon: ", stats->get_weapon_name());
    }
}

void WeaponManager::register_weapons_from_dir(String p_dir_path) {
    Ref<DirAccess> dir = DirAccess::open(p_dir_path);
    if (dir.is_null()) {
        UtilityFunctions::print("[WeaponManager] Error: Cannot open directory: ", p_dir_path);
        return;
    }

    dir->list_dir_begin();
    String file_name = dir->get_next();

    while (file_name != "") {
        // 遍历指定文件夹下所有的 txt 文件
        if (!dir->current_is_dir() && file_name.ends_with(".txt")) {
            register_weapon("", p_dir_path.path_join(file_name));
        }
        file_name = dir->get_next();
    }
}

Ref<WeaponStats> WeaponManager::get_weapon(String p_name) {
    if (weapons_cache.has(p_name)) {
        return weapons_cache[p_name];
    }
    return nullptr;
}

void WeaponManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_weapon", "name", "path"), &WeaponManager::register_weapon);
    ClassDB::bind_method(D_METHOD("register_weapons_from_dir", "dir_path"), &WeaponManager::register_weapons_from_dir);
}