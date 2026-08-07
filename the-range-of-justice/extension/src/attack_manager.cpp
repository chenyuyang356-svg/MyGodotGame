#include "attack_manager.h"
#include "unit_stats.h" 
#include "projectile_manager.h"
#include <godot_cpp/variant/utility_functions.hpp> 
#include <godot_cpp/classes/engine.hpp>
#include <algorithm>

using namespace godot;

// GODOT绑定与初始化
void AttackManager::_bind_methods() {
        ClassDB::bind_method(D_METHOD("set_projectile_manager", "p_proj_manager"), &AttackManager::set_projectile_manager);
        ClassDB::bind_method(D_METHOD("set_building_manager", "p_bmanager"), &AttackManager::set_building_manager);
        ClassDB::bind_method(D_METHOD("setup", "p_manager"), &AttackManager::setup);

        ADD_SIGNAL(MethodInfo("spawn_projectile_requested", PropertyInfo(Variant::STRING, "type_name"), PropertyInfo(Variant::VECTOR2, "start_pos"),
            PropertyInfo(Variant::FLOAT, "start_height"), PropertyInfo(Variant::INT, "target_id"), PropertyInfo(Variant::BOOL, "target_is_building"),
            PropertyInfo(Variant::FLOAT, "target_height"), PropertyInfo(Variant::INT, "source_id"), PropertyInfo(Variant::BOOL, "source_is_building"),
            PropertyInfo(Variant::FLOAT, "weapon_damage")));
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
        // 默认网格大小是256*256像素
        Vector2 cell_sz = Vector2(256.0f, 256.0f);
        if (building_manager->flow_field_manager) cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());

        // 计算建筑的实际像素尺寸：占地格子数 * 单个格子大小
        Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;

        // 建筑的中心点坐标 = 网格左上角坐标 + 一半的长宽
        out_pos = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;

        // 使用建筑长宽的最大值的一半作为等效碰撞半径
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

bool AttackManager::_is_target_full_health(int p_target_id, bool p_is_building) {
    if (p_is_building) {
        if (!building_manager) return true;
        auto it = building_manager->buildings.find(p_target_id);
        if (it == building_manager->buildings.end()) return true;
        return it->second.current_health >= it->second.stats->get_health_max();
    }
    else {
        int idx = unit_manager->get_unit_index_by_id(p_target_id);
        if (idx == -1) return true;
        UnitData& u = unit_manager->units[idx];
        return u.current_health >= u.stats->get_health_max();
    }
}

float AttackManager::_get_unit_max_attack_range(const UnitData& p_unit) {
    float max_range = 0.0f;

    // 1. 检查固定武器 (body weapons)
    if (p_unit.stats.is_valid()) {
        for (const auto& w : p_unit.stats->weapons) {
            max_range = std::max(max_range, w.attack_range);
        }
    }

    // 2. 检查独立武器 (turrets/WeaponData)
    for (const auto& wd : p_unit.weapons) {
        if (wd.stats.is_valid()) {
            max_range = std::max(max_range, wd.stats->get_attack_range());
        }
    }

    return max_range;
}

void AttackManager::update_units(double p_delta) {
    if (!unit_manager) return;

    for (int i = 0; i < unit_manager->units.size(); ++i) {
        UnitData& unit = unit_manager->units[i];

        if (unit.stats.is_valid()) {
            for (size_t w = 0; w < unit.weapon_cooldowns.size(); ++w) {
                if (unit.weapon_cooldowns[w] > 0.0f) {
                    unit.weapon_cooldowns[w] -= (float)p_delta;
                }
            }
        }

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
        if (b.state == BuildingState::BUILDING || b.state == BuildingState::DYING || b.current_health <= 0) continue;

        // 建筑现在遍历自己的武器列表
        if (b.target_id != -1) {
            if (!_is_target_valid(b.target_id, b.target_is_building)) {
                _reset_building_targets(b);
            }
            else {
                Vector2 t_pos; float t_rad;
                _get_target_info(b.target_id, b.target_is_building, t_pos, t_rad);
                // 确定目标是否为飞行单位
                bool target_is_air = _is_air_target(b.target_id, b.target_is_building);

                Vector2 cell_sz = Vector2(building_manager->get_cell_size());
                Vector2 b_center = Vector2(b.grid_pos) * cell_sz + (Vector2(b.stats->get_footprint()) * cell_sz * 0.5f);
                float dist = b_center.distance_to(t_pos);

                for (WeaponData& wd : b.weapons) {
                    if (wd.stats.is_null()) continue;

                    // 新增判断：武器是否能攻击该高度的目标
                    bool can_hit = target_is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground();
                    if (!can_hit) continue;

                    wd.target_direction = (t_pos - b_center).normalized();
                    float real_range = wd.stats->get_attack_range() + t_rad + (std::max(b.stats->get_footprint().x, b.stats->get_footprint().y) * cell_sz.x * 0.5f);

                    if (dist <= real_range) {
                        _execute_weapon_data_attack(b_center, 0.0f, 0.0f, b.id, true, b.target_id, b.target_is_building, wd);
                    }
                    else {
                        wd.target_id = -1;
                    }
                }
            }
        }

        if (b.target_id == -1) _try_find_target_for_building(b);
    }
}

