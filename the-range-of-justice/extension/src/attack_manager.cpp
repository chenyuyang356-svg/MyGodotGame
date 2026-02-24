#pragma once
#include "attack_manager.h"
#include "unit_stats.h" 
#include <godot_cpp/variant/utility_functions.hpp> 
#include <godot_cpp/classes/engine.hpp>

using namespace godot;

void AttackManager::_bind_methods() {
    // 如果不需要在 GDScript 中单独调用，这里可以留空，或者绑定 setup
}

AttackManager::AttackManager() {}
AttackManager::~AttackManager() {}

void AttackManager::setup(UnitManager* p_manager) {
    unit_manager = p_manager;
}

void AttackManager::update_units(double p_delta) {
    if (!unit_manager) return;

    for (int i = 0; i < unit_manager->units.size(); ++i) {
        UnitData& unit = unit_manager->units[i];

        // 1. 冷却逻辑
        if (unit.attack_cooldown > 0) unit.attack_cooldown -= p_delta;

        // 2. 状态分发
        switch (unit.state) {
        case IDLE:      _handle_idle(unit); break;
        case CHASING:   _handle_chasing(unit); break;
        case ATTACKING: _handle_attacking(unit, p_delta); break;
        case MOVING:    _handle_moving(unit); break;
        case PATROLLING: _handle_patrolling(unit); break;
            // 可选：移动攻击 (Attack Move) 逻辑可以在这里加
        }
    }
        // 3. 死亡清理
    for (int i = unit_manager->units.size() - 1; i >= 0; --i) {
        if (unit_manager->units[i].current_health <= 0) {
            unit_manager->despawn_unit(unit_manager->units[i].id);
        }
    }
}

bool AttackManager::try_get_combat_force(UnitData& p_unit, Vector2& out_force) {
    // 如果单位在追逐，应用“寻敌移动”力
    if (p_unit.state == CHASING || p_unit.state == PATROLLING) {
        Vector2 desired = (p_unit.target_pos - p_unit.position).normalized() * p_unit.stats->get_move_speed();
        Vector2 steering = (desired - p_unit.velocity);
        Vector2 separation = unit_manager->get_separation(p_unit) * unit_manager->get_separation_factor();
        out_force = steering + separation;
        return true;
    }
    else if (p_unit.state == ATTACKING) {
        out_force = unit_manager->get_friction(p_unit) * unit_manager->get_friction_factor() * 2.0f;
        return true;
    }
    // 其他状态（IDLE, MOVING）不归战斗系统管
    return false;
}

// 检查目标是否存在且活着
bool AttackManager::_is_target_valid(int p_target_id) {
    int idx = unit_manager->get_unit_index_by_id(p_target_id); // 调用 UnitManager 的通用查找
    if (idx == -1) return false;

    // 检查血量
    if (unit_manager->units[idx].current_health <= 0) return false;

    return true;
}

void AttackManager::_handle_idle(UnitData& p_unit) {
    // 空闲时尝试索敌（获取警戒范围内的最近目标）
    float range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius();
    if (_try_find_target(p_unit)) {
        p_unit.state = ATTACKING; // 直接开火，不进入CHASING
    }

        int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
        if (target_idx != -1) {
            UnitData& target = unit_manager->units[target_idx];
            float dist_sq = p_unit.position.distance_squared_to(target.position);

            // 计算实际的攻击距离（基础攻击距离 + 双方碰撞体积）
            float atk_range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

            if (dist_sq <= atk_range * atk_range) {
                // 如果最近的敌人在攻击范围内，直接进入攻击状态
                p_unit.state = ATTACKING;
            }
            else {
                // 如果虽然在警戒范围内，但超出了攻击范围
                // 因为是 IDLE 状态“不会主动追击”，所以当做没看见，清除目标
                p_unit.target_id = -1;
            }
        }
        else {
            p_unit.target_id = -1;
        }
}

void AttackManager::_handle_chasing(UnitData& p_unit) {
    if (!_is_target_valid(p_unit.target_id)) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE; // 回退状态
        return;
    }

    int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
    UnitData& target = unit_manager->units[target_idx];

    // 更新目标位置，供 update_velocity 使用
    p_unit.target_pos = target.position;

    // 距离判断
    float dist_sq = p_unit.position.distance_squared_to(target.position);
    float range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

    if (dist_sq <= range * range) {
        // 【修改】实现边跑边打逻辑
        if (p_unit.stats->get_can_fire_on_move()) {
            UtilityFunctions::print("单位 ", p_unit.id, " 可以移动攻击！");
            if (p_unit.attack_cooldown <= 0) {
                _execute_attack(p_unit, target);
                p_unit.attack_cooldown = p_unit.stats->get_attack_interval();
            }
        }
        else {
            p_unit.state = ATTACKING; // 传统定点攻击
        }
    }

    else if (dist_sq > p_unit.stats->get_aggro_range() * p_unit.stats->get_aggro_range() * 4.0f) {
        // 追太远了，放弃追击，恢复状态
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
    }

    else if (!p_unit.is_manual_target && dist_sq > p_unit.stats->get_aggro_range() * p_unit.stats->get_aggro_range() * 4.0f) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
    }
}

