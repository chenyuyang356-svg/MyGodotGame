#include "projectile_manager.h"
#include "unit_manager.h"   
#include "attack_manager.h" 
#include "projectile_loader.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/dir_access.hpp>

using namespace godot;

void ProjectileManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_projectile_type", "type_name", "config_path"), &ProjectileManager::register_projectile_type);
    ClassDB::bind_method(D_METHOD("register_projectiles_from_dir", "path"), &ProjectileManager::register_projectiles_from_dir);

    // 更新绑定的参数列表
    ClassDB::bind_method(D_METHOD("spawn_projectile",
        "type_name",
        "start_pos", "start_height",
        "target_id", "target_is_building", "target_height",
        "source_id", "source_is_building", "weapon_damage"),
        &ProjectileManager::spawn_projectile);

    ClassDB::bind_method(D_METHOD("setup", "p_um", "p_am"), &ProjectileManager::setup);
    ClassDB::bind_method(D_METHOD("set_building_manager", "p_bm"), &ProjectileManager::set_building_manager);

    ClassDB::bind_method(D_METHOD("set_projectile_glow_shader", "shader"), &ProjectileManager::set_projectile_glow_shader);
    ClassDB::bind_method(D_METHOD("get_projectile_glow_shader"), &ProjectileManager::get_projectile_glow_shader);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "projectile_glow_shader", PROPERTY_HINT_RESOURCE_TYPE, "Shader"), "set_projectile_glow_shader", "get_projectile_glow_shader");

    ClassDB::bind_method(D_METHOD("set_ground_light_shader", "shader"), &ProjectileManager::set_ground_light_shader);
    ClassDB::bind_method(D_METHOD("get_ground_light_shader"), &ProjectileManager::get_ground_light_shader);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "ground_light_shader", PROPERTY_HINT_RESOURCE_TYPE, "Shader"), "set_ground_light_shader", "get_ground_light_shader");

    ClassDB::bind_method(D_METHOD("set_shadow_shader", "shader"), &ProjectileManager::set_shadow_shader);
    ClassDB::bind_method(D_METHOD("get_shadow_shader"), &ProjectileManager::get_shadow_shader);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shadow_shader", PROPERTY_HINT_RESOURCE_TYPE, "Shader"), "set_shadow_shader", "get_shadow_shader");

    ClassDB::bind_method(D_METHOD("set_common_light_tex", "texture"), &ProjectileManager::set_common_light_tex);
    ClassDB::bind_method(D_METHOD("get_common_light_tex"), &ProjectileManager::get_common_light_tex);
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "common_light_tex", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_common_light_tex", "get_common_light_tex");
}

void ProjectileManager::spawn_projectile(
    const String& p_type_name,
    Vector2 p_start_pos, float p_start_height,
    int p_target_id, bool p_target_is_building, float p_target_height,
    int p_source_id, bool p_source_is_building,
    float p_weapon_damage)
{
    if (projectile_templates.find(p_type_name) == projectile_templates.end()) {
        UtilityFunctions::printerr(">>> [ProjectileManager] 无法生成投射物，未知的类型: ", p_type_name);
        return;
    }

    Ref<ProjectileStats> stats = projectile_templates[p_type_name];

    // --- 新增：立即尝试获取目标的初始位置 ---
    Vector2 initial_target_pos = p_start_pos;
    bool found_target = false;

    if (p_target_is_building) {
        if (building_manager) {
            auto b_it = building_manager->buildings.find(p_target_id);
            if (b_it != building_manager->buildings.end()) {
                Vector2 cell_sz = Vector2(16, 16);
                if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
                Vector2 fp_size = Vector2(b_it->second.stats->get_footprint()) * cell_sz;
                initial_target_pos = Vector2(b_it->second.grid_pos) * cell_sz + fp_size * 0.5f;
                found_target = true;
            }
        }
    }
    else {
        if (unit_manager) {
            int target_idx = unit_manager->get_unit_index_by_id(p_target_id);
            if (target_idx != -1) {
                initial_target_pos = unit_manager->units[target_idx].position;
                found_target = true;
            }
        }
    }

    // 如果在发射瞬间目标就没了，直接不产生子弹
    if (!found_target) {
        return;
    }

    ProjectileData p;
    p.position = p_start_pos;
    p.target_pos = p_start_pos;
    p.start_pos = p_start_pos;

    p.start_height = p_start_height;
    p.target_height = p_target_height;
    p.current_height = p_start_height;

    p.target_id = p_target_id;
    p.target_is_building = p_target_is_building;
    p.source_id = p_source_id;
    p.source_is_building = p_source_is_building;
    p.damage = p_weapon_damage;

    p.stats = stats;
    p.speed = stats->get_speed();
    p.acceleration = stats->get_acceleration();
    p.splash_radius = stats->get_splash_radius();
    p.type = stats->get_projectile_type();
    p.arc_height = stats->get_arc_height();

    projectiles.push_back(p);

    if (p.type == PROJECTILE_BULLET && !(stats->get_is_healing())) {
        Vector3 pos = Vector3(p_start_pos.x, p_start_height, p_start_pos.y);
        audio_manager->play_sfx_3d("GunShot", pos, -15.0f);
    }
}
ProjectileManager::ProjectileManager() {}
ProjectileManager::~ProjectileManager() {}