// 单位驱动力统一由unit_manager，这里只计算次要的力
bool AttackManager::try_get_combat_force(UnitData& p_unit, Vector2& out_force) {
    if (p_unit.state == CHASING || p_unit.state == PATROLLING) {
        // 移除这里的 steering。让 UnitManager 通过 p_unit.target_pos 自动计算 flow 和动力。
        // 只保留排斥力，或者干脆返回 false，让 UnitManager 统一处理。
        out_force = Vector2(0, 0);
        return false;
    }
    /*
    else if (p_unit.state == ATTACKING) {
        // 攻击时施加摩擦力使其停稳
        out_force = unit_manager->get_friction(p_unit) * unit_manager->get_friction_factor() * 2.0f;
        return true;
    }
    */
    return false;
}

// 检查目标是否存在且活着
bool AttackManager::_is_target_valid(int p_target_id, bool p_is_building) {
    if (p_is_building) {
        if (!building_manager) return false;
        auto it = building_manager->buildings.find(p_target_id);
        if (it == building_manager->buildings.end()) return false;
        return it->second.current_health > 0 && it->second.state != BuildingState::DYING;
    }
    else {
        int idx = unit_manager->get_unit_index_by_id(p_target_id);
        if (idx == -1) return false;
        return unit_manager->units[idx].current_health > 0;
    }
}

void AttackManager::_handle_idle(UnitData& p_unit) {
    // 空闲时尝试索敌
    if (_try_find_target(p_unit)) {
        Vector2 t_pos; float t_rad;
        if (_get_target_info(p_unit.target_id, p_unit.target_is_building, t_pos, t_rad)) {
            float dist = p_unit.position.distance_to(t_pos);
            float real_atk_range = _get_unit_max_attack_range(p_unit) + p_unit.stats->get_collision_radius() + t_rad;

            // 目标在射程内直接打，不在就去追
            p_unit.state = (dist <= real_atk_range) ? ATTACKING : CHASING;
        }
        else {
            _reset_unit_targets(p_unit);
        }
    }
}

