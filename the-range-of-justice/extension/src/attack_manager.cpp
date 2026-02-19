#pragma once
#include "attack_manager.h"
#include "unit_stats.h" 

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
        UnitManager::UnitData& unit = unit_manager->units[i];

        // 1. 冷却逻辑
        if (unit.attack_cooldown > 0) unit.attack_cooldown -= p_delta;

        // 2. 状态分发
        switch (unit.state) {
        case UnitManager::IDLE:      _handle_idle(unit); break;
        case UnitManager::CHASING:   _handle_chasing(unit); break;
        case UnitManager::ATTACKING: _handle_attacking(unit, p_delta); break;
        case UnitManager::MOVING:
            // 可选：移动攻击 (Attack Move) 逻辑可以在这里加
            break;
        }
    }
        // 3. 死亡清理
    for (int i = unit_manager->units.size() - 1; i >= 0; --i) {
        if (unit_manager->units[i].current_health <= 0) {
            unit_manager->despawn_unit(unit_manager->units[i].id);
        }
    }
}

bool AttackManager::AttackManager::try_get_combat_force(UnitManager::UnitData& p_unit, Vector2& out_force) {
    // 如果单位在追逐，应用“寻敌移动”力
    if (p_unit.state == UnitManager::CHASING) {
        // 目标位置已在 _handle_chasing 中更新到 p_unit.target_pos
        Vector2 desired = (p_unit.target_pos - p_unit.position).normalized() * p_unit.stats->get_move_speed();
        Vector2 steering = (desired - p_unit.velocity);

        // 加上分离力
        Vector2 separation = unit_manager->get_separation(p_unit) * unit_manager->get_separation_factor();

        out_force = steering + separation;
        return true;
    }
        // 如果单位在攻击，应用“停止/摩擦”力
    else if (p_unit.state == UnitManager::ATTACKING) {
        // 攻击时停下
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

void AttackManager::_handle_idle(UnitManager::UnitData& p_unit) {
    // 空闲时尝试索敌
    if (_try_find_target(p_unit)) {
        p_unit.state = UnitManager::CHASING;
    }
}

void AttackManager::_handle_chasing(UnitManager::UnitData& p_unit) {
    if (!_is_target_valid(p_unit.target_id)) {
        p_unit.state = UnitManager::IDLE;
        p_unit.target_id = -1;
        return;
    }

    int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
    UnitManager::UnitData& target = unit_manager->units[target_idx];

    // 更新目标位置，供 UnitManager::update_velocity 使用
    p_unit.target_pos = target.position;

    // 距离判断
    float dist_sq = p_unit.position.distance_squared_to(target.position);
    float range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

    if (dist_sq <= range * range) {
        p_unit.state = UnitManager::ATTACKING;
    }
    // 如果追太远了 (比如 2 倍警戒范围)，放弃追击
    else if (dist_sq > p_unit.stats->get_aggro_range() * p_unit.stats->get_aggro_range() * 4.0f) {
        p_unit.state = UnitManager::IDLE;
        p_unit.target_id = -1;
    }
}

void AttackManager::_handle_attacking(UnitManager::UnitData& p_unit, double p_delta) {
    if (!_is_target_valid(p_unit.target_id)) {
        p_unit.state = UnitManager::IDLE;
        return;
    }

    int target_idx = unit_manager->get_unit_index_by_id(p_unit.target_id);
    UnitManager::UnitData& target = unit_manager->units[target_idx];

    // 如果目标跑出攻击范围，转回追击
    float dist_sq = p_unit.position.distance_squared_to(target.position);
    float range = p_unit.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target.stats->get_collision_radius();

    // 给一点容错空间 (buffer)，防止在临界点反复抽搐
    float buffer = 10.0f;
    if (dist_sq > (range + buffer) * (range + buffer)) {
        p_unit.state = UnitManager::CHASING;
        return;
    }

    // 执行攻击
    if (p_unit.attack_cooldown <= 0) {
        _execute_attack(p_unit, target);
        p_unit.attack_cooldown = p_unit.stats->get_attack_interval();
    }
}

// 索敌逻辑：使用 UnitManager 的网格查询
bool AttackManager::_try_find_target(UnitManager::UnitData& p_unit) {
    float range = p_unit.stats->get_aggro_range();
    std::vector<int> nearby = unit_manager->get_nearby_units(p_unit.position, range);

    int best_target = -1;
    float min_dist_sq = range * range;

    for (int idx : nearby) {
        UnitManager::UnitData& other = unit_manager->units[idx];

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
        return true;
    }
    return false;
}

void AttackManager::_execute_attack(UnitManager::UnitData& attacker, UnitManager::UnitData& defender) {
    // 简单的伤害计算
    float dmg = attacker.stats->get_attack_damage();
    // 这里可以加减防逻辑: dmg -= defender.stats->get_armor()...

    defender.current_health -= dmg;

    // 简单的反击AI：如果不动且被打，就打回去
    if (defender.state == UnitManager::IDLE && defender.target_id == -1) {
        defender.target_id = attacker.id;
        defender.state = UnitManager::CHASING;
    }
}