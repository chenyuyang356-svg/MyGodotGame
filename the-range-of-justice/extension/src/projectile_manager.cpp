#include "projectile_manager.h"
#include "unit_manager.h"   
#include "attack_manager.h" 
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void ProjectileManager::_bind_methods() {
    // 如果需要在 GDScript 调用 spawn_projectile，可以在这里绑定
}

ProjectileManager::ProjectileManager() {}
ProjectileManager::~ProjectileManager() {}

void ProjectileManager::setup(UnitManager* p_um, AttackManager* p_am) {
    unit_manager = p_um;
    attack_manager = p_am;

    set_physics_process(true);
}

void ProjectileManager::spawn_projectile(
    Vector2 p_start_pos, float p_start_height,
    int p_target_id, float p_target_height,
    float p_damage, float p_speed, int p_source_id,
    float p_splash_radius, int p_type, float p_arc_height, float p_acceleration)
{
    ProjectileData p;
    p.position = p_start_pos;
    p.target_pos = p_start_pos;
    p.start_pos = p_start_pos;

    p.start_height = p_start_height;
    p.target_height = p_target_height;
    p.current_height = p_start_height;
    p.arc_height = p_arc_height;

    p.target_id = p_target_id;
    p.source_id = p_source_id;
    p.damage = p_damage;
    p.speed = p_speed;
    p.acceleration = p_acceleration;
    p.splash_radius = p_splash_radius;
    p.type = p_type;

    projectiles.push_back(p);
}

void ProjectileManager::_physics_process(double p_delta) {
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!unit_manager || !attack_manager) return;

    for (auto it = projectiles.begin(); it != projectiles.end(); ) {
        bool target_alive = false;

        // 1. 获取目标最新位置
        int target_idx = unit_manager->get_unit_index_by_id(it->target_id);
        if (target_idx != -1) {
            UnitData& target = unit_manager->units[target_idx];
            if (target.current_health > 0) {
                it->target_pos = target.position;
                target_alive = true;
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
                attack_manager->apply_aoe_damage(it->target_pos, it->splash_radius, it->damage, it->source_id);
            }
            else {
                if (target_alive) {
                    attack_manager->apply_damage(it->target_id, it->damage, it->source_id);
                }
            }
            it = projectiles.erase(it);
        }
        // 4. 飞行与高度计算
        else {
            it->position += (direction / distance_to_target) * move_step;

            // --- 计算动态进度 t (0.0 到 1.0) ---
            // 使用 (已走距离 / 总距离)，这样即使目标移动，t 也能平滑过渡
            float traveled = it->start_pos.distance_to(it->position);
            float remaining = distance_to_target - move_step; // 剩余距离
            float total = traveled + remaining;
            float t = (total > 0.001f) ? (traveled / total) : 1.0f;

            // --- 计算高度基线 (两点之间的 3D 直线) ---
            float base_height = it->start_height + (it->target_height - it->start_height) * t;

            // --- 根据类型附加高度 ---
            if (it->type == PROJECTILE_BULLET) {
                // 子弹：走纯直线
                it->current_height = base_height;
            }
            else if (it->type == PROJECTILE_SHELL || it->type == PROJECTILE_MISSILE) {
                // 炮弹和导弹：走抛物线。叠加 4 * H * t * (1 - t) 的二次函数偏移
                it->current_height = base_height + (4.0f * it->arc_height * t * (1.0f - t));
            }

            ++it;
        }
    }
    update_render_buffer();
}

void ProjectileManager::update_render_buffer() {
    // 省略初始化检查等代码
    int idx = 0;
    for (const auto& p : projectiles) {
        Transform3D transform;
        // 注意 Godot 3D 坐标系中，Y轴通常是向上的高度
        Vector3 pos_3d = Vector3(p.position.x, p.current_height, p.position.y);
        transform = transform.translated(pos_3d);

        // 此处可以添加 look_at() 逻辑让导弹/子弹朝向飞行方向

        // multimesh->set_instance_transform(idx, transform);
        idx++;
    }
}