void AttackManager::_handle_chasing(UnitData& p_unit) {
    if (!_is_target_valid(p_unit.target_id, p_unit.target_is_building)) {
        _reset_unit_targets(p_unit); // 目标失效，重置武器
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
        return;
    }

    Vector2 target_pos; float target_radius;
    _get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius);
    bool target_is_air = _is_air_target(p_unit.target_id, p_unit.target_is_building);

    p_unit.target_pos = target_pos;
    float dist_sq = p_unit.position.distance_squared_to(target_pos);

    // 使用综合射程
    float max_range = _get_unit_max_attack_range(p_unit);
    float atk_range = max_range + p_unit.stats->get_collision_radius() + target_radius;
    float comfortable_atk_range = atk_range * 0.85f;

    if (dist_sq <= comfortable_atk_range * comfortable_atk_range) {
        p_unit.state = ATTACKING;
    }
    else if (dist_sq <= atk_range * atk_range) {
        // 在边缘射程：如果能移动射击，就边走边打；如果不能，立即停下开火
        if (p_unit.stats->get_can_fire_on_move()) {
            // A. 固定武器
            for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
                const Weapon& weapon = p_unit.stats->weapons[i];
                // 新增高度判断
                bool can_hit = target_is_air ? weapon.can_attack_air : weapon.can_attack_ground;
                if (!can_hit) continue;

                float real_range = weapon.attack_range + p_unit.stats->get_collision_radius() + target_radius;
                if (dist_sq <= real_range * real_range && p_unit.weapon_cooldowns[i] <= 0.0f) {
                    _execute_attack(p_unit, p_unit.target_id, p_unit.target_is_building, weapon);
                    p_unit.weapon_cooldowns[i] = weapon.attack_interval;
                }
            }

            // B. 独立武器
            for (auto& wd : p_unit.weapons) {
                if (wd.stats.is_null()) continue;
                // 新增高度判断
                bool can_hit = target_is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground();
                if (!can_hit) continue;

                float real_range = wd.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target_radius;
                if (dist_sq <= real_range * real_range) {
                    _execute_weapon_data_attack(p_unit.position, p_unit.height, p_unit.rotation, p_unit.id, false, p_unit.target_id, p_unit.target_is_building, wd);
                }
                else {
                    wd.target_id = -1;
                }
            }
        }
        else {
            p_unit.state = ATTACKING;
        }
    }

    // 3. 当敌方单位跑出了目标射程的 2 倍则脱战
    else if (!p_unit.is_manual_target && dist_sq > p_unit.stats->get_aggro_range() * p_unit.stats->get_aggro_range() * 4.0f) {
        p_unit.target_id = -1;
        p_unit.state = p_unit.is_patrolling ? PATROLLING : IDLE;
    }
}

void AttackManager::_handle_attacking(UnitData& p_unit, double p_delta) {
    if (p_unit.target_id == -1 || !_is_target_valid(p_unit.target_id, p_unit.target_is_building)) {
        _reset_unit_targets(p_unit); // 关键：清除武器目标
        if (_try_find_target(p_unit)) {
            p_unit.state = CHASING;
        }
        else {
            p_unit.state = IDLE;
        }
        return;
    }

    // --- 建造者逻辑 ---
    if (p_unit.stats->get_unit_tags() & TAG_BUILDER) {
        if (_is_target_full_health(p_unit.target_id, p_unit.target_is_building)) {
            p_unit.target_id = -1;
            p_unit.state = IDLE;
            return;
        }
    }

    Vector2 target_pos; float target_radius;
    if (!_get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius)) return;
    bool target_is_air = _is_air_target(p_unit.target_id, p_unit.target_is_building);

    float dist = p_unit.position.distance_to(target_pos);
    Vector2 dir_to_target = (target_pos - p_unit.position).normalized();
    float angle_to_target = dir_to_target.angle();

    bool is_in_any_range = false;
    bool needs_body_rotation = false;

    // --- A. 处理独立武器 ---
    for (auto& wd : p_unit.weapons) {
        if (wd.stats.is_null()) continue;
        // 新增高度判断
        bool can_hit = target_is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground();
        if (!can_hit) continue;

        float real_range = wd.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target_radius;
        if (dist <= real_range) {
            is_in_any_range = true;
            _execute_weapon_data_attack(p_unit.position, p_unit.height, p_unit.rotation, p_unit.id,
                false, p_unit.target_id, p_unit.target_is_building, wd);
        }
        else {
            wd.target_id = -1;
        }
    }

    // --- B. 处理非独立武器 ---
    for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
        const auto& w = p_unit.stats->weapons[i];
        // 新增高度判断
        bool can_hit = target_is_air ? w.can_attack_air : w.can_attack_ground;
        if (!can_hit) continue;

        float real_range = w.attack_range + p_unit.stats->get_collision_radius() + target_radius;
        if (dist <= real_range) {
            is_in_any_range = true;
            needs_body_rotation = true;
            float angle_diff = Math::abs(UtilityFunctions::angle_difference(p_unit.rotation, angle_to_target));
            if (angle_diff <= Math::deg_to_rad(w.firing_tolerance)) {
                if (p_unit.weapon_cooldowns[i] <= 0.0f) {
                    _execute_attack(p_unit, p_unit.target_id, p_unit.target_is_building, w);
                    p_unit.weapon_cooldowns[i] = w.attack_interval;
                }
            }
        }
    }

    // --- C. 单位身体旋转逻辑 ---
    // 如果处于攻击状态，且有非独立武器需要对准，或者所有武器都够不到需要追击
    if (needs_body_rotation) {
        // 设置单位的 target_pos 为当前位置稍微靠向目标的方向，迫使 UnitManager 的转向逻辑生效
        // 或者更直接地，给 UnitData 添加一个 target_rotation (需在 UnitData 结构体中添加)
        p_unit.target_rotation = angle_to_target;
    }
    else {
        p_unit.target_rotation = p_unit.rotation;
    }

    if (!is_in_any_range) {
        p_unit.state = CHASING;
    }
}

