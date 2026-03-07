#pragma once
#include "attack_manager.h"
#include "unit_stats.h" 
#include "projectile_manager.h"
#include <godot_cpp/variant/utility_functions.hpp> 
#include <godot_cpp/classes/engine.hpp>
#include <algorithm>

using namespace godot;

void AttackManager::_bind_methods() {
        ClassDB::bind_method(D_METHOD("set_projectile_manager", "p_proj_manager"), &AttackManager::set_projectile_manager);
        ClassDB::bind_method(D_METHOD("set_building_manager", "p_bmanager"), &AttackManager::set_building_manager);
        ClassDB::bind_method(D_METHOD("setup", "p_manager"), &AttackManager::setup);
}

AttackManager::AttackManager() {}
AttackManager::~AttackManager() {}

void AttackManager::setup(UnitManager* p_manager) {
    unit_manager = p_manager;
}

void AttackManager::set_projectile_manager(ProjectileManager* p_proj_manager) {
    projectile_manager = p_proj_manager;
}

void AttackManager::set_building_manager(BuildingManager* p_bmanager) {
    building_manager = p_bmanager;
}

bool AttackManager::_get_target_info(int p_target_id, bool p_is_building, Vector2& out_pos, float& out_radius) {
    if (p_is_building) {
        if (!building_manager) return false;
        auto it = building_manager->buildings.find(p_target_id);
        if (it == building_manager->buildings.end()) return false;

        BuildingData& b = it->second;
        Vector2 cell_sz = Vector2(256.0f, 256.0f);
        if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
        Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;

        out_pos = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
        out_radius = std::max(fp_size.x, fp_size.y) * 0.5f;
        return true;
    }
    else {
        int idx = unit_manager->get_unit_index_by_id(p_target_id);
        if (idx == -1) return false;
        UnitData& u = unit_manager->units[idx];
        out_pos = u.position;
        out_radius = u.stats->get_collision_radius();
        return true;
    }
}

void AttackManager::update_units(double p_delta) {
    if (!unit_manager) return;

    for (int i = 0; i < unit_manager->units.size(); ++i) {
        UnitData& unit = unit_manager->units[i];

        switch (unit.state) {
        case IDLE:      _handle_idle(unit); break;
        case CHASING:   _handle_chasing(unit); break;
        case ATTACKING: _handle_attacking(unit, p_delta); break;
        case MOVING:    _handle_moving(unit); break;
        case PATROLLING: _handle_patrolling(unit); break;
        }
    }
}

