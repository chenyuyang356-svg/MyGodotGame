#include "projectile_manager.h"
#include "unit_manager.h"   
#include "attack_manager.h" 
#include "projectile_loader.h"
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void ProjectileManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_projectile_type", "type_name", "config_path"), &ProjectileManager::register_projectile_type);
    ClassDB::bind_method(D_METHOD("spawn_projectile",
        "start_pos", "start_height",
        "target_id", "target_is_building", "target_height",
        "damage", "speed",
        "source_id", "source_is_building", "splash_radius",
        "type", "arc_height", "acceleration"),
        &ProjectileManager::spawn_projectile);
    ClassDB::bind_method(D_METHOD("setup", "p_um", "p_am"), &ProjectileManager::setup);
    ClassDB::bind_method(D_METHOD("set_building_manager", "p_bm"), &ProjectileManager::set_building_manager);
}

ProjectileManager::ProjectileManager() {}
ProjectileManager::~ProjectileManager() {}

void ProjectileManager::register_projectile_type(const String& p_type_name, const String& p_config_path) {
    Ref<ProjectileStats> stats = ProjectileLoader::load_stats_from_txt(p_config_path);

    if (stats.is_valid()) {
        projectile_templates[p_type_name] = stats;
        UtilityFunctions::print("成功注册投射物: ", p_type_name, " 路径: ", p_config_path);
    }
    else {
        UtilityFunctions::printerr("注册投射物失败, 无法加载配置文件: ", p_config_path);
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

void ProjectileManager::spawn_projectile(
    Vector2 p_start_pos, float p_start_height,
    int p_target_id, bool p_target_is_building, float p_target_height,
    float p_damage, float p_speed, int p_source_id, bool p_source_is_building,
    float p_splash_radius, int p_type, float p_arc_height, float p_acceleration)
{
    UtilityFunctions::print(">>> [ProjectileManager] Spawning projectile! Target ID: ", p_target_id);
    ProjectileData p;
    p.position = p_start_pos;
    p.target_pos = p_start_pos;
    p.start_pos = p_start_pos;

    p.start_height = p_start_height;
    p.target_height = p_target_height;
    p.current_height = p_start_height;
    p.arc_height = p_arc_height;

    p.target_id = p_target_id;
    p.target_is_building = p_target_is_building; // 记录目标类型
    p.source_id = p_source_id;
    p.source_is_building = p_source_is_building; // 记录攻击者类型

    p.damage = p_damage;
    p.speed = p_speed;
    p.acceleration = p_acceleration;
    p.splash_radius = p_splash_radius;
    p.type = p_type;

    projectiles.push_back(p);
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
                    Vector2 cell_sz = Vector2(32, 32);
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
        float move_step = it->speed * p_delta;

        // 3. 命中判定
        if (distance_to_target <= move_step) {
            if (it->splash_radius > 0.0f) {
                // AoE 伤害：传入攻击者是建筑还是单位
                attack_manager->apply_aoe_damage(it->target_pos, it->splash_radius, it->damage, it->source_id, it->source_is_building);
            }
            else {
                // 单体伤害：如果目标依然存活，则造成伤害
                if (target_alive) {
                    attack_manager->apply_damage(it->target_id, it->target_is_building, it->damage, it->source_id, it->source_is_building);
                }
            }
            UtilityFunctions::print("get it! target_alive:", target_alive);
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
    update_render_buffer();
}

void ProjectileManager::update_render_buffer() {
    int idx = 0;
    for (const auto& p : projectiles) {
        Transform3D transform;
        Vector3 pos_3d = Vector3(p.position.x, p.current_height, p.position.y);
        transform = transform.translated(pos_3d);

        // 此处可以添加 look_at() 逻辑

        // multimesh->set_instance_transform(idx, transform);
        idx++;
    }
}