void AttackManager::_handle_patrolling(UnitData& p_unit) {
    // 1. 巡逻时主动寻找敌人，若找到敌人，则变为追击状态
    if (_try_find_target(p_unit)) {
        p_unit.state = CHASING;
        p_unit.target_pos_offset = Vector2(0, 0);
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
        // 1. 如果当前没目标，尝试寻找一个
        if (p_unit.target_id == -1 || !_is_target_valid(p_unit.target_id, p_unit.target_is_building)) {
            _try_find_target(p_unit);
        }

        // 2. 如果有了有效目标，检查是否在射程内并开火
        if (p_unit.target_id != -1) {
            Vector2 target_pos; float target_radius;
            if (_get_target_info(p_unit.target_id, p_unit.target_is_building, target_pos, target_radius)) {
                bool target_is_air = _is_air_target(p_unit.target_id, p_unit.target_is_building);
                float dist = p_unit.position.distance_to(target_pos);

                // A. 固定武器
                for (size_t i = 0; i < p_unit.stats->weapons.size(); ++i) {
                    const Weapon& weapon = p_unit.stats->weapons[i];
                    // 新增高度判断
                    bool can_hit = target_is_air ? weapon.can_attack_air : weapon.can_attack_ground;
                    if (!can_hit) continue;

                    float real_range = weapon.attack_range + p_unit.stats->get_collision_radius() + target_radius;
                    if (dist <= real_range && p_unit.weapon_cooldowns[i] <= 0.0f) {
                        _execute_attack(p_unit, p_unit.target_id, p_unit.target_is_building, weapon);
                        p_unit.weapon_cooldowns[i] = weapon.attack_interval;
                    }
                }

                // B. 独立武器
                for (auto& wd : p_unit.weapons) {
                    if (wd.stats.is_null()) continue;
                    // 新增高度判断
                    bool can_hit = target_is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground();
                    if (!can_hit) continue;

                    float real_range = wd.stats->get_attack_range() + p_unit.stats->get_collision_radius() + target_radius;
                    if (dist <= real_range) {
                        _execute_weapon_data_attack(p_unit.position, p_unit.height, p_unit.rotation, p_unit.id,
                            false, p_unit.target_id, p_unit.target_is_building, wd);
                    }
                    else {
                        wd.target_id = -1;
                    }
                }
            }
        }
    }
}