void AttackManager::update_buildings(double p_delta) {
    if (!building_manager) return;

    for (auto& pair : building_manager->buildings) {
        BuildingData& b = pair.second;

        // 如果建筑没建好，或已经死亡，或者没有攻击力，则跳过
        if (b.state == BuildingState::BUILDING || b.current_health <= 0) continue;
        if (b.stats->get_attack_damage() <= 0 || b.stats->get_attack_range() <= 0) continue;

        if (b.attack_cooldown > 0) b.attack_cooldown -= p_delta;

        if (b.target_id != -1) {
            if (!_is_target_valid(b.target_id, b.target_is_building)) {
                b.target_id = -1;
            }
            else {
                Vector2 target_pos;
                float target_radius;
                _get_target_info(b.target_id, b.target_is_building, target_pos, target_radius);

                // 计算建筑中心点
                Vector2 cell_sz = Vector2(256.0f, 256.0f);
                if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
                Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
                Vector2 b_center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;

                float dist_sq = b_center.distance_squared_to(target_pos);
                float b_radius = std::max(fp_size.x, fp_size.y) * 0.5f; // 建筑等效半径
                float atk_range = b.stats->get_attack_range() + b_radius + target_radius;

                if (dist_sq > atk_range * atk_range) {
                    b.target_id = -1; // 跑出范围，丢失目标
                }
                else if (b.attack_cooldown <= 0) {
                    _execute_building_attack(b, b.target_id, b.target_is_building);
                    b.attack_cooldown = b.stats->get_attack_interval();
                }
            }
        }

        // 如果没有目标，尝试索敌
        if (b.target_id == -1) {
            _try_find_target_for_building(b);
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
bool AttackManager::_is_target_valid(int p_target_id, bool p_is_building) {
    if (p_is_building) {
        if (!building_manager) return false;
        auto it = building_manager->buildings.find(p_target_id);
        if (it == building_manager->buildings.end()) return false;
        return it->second.current_health > 0 && it->second.state != BuildingState::BUILDING;
    }
    else {
        int idx = unit_manager->get_unit_index_by_id(p_target_id);
        if (idx == -1) return false;
        return unit_manager->units[idx].current_health > 0;
    }
}

void AttackManager::_handle_idle(UnitData& p_unit) {
    // 空闲时尝试索敌（_try_find_target 会寻找 aggro_range 范围内的最近目标）
    if (_try_find_target(p_unit)) {

        Vector2 target_pos;
        float target_radius;

        // 使用通用的 _get_target_info，完美兼容目标是建筑还是单位
        if (_get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius)) {

            float dist_sq = p_unit.position.distance_squared_to(target_pos);
            // 计算实际的攻击距离（基础攻击距离 + 双方碰撞体积）
            float max_attack_range = 0.0f;
            if (!p_unit.stats->weapons.empty()) {
                for (const auto& weapon : p_unit.stats->weapons) {
                    if (weapon.attack_range > max_attack_range) {
                        max_attack_range = weapon.attack_range;
                    }
                }
            }
            float atk_range = max_attack_range + p_unit.stats->get_collision_radius() + target_radius;

            if (dist_sq <= atk_range * atk_range) {
                // 如果敌人在攻击范围内，直接进入攻击状态
                p_unit.state = ATTACKING;
                UtilityFunctions::print("[Debug Attack] unit ", p_unit.id, " into attack range,changing ATTACKING。目标 ID: ", p_unit.target_id);
            }
            else {
                // 如果虽然在警戒范围内，但超出了攻击范围
                // 因为是 IDLE 状态“不会主动追击”，所以当做没看见，清除目标并保持 IDLE
                // （如果你希望单位主动追击，这里可以改成 p_unit.state = CHASING;）
                p_unit.target_id = -1;
                p_unit.state = IDLE;
            }
        }
        else {
            // 获取目标信息失败（可能目标刚死亡）
            p_unit.target_id = -1;
            p_unit.state = IDLE;
        }
    }
}

void AttackManager::_handle_chasing(UnitData& p_unit) {
    if (!_is_target_valid(p_unit.target_id, p_unit.target_is_building)) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
        return;
    }

    Vector2 target_pos;
    float target_radius;
    _get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius);

    p_unit.target_pos = target_pos;
    float dist_sq = p_unit.position.distance_squared_to(target_pos);

    // 1. 获取所有武器中的最大射程
    float max_attack_range = 0.0f;
    if (p_unit.stats.is_valid() && !p_unit.stats->weapons.empty()) {
        for (const auto& weapon : p_unit.stats->weapons) {
            if (weapon.attack_range > max_attack_range) {
                max_attack_range = weapon.attack_range;
            }
        }
    }
    float atk_range = max_attack_range + p_unit.stats->get_collision_radius() + target_radius;

    // 2. 如果进入了最大射程
    if (dist_sq <= atk_range * atk_range) {
        if (p_unit.stats->get_can_fire_on_move()) {
            // 【重写：多武器系统的移动射击逻辑】
            for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
                const WeaponStats& weapon = p_unit.stats->weapons[i];
                float real_weapon_range = weapon.attack_range + p_unit.stats->get_collision_radius() + target_radius;

                // 检查这把特定武器是否够得着
                if (dist_sq <= real_weapon_range * real_weapon_range) {
                    // 检查这把特定武器是否冷却完毕
                    if (p_unit.weapon_cooldowns[i] <= 0.0f) {

                        // 生成投射物并注入专属伤害
                        if (projectile_manager) {
                            float start_height = p_unit.stats->base_height + 5.0f;
                            projectile_manager->spawn_projectile(
                                weapon.projectile_type_name,
                                p_unit.position, start_height,
                                p_unit.target_id, p_unit.target_is_building, 0.0f,
                                p_unit.id, false,
                                weapon.damage
                            );
                        }

                        // 独立重置这把武器的冷却时间
                        p_unit.weapon_cooldowns[i] = weapon.attack_interval;
                    }
                }
            }
        }
        else {
            // 不能移动射击，停下脚步切换到 ATTACKING 状态
            p_unit.state = ATTACKING;
        }
    }
    // 3. 脱战逻辑 (如果目标跑出了仇恨范围的2倍距离)
    else if (!p_unit.is_manual_target && dist_sq > p_unit.stats->get_aggro_range() * p_unit.stats->get_aggro_range() * 4.0f) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
    }
}

