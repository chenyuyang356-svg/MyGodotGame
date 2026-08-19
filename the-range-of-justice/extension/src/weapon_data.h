#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "weapon_stats.h"

namespace godot {
    enum WeaponStateEnum {
        WEAPON_IDLE,
        WEAPON_ATTACKING
    };

    struct WeaponData {
        Ref<WeaponStats> stats;         // 指向共享的武器配置资源
        WeaponStateEnum state = WEAPON_IDLE; // 当前状态

        float current_cooldown = 0.0f;
        float rotation = 0.0f;          // 武器当前真实朝向 (弧度)
        float prev_rotation = 0.0f;
        float next_rotation = 0.0f;
        Vector2 target_direction = Vector2(1, 0); // 期望朝向
        int target_id = -1;             // 锁定的目标

        float anim_time = 0.0f;         // 武器动画播放时间
        Vector2 local_position;         // 相对于主体中心的偏移位置（由主体的插槽决定）

        WeaponData() {}

        void update(float parent_rotation, double p_delta) {
            if (stats.is_null()) return;
            
            // 1. 旋转更新逻辑
            if (stats->get_is_turret()) {
                // 有目标：炮塔独立旋转瞄准目标
                if (target_id != -1 && target_direction.length_squared() > 0.001f) {
                    float final_target_angle = target_direction.angle();
                    float angle_diff = UtilityFunctions::angle_difference(rotation, final_target_angle);
                    float step = stats->get_turn_speed() * p_delta;
                    if (Math::abs(angle_diff) <= step) rotation = final_target_angle;
                    else rotation += Math::sign(angle_diff) * step;
                }
                // 无目标：直接锁定车身朝向，避免车身转弯时炮塔滞后甩动
                else {
                    rotation = parent_rotation;
                }
            }
            else {
                rotation = parent_rotation;
            }
            
            // 2. 冷却更新逻辑
            if (current_cooldown > 0.0f) {
                current_cooldown -= p_delta;
            }

            // 3. 动画状态机逻辑
            anim_time += p_delta;
            if (state == WEAPON_ATTACKING) {
                float duration = (float)stats->get_attacking_frames() / stats->get_anim_fps();
                if (anim_time >= duration) {
                    state = WEAPON_IDLE;
                    anim_time = 0.0f;
                }
            }
        }

        bool try_attack() {
            if (current_cooldown <= 0.0f) {
                current_cooldown = stats->get_attack_interval();
                state = WEAPON_ATTACKING;
                anim_time = 0.0f;
                return true;
            }
            return false;
        }
    };
}