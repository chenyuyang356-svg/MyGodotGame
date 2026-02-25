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

void ProjectileManager::spawn_projectile(Vector2 p_start_pos, int p_target_id, float p_damage, float p_speed, int p_source_id, float p_splash_radius) {
    ProjectileData p;
    p.position = p_start_pos;
    p.target_pos = p_start_pos;
    p.target_id = p_target_id;
    p.source_id = p_source_id;
    p.damage = p_damage;
    p.speed = p_speed;
    p.splash_radius = p_splash_radius;

    projectiles.push_back(p);
}

void ProjectileManager::_physics_process(double p_delta) {
    // 在编辑器模式下不执行
    if (Engine::get_singleton()->is_editor_hint()) return;
    if (!unit_manager || !attack_manager) return;

    // 核心循环：使用迭代器遍历，因为我们会在子弹命中时将其从 vector 中安全删除
    for (auto it = projectiles.begin(); it != projectiles.end(); ) {

        bool target_alive = false;

        // 1. 尝试获取目标的最新信息
        int target_idx = unit_manager->get_unit_index_by_id(it->target_id);
        if (target_idx != -1) {
            UnitData& target = unit_manager->units[target_idx];
            if (target.current_health > 0) {
                // 目标存活：更新子弹的追踪目标点
                it->target_pos = target.position;
                target_alive = true;
            }
        }
        // 如果 target_idx == -1 或 health <= 0，说明目标已死
        // 我们不更新 it->target_pos，让子弹继续飞向最后一次记录的 "target_pos" (即鞭尸位置)

        // 2. 计算这一帧的位移方向和距离
        Vector2 direction = it->target_pos - it->position;
        float distance_to_target = direction.length();
        float move_step = it->speed * p_delta; // 这一帧能飞多远

        // 3. 命中判定
        // 如果距离目标的距离，小于或等于这帧能飞行的距离，说明这一帧必定打中
        if (distance_to_target <= move_step) {
            if (it->splash_radius > 0.0f) {
                // 如果是范围伤害，无视原目标死活，直接在当前落地坐标引爆！
                // 这样即使原目标提前死了，子弹飞到地上的“尸体”位置也会炸伤周围的人
                attack_manager->apply_aoe_damage(it->target_pos, it->splash_radius, it->damage, it->source_id);
            }
            else {
                // 如果是单体伤害（如普通机枪），只有目标存活才扣血
                if (target_alive) {
                    attack_manager->apply_damage(it->target_id, it->damage, it->source_id);
                }
            }
            it = projectiles.erase(it);
        }
        // 4. 继续飞行
        else {
            it->position += (direction / distance_to_target) * move_step;

            // 处理下一颗子弹
            ++it;
        }
    }

    // 5. 逻辑算完后，更新视觉渲染
    update_render_buffer();
}

void ProjectileManager::update_render_buffer() {
    // 类似于你在 UnitManager 中的 update_multimesh_buffer 逻辑
    // 遍历 projectiles 数组，将每一发子弹的 position (和朝向) 写入到 MultiMesh 中
    // 这样 1 个 Draw Call 就能画出几千发子弹
}