void AttackManager::_handle_attacking(UnitData& p_unit, double p_delta) {
    if (p_unit.target_id == -1) {
        p_unit.state = IDLE;
        return;
    }

    Vector2 target_pos;
    float target_radius;
    bool target_alive = _get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius);

    if (!target_alive) {
        p_unit.target_id = -1;
        p_unit.state = IDLE;
        return;
    }

    float distance = p_unit.position.distance_to(target_pos);
    bool is_in_any_range = false;

    // 遍历该单位挂载的所有武器
    if (p_unit.stats.is_valid()) {
        for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
            const WeaponStats& weapon = p_unit.stats->weapons[i];

            // 计算这把武器的实际攻击距离（武器基础射程 + 双方碰撞半径）
            float real_range = weapon.attack_range + p_unit.stats->get_collision_radius() + target_radius;

            // 独立扣减这把武器的冷却时间
            if (p_unit.weapon_cooldowns[i] > 0.0f) {
                p_unit.weapon_cooldowns[i] -= p_delta;
            }

            // 判断目标是否在这把武器的射程内
            if (distance <= real_range) {
                is_in_any_range = true; 

                // 检查这把武器是否冷却完毕
                if (p_unit.weapon_cooldowns[i] <= 0.0f) {
                    _execute_attack(p_unit, p_unit.target_id, p_unit.target_is_building, weapon);
                    p_unit.weapon_cooldowns[i] = weapon.attack_interval;
                }
            }
        }
    }
    if (!is_in_any_range) {
        p_unit.state = CHASING;
    }
}

void AttackManager::_handle_patrolling(UnitData& p_unit) {
    // 1. 巡逻时主动寻找敌人 (这里的 _try_find_target 已经能自动处理多武器最大射程)
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
    // 只有允许移动射击的单位，在纯移动状态下才会开火
    if (p_unit.stats.is_valid() && p_unit.stats->get_can_fire_on_move()) {
        if (_try_find_target(p_unit)) {
            Vector2 target_pos;
            float target_radius;
            if (_get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius)) {
                float dist_sq = p_unit.position.distance_squared_to(target_pos);

                // 遍历所有武器进行移动射击判定
                for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
                    const WeaponStats& weapon = p_unit.stats->weapons[i];
                    float real_weapon_range = weapon.attack_range + p_unit.stats->get_collision_radius() + target_radius;

                    // 检查这把特定武器是否够得着
                    if (dist_sq <= real_weapon_range * real_weapon_range) {

                        // 移动状态下，冷却时间的扣减通常在全局更新里，这里只负责判断是否冷却完毕
                        if (p_unit.weapon_cooldowns[i] <= 0.0f) {
                            _execute_attack(p_unit, p_unit.target_id, p_unit.target_is_building, weapon);
                            p_unit.weapon_cooldowns[i] = weapon.attack_interval;
                        }
                    }
                }
            }
        }
    }
}