void ProjectileManager::_internal_register_projectile(Ref<ProjectileStats> p_stats) {
    String p_type_name = p_stats->get_projectile_name();
    projectile_templates[p_type_name] = p_stats;
    ProjectileStats* stats_ptr = p_stats.ptr();

    if (type_renderers.find(stats_ptr) != type_renderers.end()) return;

    // 1. 创建主渲染器 (Glow)
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_type_name + "_Renderer");
    add_child(mmi);

    // 配置 MultiMesh 和 Mesh (逻辑同你之前的代码)
    Ref<MultiMesh> mm; mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh; qmesh.instantiate();
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(p_stats->get_visual_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(p_stats->get_h_frames(), p_stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    Ref<ShaderMaterial> mat; mat.instantiate();
    mat->set_shader(projectile_glow_shader); // 使用注册的 Glow Shader
    mat->set_shader_parameter("albedo_texture", tex);
    mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    mmi->set_material_override(mat);
    type_renderers[stats_ptr] = mmi;

    // 2. 创建地面假光渲染器
    MultiMeshInstance3D* l_mmi = memnew(MultiMeshInstance3D);
    l_mmi->set_name(p_type_name + "_GroundLight");
    add_child(l_mmi);

    Ref<MultiMesh> l_mm; l_mm.instantiate();
    l_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    l_mm->set_use_colors(true);
    l_mm->set_mesh(qmesh); // 复用尺寸
    l_mmi->set_multimesh(l_mm);

    Ref<ShaderMaterial> l_mat; l_mat.instantiate();
    l_mat->set_shader(ground_light_shader); // 使用地面假光 Shader
    l_mat->set_shader_parameter("light_texture", common_light_tex); // 使用编辑器拖入的 tres
    l_mmi->set_material_override(l_mat);
    light_renderers[stats_ptr] = l_mmi;

    // 3. 创建影子渲染器 (代码逻辑同你之前的，使用 shadow_shader)
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_type_name + "_Shadows");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true);
    s_mm->set_mesh(qmesh); // 复用上面的 QuadMesh 尺寸
    s_mmi->set_multimesh(s_mm);

    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shadow.gdshader");
    }
    Ref<ShaderMaterial> s_mat;
    s_mat.instantiate();
    s_mat->set_shader(shadow_shader);
    s_mat->set_shader_parameter("albedo_texture", tex);
    s_mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    s_mat->set_shader_parameter("v_frames", p_stats->get_v_frames());

    s_mmi->set_material_override(s_mat);
    shadow_renderers[stats_ptr] = s_mmi;

    UtilityFunctions::print(">>> [ProjectileManager] 成功注册投射物渲染器: ", p_type_name);
}

void ProjectileManager::register_projectile_type(String p_type_name, String p_config_path) {
    Ref<ProjectileStats> stats = ProjectileLoader::load_stats_from_cfg(p_config_path);
    if (stats.is_null()) return;
    if (stats->get_projectile_name().is_empty()) stats->set_projectile_name(p_type_name);
    _internal_register_projectile(stats);
}

void ProjectileManager::register_projectiles_from_dir(String p_dir_path) {
    Ref<DirAccess> dir = DirAccess::open(p_dir_path);
    if (dir.is_null()) return;

    dir->list_dir_begin();
    String file_name = dir->get_next();
    while (file_name != "") {
        if (!dir->current_is_dir() && file_name.ends_with(".cfg")) {
            String full_path = p_dir_path.path_join(file_name);
            Ref<ProjectileStats> stats = ProjectileLoader::load_stats_from_cfg(full_path);
            if (stats.is_valid() && !stats->get_projectile_name().is_empty()) {
                _internal_register_projectile(stats);
            }
        }
        file_name = dir->get_next();
    }
}

void ProjectileManager::setup(UnitManager* p_um, AttackManager* p_am) {
    unit_manager = p_um;
    attack_manager = p_am;

    set_physics_process(true);
}

// 接收 BuildingManager
void ProjectileManager::set_building_manager(BuildingManager* p_bm) {
    building_manager = p_bm;
}

void ProjectileManager::set_effect_manager(Node* p_node) {
    effect_manager = Object::cast_to<EffectManager>(p_node);
}

