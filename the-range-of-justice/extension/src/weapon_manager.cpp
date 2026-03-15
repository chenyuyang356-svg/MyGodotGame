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
}

void WeaponManager::update_multimesh_buffer(double p_delta, float p_alpha, UnitManager* p_unit_mgr, BuildingManager* p_bld_mgr, SelectionManager* p_sel_mgr) {
    if (weapon_renderers.empty() || !p_unit_mgr) return;

    for (auto& pair : weapon_grouping_cache) {
        pair.second.clear();
    }

    // 1. 扫描所有的 Unit 提取武器信息
    for (int i = 0; i < p_unit_mgr->units.size(); ++i) {
        UnitData& unit = p_unit_mgr->units[i];
        for (int w = 0; w < unit.weapons.size(); ++w) {
            WeaponStats* w_ptr = unit.weapons[w].stats.ptr();
            if (w_ptr) {
                weapon_grouping_cache[w_ptr].push_back({ i, w });
            }
        }
    }

    // 2. 扫描所有的 Building 提取武器信息 (目前占位，以后再处理)
    /*
    if (p_bld_mgr) {
        // ... (待开发)
    }
    */

    // 3. 执行渲染更新
    for (auto const& [w_ptr, mmi] : weapon_renderers) {
        const auto& indices = weapon_grouping_cache[w_ptr];
        int count = indices.size();

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
        }

        MultiMeshInstance3D* s_mmi = weapon_shadow_renderers[w_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        if (s_mm->get_instance_count() != count) {
            s_mm->set_instance_count(count);
        }

        if (count == 0) continue;

        for (int i = 0; i < count; ++i) {
            int u_idx = indices[i].first;
            int w_idx = indices[i].second;

            UnitData& unit = p_unit_mgr->units[u_idx];
            WeaponData& weapon = unit.weapons[w_idx];

            Vector2 unit_visual_pos = UtilityFunctions::lerp(unit.prev_position, unit.next_position, p_alpha);
            float unit_visual_height = UtilityFunctions::lerp(unit.prev_height, unit.next_height, p_alpha);
            float unit_visual_rotation = UtilityFunctions::lerp_angle(unit.prev_rotation, unit.next_rotation, p_alpha);
            float weapon_visual_rotation = UtilityFunctions::lerp_angle(weapon.prev_rotation, weapon.next_rotation, p_alpha);

            Vector2 rotated_offset = weapon.local_position.rotated(unit_visual_rotation);
            Vector2 weapon_visual_pos = unit_visual_pos + rotated_offset;

            float fake_depth_offset = weapon_visual_pos.y * 0.0001f;
            Vector3 pos_3d = Vector3(weapon_visual_pos.x, unit_visual_height + fake_depth_offset + 0.1f, weapon_visual_pos.y - unit_visual_height);

            Transform3D xform;
            xform.origin = pos_3d;
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);
            xform.basis = xform.basis.rotated(Vector3(0, -1, 0), (weapon_visual_rotation + Math_PI / 2.0f));
            mm->set_instance_transform(i, xform);

            int frames = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_frames() : w_ptr->get_idle_frames();
            int row = (weapon.state == WEAPON_ATTACKING) ? w_ptr->get_attacking_row() : w_ptr->get_idle_row();
            float duration = (float)frames / w_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(weapon.anim_time, duration) * w_ptr->get_anim_fps());

            float modulate = 1.0;
            if (p_sel_mgr && p_sel_mgr->is_unit_selected(unit.id)) {
                modulate = 1.5f;
            }
            else if (p_sel_mgr && p_sel_mgr->is_unit_hovered(unit.id)) {
                modulate = 1.2f;
            }

            mm->set_instance_custom_data(i, Color(frame_idx, row, modulate, 0));
            mm->set_instance_color(i, get_team_color(unit.team_id));

            // 更新阴影
            Transform3D shadow_xform;
            shadow_xform.origin = Vector3(weapon_visual_pos.x + 4.0f,
                unit_visual_height + fake_depth_offset - 0.05f,
                weapon_visual_pos.y + 4.0f);
            shadow_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);
            shadow_xform.basis = shadow_xform.basis.rotated(Vector3(0, -1, 0), (weapon_visual_rotation + Math_PI / 2.0f));

            s_mm->set_instance_transform(i, shadow_xform);
            s_mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));
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