// 索敌逻辑：使用 UnitManager 的网格查询
bool AttackManager::_try_find_target(UnitData& p_unit) {
    if (!p_unit.stats.is_valid() || p_unit.stats->weapons.empty()) {
        return false;
    }

    float max_weapon_range = 0.0f;
    for (const auto& weapon : p_unit.stats->weapons) {
        if (weapon.attack_range > max_weapon_range) {
            max_weapon_range = weapon.attack_range;
        }
    }

    float range = std::max(p_unit.stats->get_aggro_range(), max_weapon_range);
    float min_dist_sq = range * range;

    int best_target_id = -1;
    bool best_is_building = false;

    // 1. 查找敌方单位
    std::vector<int> nearby_units = unit_manager->get_nearby_units(p_unit.position, range);
    for (int idx : nearby_units) {
        UnitData& other = unit_manager->units[idx];
        if (other.id == p_unit.id || other.team_id == p_unit.team_id || other.current_health <= 0) continue;

        float d = p_unit.position.distance_squared_to(other.position);
        if (d < min_dist_sq) {
            min_dist_sq = d;
            best_target_id = other.id;
            best_is_building = false;
        }
    }

    // 2. 查找敌方建筑 
    if (building_manager) {
        Vector2 cell_sz = Vector2(256.0f, 256.0f);
        if (building_manager->flow_field_manager) {
            cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
        }

        for (auto& pair : building_manager->buildings) {
            BuildingData& b = pair.second;
            if (b.team_id == p_unit.team_id || b.current_health <= 0 || b.state == BuildingState::BUILDING) continue;

            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 b_center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;

            float d = p_unit.position.distance_squared_to(b_center);
            if (d < min_dist_sq) { // 优先攻击更近的建筑
                min_dist_sq = d;
                best_target_id = b.id;
                best_is_building = true;
            }
        }
    }

    // 3. 赋值最优目标
    if (best_target_id != -1) {
        p_unit.target_id = best_target_id;
        p_unit.target_is_building = best_is_building;
        p_unit.is_manual_target = false;
        return true;
    }

    return false;
}

bool AttackManager::_try_find_target_for_building(BuildingData& p_building) {
    float range = p_building.stats->get_attack_range();
    float min_dist_sq = range * range;
    int best_target_id = -1;

    Vector2 cell_sz = Vector2(256.0f, 256.0f);;
    if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
    Vector2 fp_size = Vector2(p_building.stats->get_footprint()) * cell_sz;
    Vector2 b_center = Vector2(p_building.grid_pos) * cell_sz + fp_size * 0.5f;

    // 防御塔目前一般只索敌单位
    std::vector<int> nearby_units = unit_manager->get_nearby_units(b_center, range);
    for (int idx : nearby_units) {
        UnitData& other = unit_manager->units[idx];
        if (other.team_id == p_building.team_id || other.current_health <= 0) continue;

        float d = b_center.distance_squared_to(other.position);
        if (d < min_dist_sq) {
            min_dist_sq = d;
            best_target_id = other.id;
        }
    }

    if (best_target_id != -1) {
        p_building.target_id = best_target_id;
        p_building.target_is_building = false; // 塔优先打单位
        return true;
    }
    return false;
}

void AttackManager::_execute_attack(UnitData& attacker, int target_id, bool target_is_building, const WeaponStats& weapon) {
    float dmg = weapon.damage;
    float proj_speed = weapon.projectile_speed;
    float splash = weapon.splash_radius;

    Vector2 target_pos;
    float target_radius;
    _get_target_info(target_id, target_is_building, target_pos, target_radius);

    // 激光/即时命中逻辑 (Hitscan)
    if (proj_speed <= 0.0f || proj_speed > 5000.0f) {
        if (splash > 0.0f) {
            apply_aoe_damage(target_pos, splash, dmg, attacker.id, false);
        }
        else {
            apply_damage(target_id, target_is_building, dmg, attacker.id, false);
        }
    }
    // 实体投射物逻辑
    else {
        if (projectile_manager) {
            // 1. 动态计算攻击者的发射高度 (地形高度 + 炮口相对高度)
            float start_height = attacker.height + attacker.stats->get_base_height();
            float target_height = 0.0f;

            // 2. 动态获取目标的受击高度
            if (target_is_building) {
                if (building_manager) {
                    auto b_it = building_manager->buildings.find(target_id);
                    if (b_it != building_manager->buildings.end()) {
                        // 获取建筑真实的受击高度 (通常是模型高度的一半)
                        target_height = b_it->second.stats->get_base_height() * 0.5f;
                    }
                }
            }
            else {
                int t_idx = unit_manager->get_unit_index_by_id(target_id);
                if (t_idx != -1) {
                    UnitData& tu = unit_manager->units[t_idx];
                    // 瞄准敌方单位的半身位置 (地形高度 + 身高的一半)
                    target_height = tu.height + (tu.stats->get_base_height() * 0.5f);
                }
            }

            // 3. 生成投射物 (保持你原本的 13个 参数不变，注入当前武器的具体数值)
            projectile_manager->spawn_projectile(
                weapon.projectile_type_name, 
                attacker.position,           
                start_height,               
                target_id,                  
                target_is_building,     
                target_height,            
                attacker.id,                 
                false,                      
                dmg                         
            );

            UtilityFunctions::print("fire! target:", target_id, " dmg:", dmg);
        }
    }
}