void ProjectileManager::set_audio_manager(Node* p_node)
{
    audio_manager = Object::cast_to<AudioManager>(p_node);
}

void ProjectileManager::_physics_process(double p_delta) {
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!unit_manager || !attack_manager) {
        if (Engine::get_singleton()->get_frames_drawn() % 60 == 0) {
            UtilityFunctions::printerr(">>> [ProjectileManager] Waiting for dependencies... UM: ", unit_manager != nullptr, " AM: ", attack_manager != nullptr);
        }
        return;
    }

    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        bool target_alive = false;

        // 1. 获取目标最新位置 (区分建筑和单位)
        if (it->target_is_building) {
            if (building_manager) {
                auto b_it = building_manager->buildings.find(it->target_id);
                if (b_it != building_manager->buildings.end() && b_it->second.current_health > 0) {
                    // 获取建筑的中心坐标
                    Vector2 cell_sz = Vector2(16, 16);
                    if (building_manager->flow_field_manager) {
                        cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
                    }
                    Vector2 fp_size = Vector2(b_it->second.stats->get_footprint()) * cell_sz;
                    it->target_pos = Vector2(b_it->second.grid_pos) * cell_sz + fp_size * 0.5f;
                    target_alive = true;
                }
            }
        }
        else {
            int target_idx = unit_manager->get_unit_index_by_id(it->target_id);
            if (target_idx != -1) {
                UnitData& target = unit_manager->units[target_idx];
                if (target.current_health > 0) {
                    it->target_pos = target.position;
                    target_alive = true;
                }
            }
        }

        // 2. 如果是导弹，应用加速度 (越飞越快)
        if (it->type == PROJECTILE_MISSILE) {
            it->speed += it->acceleration * p_delta;
        }

        Vector2 direction = it->target_pos - it->position;
        float distance_to_target = direction.length();

        // --- 保护措施：防止距离为0导致的计算错误 ---
        if (distance_to_target < 0.1f) {
            distance_to_target = 0.1f;
        }

        float move_step = it->speed * p_delta;

        // 3. 命中判定
        if (distance_to_target <= move_step) {
            // --- 联机逻辑关键修改 ---
            // 只有服务器有权调用 AttackManager 进行结算
            if (get_multiplayer()->is_server()) {
                if (it->stats->get_is_healing()) {
                    // 触发治疗逻辑
                    attack_manager->apply_healing(it->target_id, it->target_is_building, it->damage, it->source_id);
                }
                else {
                    // 触发原有伤害逻辑
                    if (it->splash_radius > 0.0f) {
                        attack_manager->apply_aoe_damage(it->target_pos, it->splash_radius, it->damage, it->source_id, it->source_is_building);
                    }
                    else if (target_alive) {
                        attack_manager->apply_damage(it->target_id, it->target_is_building, it->damage, it->source_id, it->source_is_building);
                    }
                }
            }
            else {
                // 客户端仅销毁，不处理伤害
                // UtilityFunctions::print("Client: Projectile Visual Hit.");
            }

            // 爆炸特效
            if (it->target_id != -1) {
                // 计算从发射点到现在的位移
                float traveled_so_far = it->start_pos.distance_to(it->position);

                // 只有当移动距离超过一定阈值（比如5.0像素），才产生爆炸效果
                // 这样即使在非常极端的情况下目标死在发射者脚下，也不会有特效
                if (traveled_so_far > 10.0f) {
                    Vector3 pos = Vector3((it->position).x, it->current_height + 0.1f, (it->position).y);
                    Vector3 vel = Vector3(0, 0, 0);
                    if (it->stats->get_is_healing()) {
                        effect_manager->emit_particle("Healing", pos, vel, 2.0f, 1.0f);
                    }
                    else {
                        float scale = (it->type == PROJECTILE_BULLET) ? 0.5f : 1.5f;
                        effect_manager->emit_particle("Explosion", pos, vel, scale, 0.2f);
                        if (!(it->type == PROJECTILE_BULLET)) {
                            audio_manager->play_sfx_3d("Explosion", pos, -5.0f);
                        }
                    }
                }
            }

            it = projectiles.erase(it);
        }
        // 4. 飞行与高度计算
        else {
            it->position += (direction / distance_to_target) * move_step;

            float traveled = it->start_pos.distance_to(it->position);
            float remaining = distance_to_target - move_step;
            float total = traveled + remaining;
            float t = (total > 0.001f) ? (traveled / total) : 1.0f;

            float base_height = it->start_height + (it->target_height - it->start_height) * t;

            if (it->type == PROJECTILE_BULLET) {
                it->current_height = base_height;
            }
            else if (it->type == PROJECTILE_SHELL || it->type == PROJECTILE_MISSILE) {
                it->current_height = base_height + (4.0f * it->arc_height * t * (1.0f - t));
            }

            ++it;
        }
    }
    update_render_buffer(p_delta);
}