void AttackManager::_handle_attacking(UnitData& p_unit, double p_delta) {
    if (!_is_target_valid(p_unit.target_id)) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE; // 目标死亡恢复状态
        return;
    }

    int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
    UnitData& target = unit_manager->units[target_idx];

    // 如果目标跑出攻击范围，转回追击
    float dist_sq = p_unit.position.distance_squared_to(target.position);
    float range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

    // 给一点容错空间 (buffer)，防止在临界点反复抽搐
    float buffer = 10.0f;
    if (dist_sq > (range + buffer) * (range + buffer)) {
        p_unit.state = CHASING;

    // 当目标无效（死亡或消失）时
    if (!_is_target_valid(p_unit.target_id)) {
        p_unit.target_id = -1;
   // 如果身上带有巡逻标记，就回去继续巡逻，否则回退到 IDLE
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
        return;
        }
    return;
    }

    // 执行攻击
    if (p_unit.attack_cooldown <= 0) {
        _execute_attack(p_unit, target);
        p_unit.attack_cooldown = p_unit.stats->get_attack_interval();
    }
}

void AttackManager::_handle_patrolling(UnitData& p_unit) {
    // 1. 巡逻时主动寻找 aggro_range 范围内的敌人
    if (_try_find_target(p_unit)) {
        p_unit.state = CHASING;
        return;
    }

    // 2. 没有敌人，沿着指定路径点移动
    if (p_unit.patrol_waypoints.empty()) {
        p_unit.is_patrolling = false;
        p_unit.state = IDLE;
        return;
    }

    Vector2 current_target = p_unit.patrol_waypoints[p_unit.current_waypoint_idx];
    p_unit.target_pos = current_target;

    // 检查是否到达当前巡逻点 (给予一点寻路容差)
    float col_rad = p_unit.stats->get_collision_radius();
    if (p_unit.position.distance_squared_to(current_target) < col_rad * col_rad * 4.0f) {
        p_unit.current_waypoint_idx = (p_unit.current_waypoint_idx + 1) % p_unit.patrol_waypoints.size();
    }
}

void AttackManager::_handle_moving(UnitData& p_unit) {
    // 1. Check state and permission (Prints every 60 frames to avoid spam)
    if (Engine::get_singleton()->get_process_frames() % 60 == 0) {
        UtilityFunctions::print("[MOVING State] Unit ID: ", p_unit.id, " | can_fire_on_move: ", p_unit.stats->get_can_fire_on_move());
    }

    if (p_unit.stats->get_can_fire_on_move()) {
        // 2. Try to find a target
        if (_try_find_target(p_unit)) {
            int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
            if (target_idx != -1) {
                UnitData& target = unit_manager->units[target_idx];
                float dist_sq = p_unit.position.distance_squared_to(target.position);
                float atk_range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

                // 3. Check distance
                if (dist_sq <= atk_range * atk_range) {
                    if (p_unit.attack_cooldown <= 0) {
                        _execute_attack(p_unit, target);
                        p_unit.attack_cooldown = p_unit.stats->get_attack_interval();

                        // Action executed!
                        UtilityFunctions::print("----> SUCCESS: Move-attack executed on Target ID: ", target.id);
                    }
                }
                else {
                    // Target found, but out of range
                    if (Engine::get_singleton()->get_process_frames() % 60 == 0) {
                        UtilityFunctions::print("---- FAILED: Target found but out of range. Current dist_sq: ", dist_sq, " | Required range_sq: ", atk_range * atk_range);
                    }
                }
            }
        }
    }
}

// 索敌逻辑：使用 UnitManager 的网格查询
bool AttackManager::_try_find_target(UnitData& p_unit) {
    float range = p_unit.stats->get_aggro_range();
    std::vector<int> nearby = unit_manager->get_nearby_units(p_unit.position, range);

    int best_target = -1;
    float min_dist_sq = range * range;

    for (int idx : nearby) {
        UnitData& other = unit_manager->units[idx];

        if (other.id == p_unit.id) continue;          // 不是自己
        if (other.team_id == p_unit.team_id) continue; // 不是友军
        if (other.current_health <= 0) continue;       // 不是死人

        float d = p_unit.position.distance_squared_to(other.position);
        if (d < min_dist_sq) {
            min_dist_sq = d;
            best_target = other.id;
        }
    }

    if (best_target != -1) {
        p_unit.target_id = best_target;
        p_unit.is_manual_target = false;
        return true;
    }
    return false;
}

void AttackManager::_execute_attack(UnitData& attacker, UnitData& defender) {
    // 简单的伤害计算
    float dmg = attacker.stats->get_attack_damage();
    // 这里可以加减防逻辑: dmg -= defender.stats->get_armor()...

    defender.current_health -= dmg;

    // 简单的反击AI：如果不动且被打，就打回去
    if (defender.state == IDLE && defender.target_id == -1) {
        defender.target_id = attacker.id;
        defender.state = CHASING;
    }
}