void AttackManager::_execute_building_attack(BuildingData& attacker, int target_id, bool target_is_building) {
    float dmg = attacker.stats->get_attack_damage();
    float proj_speed = attacker.stats->get_projectile_speed(); // 获取建筑的投射物速度
    float splash = attacker.stats->get_splash_radius();        // 获取建筑的溅射范围

    Vector2 target_pos;
    float target_radius;
    _get_target_info(target_id, target_is_building, target_pos, target_radius);

    if (proj_speed <= 0.0f || proj_speed > 5000.0f) {
        // 瞬间命中
        if (splash > 0.0f) {
            apply_aoe_damage(target_pos, splash, dmg, attacker.id, true);
        }
        else {
            apply_damage(target_id, target_is_building, dmg, attacker.id, true);
        }
    }
    else {
        // 建筑发射投射物
        if (projectile_manager) {
            // 炮塔发射点高度设为建筑的 base_height（例如塔顶）
            float start_height = attacker.stats->get_base_height();
            float target_height = 0.0f;

            if (target_is_building) {
                auto b_it = building_manager->buildings.find(target_id);
                if (b_it != building_manager->buildings.end()) {
                    target_height = b_it->second.stats->get_base_height() * 0.5f;
                }
            }
            else {
                int t_idx = unit_manager->get_unit_index_by_id(target_id);
                if (t_idx != -1) {
                    UnitData& tu = unit_manager->units[t_idx];
                    target_height = tu.height + (tu.stats->get_base_height() * 0.5f);
                }
            }

            // --- 动态计算 256x256 网格下的发射坐标 ---
            Vector2 cell_sz = Vector2(256.0f, 256.0f);

            if (building_manager && building_manager->flow_field_manager) {
                cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
            }

            // 获取建筑的占地网格数 (例如 1x1, 2x2)
            Vector2 fp_size = Vector2(attacker.stats->get_footprint()) * cell_sz;

            // 发射中心点 = 起点 + 宽高的一半
            Vector2 spawn_pos = Vector2(attacker.grid_pos) * cell_sz + fp_size * 0.5f;

            // 注意：这里需要传入一个子弹名字。如果你在 BuildingStats 里增加了这个属性，可以用 attacker.stats->get_projectile_type_name()
            // 如果没有，你可以先硬编码一个你在 txt 里注册过的子弹名字，比如 "Shell" 或 "Bullet"
            String building_projectile_type = "Shell";

            projectile_manager->spawn_projectile(
                building_projectile_type, 
                spawn_pos,                
                start_height,           
                target_id,                
                target_is_building,      
                target_height,            
                attacker.id,              
                true,                     
                dmg                      
            );

            UtilityFunctions::print("Building fire! target:", target_id, " dmg:", dmg);
        }
    }
}