void ProjectileManager::update_render_buffer(double p_delta) {
    if (type_renderers.empty()) return;

    // 1. 清空分组缓存
    for (auto& pair : type_grouping_cache) {
        pair.second.clear();
    }

    // 2. 按 stats 指针进行分组
    for (int i = 0; i < (int)projectiles.size(); ++i) {
        ProjectileStats* s_ptr = projectiles[i].stats.ptr();
        if (s_ptr) {
            type_grouping_cache[s_ptr].push_back(i);
        }
    }

    // 3. 遍历渲染器更新
    for (auto const& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& indices = type_grouping_cache[s_ptr];
        int count = (int)indices.size();

        mmi->get_multimesh()->set_instance_count(count);
        shadow_renderers[s_ptr]->get_multimesh()->set_instance_count(count);
        light_renderers[s_ptr]->get_multimesh()->set_instance_count(count);

        if (count == 0) continue;

        Ref<MultiMesh> mm = mmi->get_multimesh();
        Ref<MultiMesh> s_mm = shadow_renderers[s_ptr]->get_multimesh();
        Ref<MultiMesh> l_mm = light_renderers[s_ptr]->get_multimesh();

        for (int i = 0; i < count; ++i) {
            ProjectileData& p = projectiles[indices[i]]; // 注意：这里用引用，因为要更新 anim_time

            // --- A. 更新动画时间 ---
            p.anim_time += p_delta;

            // --- B. 计算动画帧 (完全仿照 UnitManager 逻辑) ---
            // 假设 ProjectileStats 有这些 Getter（如果没有，需在 stats 类中添加）
            int h_frames = s_ptr->get_h_frames();
            float anim_fps = s_ptr->get_anim_fps();

            // 计算当前帧索引 (简单的循环播放)
            // 如果投射物有多行(如旋转图集)，row 可以根据方向计算，这里默认取第0行
            int frame_idx = (int)(p.anim_time * anim_fps) % h_frames;
            int row = 0;
            float modulate = 1.0f; // 投射物通常不需要 hover 高亮，设为 1.0

            // --- C. 物理变换计算 ---
            float fake_depth_offset = p.position.y * 0.0001f;
            Vector3 pos_3d = Vector3(p.position.x, p.current_height + fake_depth_offset, p.position.y);

            // 计算旋转方向
            Vector2 dir = p.target_pos - p.position;
            float angle = dir.angle();

            // D. 更新投射物本体
            Transform3D xform;
            xform.origin = Vector3(p.position.x, p.current_height, p.position.y - p.current_height);
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0).rotated(Vector3(0, -1, 0), angle + Math_PI / 2.0f);
            mm->set_instance_transform(i, xform);
            mm->set_instance_custom_data(i, Color((float)frame_idx, 0.0f, 1.0f, 0.0f));

            // E. 更新影子
            Transform3D s_xform;
            s_xform.origin = Vector3(p.position.x + 1.0f, 0.05f, p.position.y + 1.0f);
            s_xform.basis = xform.basis;
            s_mm->set_instance_transform(i, s_xform);
            s_mm->set_instance_custom_data(i, Color((float)frame_idx, 0.0f, 0.0f, 0.0f));

            // F. 更新地面假光
            Transform3D l_xform;
            // 离地高度固定在 0.08，略高于影子
            l_xform.origin = Vector3(p.position.x, 0.08f, p.position.y - p.current_height);
            l_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            // 假光随高度变淡
            float h_ratio = UtilityFunctions::clamp(1.0f - (p.current_height / 15.0f), 0.3f, 1.0f);
            l_xform.basis = l_xform.basis.scaled(Vector3(3.0, 3.0, 3.0) * (1.2f - h_ratio * 0.2f));

            l_mm->set_instance_transform(i, l_xform);
            // 颜色可以根据投射物类型动态设置，这里示例用橙色
            Vector3 color = (p.stats->get_is_healing()) ? Vector3(0.2, 1.0, 0.4) : Vector3(1.0, 0.4, 0.1);
            l_mm->set_instance_color(i, Color(color.x, color.y, color.z, 0.6 * h_ratio));

            // --- G. 生成粒子 ---
            if (p.type == PROJECTILE_MISSILE) {
                p.particle_update_timer += p_delta;
                if (p.particle_update_timer > 0.05f) {
                    p.particle_update_timer = 0.0f;
                    Vector3 pos = Vector3(p.position.x, p.current_height, p.position.y);
                    Vector3 vel = Vector3(UtilityFunctions::randf_range(-10, 10), 2.0, UtilityFunctions::randf_range(-10, 10));
                    effect_manager->emit_particle("Dust", pos, vel, 1.75, 1.0);
                }
            }
        }
    }
}