// 索敌逻辑：使用 UnitManager 的网格查询
bool AttackManager::_try_find_target(UnitData& p_unit) {
    if (!p_unit.stats.is_valid()) return false;
    // 建造者不会主动寻找敌人
    if (p_unit.stats->get_unit_tags() & TAG_BUILDER) return false;

    float range = std::max(p_unit.stats->get_aggro_range(), _get_unit_max_attack_range(p_unit));
    float min_dist_sq = range * range;
    int best_id = -1; bool best_is_b = false;

    // 检查是否有任何武器可以打该高度
    auto can_unit_attack_altitude = [&](bool is_air) {
        for (const auto& w : p_unit.stats->weapons)
            if (is_air ? w.can_attack_air : w.can_attack_ground) return true;
        for (const auto& wd : p_unit.weapons)
            if (wd.stats.is_valid() && (is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground())) return true;
        return false;
        };

    bool has_anti_ground = can_unit_attack_altitude(false);
    bool has_anti_air = can_unit_attack_altitude(true);

    // 1. 单位
    std::vector<int> nearby = unit_manager->get_nearby_units(p_unit.position, range);
    for (int idx : nearby) {
        UnitData& other = unit_manager->units[idx];
        if (other.id == p_unit.id || other.team_id == p_unit.team_id || other.current_health <= 0) continue;

        bool target_is_air = other.height > 20.0f;
        if (target_is_air ? !has_anti_air : !has_anti_ground) continue;

        float d = p_unit.position.distance_squared_to(other.position);
        if (d < min_dist_sq) { min_dist_sq = d; best_id = other.id; best_is_b = false; }
    }
    // 2. 建筑 (通常为地面)
    if (best_id == -1 && has_anti_ground && building_manager) {
        Vector2 cell_sz = Vector2(building_manager->get_cell_size());

        for (auto& pair : building_manager->buildings) {
            BuildingData& b = pair.second;
            if (b.team_id == p_unit.team_id || b.current_health <= 0) continue;

            // 计算建筑中心和半径
            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 b_center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
            float b_radius = std::max(fp_size.x, fp_size.y) * 0.5f;

            // 核心修正：计算单位到建筑边缘的距离
            float dist_to_center = p_unit.position.distance_to(b_center);
            float dist_to_edge = std::max(0.0f, dist_to_center - b_radius);

            if (dist_to_edge <= range) {
                float d_sq = dist_to_edge * dist_to_edge;
                if (d_sq < min_dist_sq) {
                    min_dist_sq = d_sq;
                    best_id = b.id;
                    best_is_b = true;
                }
            }
        }
    }

    if (best_id != -1) {
        p_unit.target_id = best_id; p_unit.target_is_building = best_is_b;
        return true;
    }
    return false;
}

bool AttackManager::_try_find_target_for_building(BuildingData& p_building) {
    if (!building_manager) return false;

    // --- 1. 预检查：获取建筑所有独立武器的综合攻击能力和最大射程 ---
    bool has_anti_ground = false;
    bool has_anti_air = false;
    float max_weapon_range = 0.0f;

    for (const auto& wd : p_building.weapons) {
        if (wd.stats.is_null()) continue;

        if (wd.stats->get_can_attack_ground()) has_anti_ground = true;
        if (wd.stats->get_can_attack_air()) has_anti_air = true;

        max_weapon_range = std::max(max_weapon_range, wd.stats->get_attack_range());
    }

    // 如果没有任何武器或者武器都不具备攻击能力，直接返回
    if (!has_anti_ground && !has_anti_air) return false;

    // 使用武器中的最大射程作为索敌范围
    float range = max_weapon_range;
    int best_target_id = -1;
    bool best_is_building = false;
    float min_dist_sq = range * range;

    // 计算攻击建筑的中心和等效半径
    Vector2 cell_sz = Vector2(building_manager->get_cell_size());
    Vector2 fp_size = Vector2(p_building.stats->get_footprint()) * cell_sz;
    Vector2 b_center = Vector2(p_building.grid_pos) * cell_sz + fp_size * 0.5f;
    float b_radius = std::max(fp_size.x, fp_size.y) * 0.5f;

    // --- 2. 第一阶段：索敌单位 (需判断空中/地面) ---
    // 搜索半径需要加上建筑自身的半径，以防大型建筑漏掉近处的单位
    std::vector<int> nearby_units = unit_manager->get_nearby_units(b_center, range + b_radius);
    for (int idx : nearby_units) {
        UnitData& other = unit_manager->units[idx];
        if (other.team_id == p_building.team_id || other.current_health <= 0) continue;

        // 判定目标是否为空中单位 (参考 _try_find_target 的逻辑)
        bool target_is_air = other.height > 20.0f;

        // 核心过滤：如果目标在空中但建筑没防空武器，或目标在地面但建筑没对地武器，则跳过
        if (target_is_air ? !has_anti_air : !has_anti_ground) continue;

        float dist = b_center.distance_to(other.position);
        float dist_to_edge = std::max(0.0f, dist - b_radius - other.stats->get_collision_radius());

        if (dist_to_edge <= range) {
            float d_sq = dist_to_edge * dist_to_edge;
            if (d_sq < min_dist_sq) {
                min_dist_sq = d_sq;
                best_target_id = other.id;
                best_is_building = false;
            }
        }
    }

    // --- 3. 第二阶段：索敌建筑 (建筑通常视为地面目标) ---
    if (best_target_id == -1 && has_anti_ground) {
        for (auto& pair : building_manager->buildings) {
            BuildingData& other_b = pair.second;
            // 排除自己、队友、已摧毁或正在建造中的建筑
            if (other_b.id == p_building.id || other_b.team_id == p_building.team_id ||
                other_b.current_health <= 0 || other_b.state == BuildingState::BUILDING) continue;

            Vector2 other_fp_size = Vector2(other_b.stats->get_footprint()) * cell_sz;
            Vector2 other_center = Vector2(other_b.grid_pos) * cell_sz + other_fp_size * 0.5f;
            float other_radius = std::max(other_fp_size.x, other_fp_size.y) * 0.5f;

            float dist = b_center.distance_to(other_center);
            float dist_to_edge = std::max(0.0f, dist - b_radius - other_radius);

            if (dist_to_edge <= range) {
                float d_sq = dist_to_edge * dist_to_edge;
                if (d_sq < min_dist_sq) {
                    min_dist_sq = d_sq;
                    best_target_id = other_b.id;
                    best_is_building = true;
                }
            }
        }
    }

    if (best_target_id != -1) {
        p_building.target_id = best_target_id;
        p_building.target_is_building = best_is_building;
        return true;
    }
    return false;
}