void AttackManager::apply_damage(int target_id, bool is_building, float damage, int attacker_id, bool attacker_is_building) {
    // Check if the function is successfully called
    godot::UtilityFunctions::print(">>> [Apply Damage] Triggered! Attacker ID: ", attacker_id, " Target ID: ", target_id, " Damage: ", damage, " Is Building: ", is_building);

    if (is_building) {
        // 1. Target is a building
        if (!building_manager) {
            godot::UtilityFunctions::printerr(">>> [Apply Damage Failed] FATAL: building_manager is null! (Check setup/bind_methods)");
            return;
        }

        auto it = building_manager->buildings.find(target_id);
        if (it != building_manager->buildings.end()) {
            float old_health = it->second.current_health;
            it->second.current_health -= damage;
            godot::UtilityFunctions::print(">>> [Apply Damage Success] Building hit! ID: ", target_id, " Health: ", old_health, " -> ", it->second.current_health);
        }
        else {
            godot::UtilityFunctions::printerr(">>> [Apply Damage Failed] Cannot find building ID: ", target_id);
        }
    }
    else {
        // 2. Target is a unit
        if (!unit_manager) {
            godot::UtilityFunctions::printerr(">>> [Apply Damage Failed] FATAL: unit_manager is null! (Check setup/bind_methods)");
            return;
        }

        int target_idx = unit_manager->get_unit_index_by_id(target_id);
        if (target_idx == -1) {
            godot::UtilityFunctions::printerr(">>> [Apply Damage Failed] Cannot find unit ID: ", target_id, " (Might be dead and cleared)");
            return;
        }

        UnitData& defender = unit_manager->units[target_idx];
        if (defender.current_health <= 0) {
            godot::UtilityFunctions::print(">>> [Apply Damage Invalid] Target unit is already dead. ID: ", target_id);
            return;
        }

        float old_health = defender.current_health;
        defender.current_health -= damage;
        godot::UtilityFunctions::print(">>> [Apply Damage Success] Unit hit! ID: ", target_id, " Health: ", old_health, " -> ", defender.current_health);
    }
}

void AttackManager::apply_aoe_damage(Vector2 p_epicenter, float p_radius, float p_damage, int p_attacker_id, bool attacker_is_building) {
    // 这里以查找攻击者队伍ID避免误伤为例
    int attacker_team = -1;
    if (attacker_is_building && building_manager) {
        auto it = building_manager->buildings.find(p_attacker_id);
        if (it != building_manager->buildings.end()) attacker_team = it->second.team_id;
    }
    else {
        int attacker_idx = unit_manager->get_unit_index_by_id(p_attacker_id);
        if (attacker_idx != -1) attacker_team = unit_manager->units[attacker_idx].team_id;
    }

    // 1. 对范围内单位造成伤害
    std::vector<int> nearby_units = unit_manager->get_nearby_units(p_epicenter, p_radius);
    for (int target_idx : nearby_units) {
        UnitData& target = unit_manager->units[target_idx];
        if (target.current_health <= 0) continue;
        if (attacker_team != -1 && target.team_id == attacker_team) continue;

        float dist_sq = p_epicenter.distance_squared_to(target.position);
        float real_radius = p_radius + target.stats->get_collision_radius();

        if (dist_sq <= real_radius * real_radius) {
            target.current_health -= p_damage;
            if (target.state == IDLE && target.target_id == -1 && p_attacker_id != -1) {
                target.target_id = p_attacker_id;
                target.target_is_building = attacker_is_building;
                target.state = CHASING;
            }
        }
    }

    // 2. 对范围内建筑造成伤害
    if (building_manager) {
        Vector2 cell_sz = Vector2(256.0f, 256.0f);
        if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());

        for (auto& pair : building_manager->buildings) {
            BuildingData& b = pair.second;
            if (b.current_health <= 0 || (attacker_team != -1 && b.team_id == attacker_team)) continue;

            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 b_center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
            float b_radius = std::max(fp_size.x, fp_size.y) * 0.5f;

            float dist_sq = p_epicenter.distance_squared_to(b_center);
            float real_radius = p_radius + b_radius;

            if (dist_sq <= real_radius * real_radius) {
                b.current_health -= p_damage;
            }
        }
    }
}