void AttackManager::_execute_attack(UnitData& attacker, int target_id, bool target_is_building, const Weapon& weapon) {
    float dmg = weapon.damage;
    float proj_speed = weapon.projectile_speed;
    float splash = weapon.splash_radius;

    Vector2 target_pos;
    float target_radius;
    if (!_get_target_info(target_id, target_is_building, target_pos, target_radius)) return;

    // --- 新增：朝向检查 ---
    Vector2 dir_to_target = (target_pos - attacker.position).normalized();
    float angle_to_target = dir_to_target.angle();
    float angle_diff = Math::abs(UtilityFunctions::angle_difference(attacker.rotation, angle_to_target));

    // 如果角度偏差大于容差，则不发射
    if (angle_diff > Math::deg_to_rad(weapon.firing_tolerance)) {
        return;
    }

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
            float start_height = attacker.height;
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

            Vector2 horizontal_offset = Vector2(weapon.spawn_offset.x, weapon.spawn_offset.y).rotated(attacker.rotation);
            Vector2 final_spawn_pos = attacker.position + horizontal_offset;
            float final_start_height = attacker.height + weapon.spawn_offset.z;

            // 3. 生成投射物 (保持你原本的 13个 参数不变，注入当前武器的具体数值)
            emit_signal("spawn_projectile_requested",
                weapon.projectile_type_name, 
                final_spawn_pos,
                final_start_height,
                target_id,                  
                target_is_building,     
                target_height,            
                attacker.id,                 
                false,                      
                dmg                         
            );
        }
    }
}

void AttackManager::_execute_weapon_data_attack(Vector2 p_source_pos, float p_source_h, float p_source_rot, int p_source_id, bool p_source_is_b,
    int p_target_id, bool p_target_is_b, WeaponData& wd) {

    Vector2 t_pos; float t_rad;
    if (!_get_target_info(p_target_id, p_target_is_b, t_pos, t_rad)) {
        wd.target_id = -1;
        return;
    }

    // --- 新增：更新武器的目标信息，供 WeaponData::update 旋转使用 ---
    wd.target_id = p_target_id;

    // 计算相对于武器挂载点（Mount Point）的方向
    Vector2 mount_world_pos = p_source_pos;
    if (!p_source_is_b) mount_world_pos += wd.local_position.rotated(p_source_rot);
    else mount_world_pos += wd.local_position;

    wd.target_direction = (t_pos - mount_world_pos).normalized();

    // --- 新增：检查独立武器对准误差 ---
    float angle_to_target = wd.target_direction.angle();
    float angle_diff = Math::abs(UtilityFunctions::angle_difference(wd.rotation, angle_to_target));

    if (angle_diff > Math::deg_to_rad(wd.stats->get_firing_tolerance())) {
        return; // 还没转到位，不攻击
    }

    if (wd.try_attack()) { // 内部会重置冷却并触发动画状态
        float dmg = wd.stats->get_damage();
        float speed = wd.stats->get_projectile_speed();

        Vector2 t_pos; float t_rad;
        _get_target_info(p_target_id, p_target_is_b, t_pos, t_rad);

        if (speed <= 0.0f || speed > 5000.0f) {
            if (wd.stats->get_splash_radius() > 0.0f)
                apply_aoe_damage(t_pos, wd.stats->get_splash_radius(), dmg, p_source_id, p_source_is_b);
            else
                apply_damage(p_target_id, p_target_is_b, dmg, p_source_id, p_source_is_b);
        }
        else {
            const Ref<WeaponStats>& ws = wd.stats;
            Vector3 m_off = ws->get_muzzle_offset();

            // 1. 计算炮塔底座在世界空间的位置
            // 如果是建筑，不旋转底座偏移；如果是单位，底座偏移随单位旋转
            Vector2 mount_world_pos = p_source_pos;
            if (!p_source_is_b) {
                mount_world_pos += wd.local_position.rotated(p_source_rot);
            }
            else {
                mount_world_pos += wd.local_position;
            }

            // 2. 计算炮口在世界空间的位置 (相对于炮塔朝向旋转)
            Vector2 muzzle_horizontal = Vector2(m_off.x, m_off.y).rotated(wd.rotation);
            Vector2 final_spawn_pos = mount_world_pos + muzzle_horizontal;

            // 3. 计算高度 (来源高度 + 炮塔偏移高度)
            float final_start_height = p_source_h + m_off.z;

            emit_signal("spawn_projectile_requested",
                wd.stats->get_projectile_type_name(),
                final_spawn_pos, final_start_height,
                p_target_id, p_target_is_b, 0.0f,
                p_source_id, p_source_is_b,
                dmg
            );
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
        // 投射物速度设置在这个范围时，默认为瞬间命中
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

            // 动态计算 256x256 网格下的发射坐标
            Vector2 cell_sz = Vector2(256.0f, 256.0f);

            if (building_manager && building_manager->flow_field_manager) {
                cell_sz = Vector2(building_manager->flow_field_manager->get_cell_size());
            }

            // 获取建筑的占地网格数 
            Vector2 fp_size = Vector2(attacker.stats->get_footprint()) * cell_sz;

            // 发射中心点 = 起点 + 宽高的一半
            Vector2 spawn_pos = Vector2(attacker.grid_pos) * cell_sz + fp_size * 0.5f;

            // 这里需要传入一个子弹名字。
            // 目前建筑攻击尚未完成，如果后续在 BuildingStats 里增加了这个属性，可以用 attacker.stats->get_projectile_type_name()
            String building_projectile_type = "Shell";

            emit_signal("spawn_projectile_requested", 
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
        }
    }
}

bool AttackManager::_process_weapons_logic(int p_id, bool p_is_b, Vector2 p_pos, float p_h, float p_rot,
    int p_tid, bool p_tis_b, std::vector<WeaponData>& p_independent_weapons,
    const std::vector<Weapon>* p_body_weapons, std::vector<float>* p_cooldowns) {

    Vector2 t_pos; float t_rad;
    if (!_get_target_info(p_tid, p_tis_b, t_pos, t_rad)) return false;

    bool target_is_air = _is_air_target(p_tid, p_tis_b);
    float dist = p_pos.distance_to(t_pos);
    bool is_any_weapon_in_range = false;

    // 1. 处理独立武器 (Turrets/WeaponData)
    for (auto& wd : p_independent_weapons) {
        if (wd.stats.is_null()) continue;

        bool can_hit_altitude = target_is_air ? wd.stats->get_can_attack_air() : wd.stats->get_can_attack_ground();
        float real_range = wd.stats->get_attack_range() + t_rad;

        if (can_hit_altitude && dist <= real_range) {
            is_any_weapon_in_range = true;
            _execute_weapon_data_attack(p_pos, p_h, p_rot, p_id, p_is_b, p_tid, p_tis_b, wd);
        }
        else {
            wd.target_id = -1; // 该武器够不着或无法攻击此高度
        }
    }

    // 2. 处理固定武器 (Body Weapons) - 仅单位
    if (p_body_weapons && p_cooldowns) {
        int u_idx = unit_manager->get_unit_index_by_id(p_id);
        if (u_idx != -1) {
            UnitData& unit = unit_manager->units[u_idx];
            for (size_t i = 0; i < p_body_weapons->size(); ++i) {
                const Weapon& w = (*p_body_weapons)[i];
                bool can_hit_altitude = target_is_air ? w.can_attack_air : w.can_attack_ground;
                float real_range = w.attack_range + t_rad + unit.stats->get_collision_radius();

                if (can_hit_altitude && dist <= real_range) {
                    is_any_weapon_in_range = true;
                    // 身体对准检查在 _execute_attack 内部
                    if ((*p_cooldowns)[i] <= 0.0f) {
                        _execute_attack(unit, p_tid, p_tis_b, w);
                        (*p_cooldowns)[i] = w.attack_interval;
                    }
                }
            }
        }
    }

    return is_any_weapon_in_range;
}

bool AttackManager::_is_air_target(int p_target_id, bool p_is_building) {
    if (p_is_building) return false;
    int idx = unit_manager->get_unit_index_by_id(p_target_id);
    if (idx == -1) return false;
    return unit_manager->units[idx].height > 20.0f;
}

bool AttackManager::_can_weapon_hit_altitude(bool p_can_ground, bool p_can_air, bool p_is_air) {
    return p_is_air ? p_can_air : p_can_ground;
}

void AttackManager::_reset_unit_targets(UnitData& p_unit) {
    p_unit.target_id = -1;
    // 重置所有独立武器的目标锁定
    for (auto& wd : p_unit.weapons) {
        wd.target_id = -1;
        wd.target_direction = Vector2(0, 0); // 或者设为单位正前方
    }
}

void AttackManager::_reset_building_targets(BuildingData& p_building) {
    p_building.target_id = -1;
    for (auto& wd : p_building.weapons) {
        wd.target_id = -1;
    }
}

void AttackManager::apply_damage(int target_id, bool is_building, float damage, int attacker_id, bool attacker_is_building) {
    if (is_building) {
        // 1. 当目标为建筑
        if (!building_manager) {
            return;
        }

        auto it = building_manager->buildings.find(target_id);
        if (it != building_manager->buildings.end()) {
            it->second.current_health -= damage;
        }
    }
    else {
        // 2. 当目标为单位
        if (!unit_manager) {
            return;
        }

        int target_idx = unit_manager->get_unit_index_by_id(target_id);
        if (target_idx == -1) {
            return;
        }

        UnitData& defender = unit_manager->units[target_idx];
        if (defender.current_health <= 0) {
            return;
        }

        defender.current_health -= damage;
    }
}

void AttackManager::apply_aoe_damage(Vector2 p_epicenter, float p_radius, float p_damage, int p_attacker_id, bool attacker_is_building) {
    // 查找攻击者ID避免误伤
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

void AttackManager::apply_healing(int target_id, bool is_building, float amount, int attacker_id) {
    if (is_building) {
        if (!building_manager) return;
        auto it = building_manager->buildings.find(target_id);
        if (it != building_manager->buildings.end()) {
            BuildingData& b = it->second;
            if (b.current_health <= 0 || b.state == BuildingState::DYING) return;

            float max_h = b.stats->get_health_max();
            b.current_health = std::min(b.current_health + amount, max_h);

            // 如果是正在建造中的建筑，且血量达到上限，则视为建造完成
            if (b.state == BuildingState::BUILDING && b.current_health >= max_h) {
                b.state = BuildingState::IDLE;
                b.build_timer = 0.0f;
                godot::UtilityFunctions::print("Building construction complete! ID: ", target_id);
            }
        }
    }
    else {
        if (!unit_manager) return;
        int idx = unit_manager->get_unit_index_by_id(target_id);
        if (idx != -1) {
            UnitData& u = unit_manager->units[idx];
            if (u.current_health <= 0) return;

            float max_h = u.stats->get_health_max();
            u.current_health = std::min(u.current_health + amount, max_h);
        }
    }
}