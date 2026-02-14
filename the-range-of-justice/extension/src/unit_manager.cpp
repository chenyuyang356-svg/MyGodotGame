#include "unit_manager.h"
#include <queue>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

UnitManager::UnitManager() {
    units.reserve(1000);
}

UnitManager::~UnitManager() {}

void UnitManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
    if (!flow_field_manager) {
        flow_field_manager = get_node<FlowFieldManager>("../FlowFieldManager");
        if (!flow_field_manager) return;
    }

    flow_field_manager->setup_grid(p_width, p_height, p_origin, p_cell_size);
    unit_grid_width = p_width / 2;
    unit_grid_height = p_height / 2;
    unit_grid_size = unit_grid_width * unit_grid_height;
    unit_grid_cell_size = p_cell_size / 2;
    unit_grid.resize(unit_grid_size);
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].reserve(10);
    }
    is_setup = true;
}

// [重点修改] 实现匹配头文件
int UnitManager::spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id) {
    if (p_stats.is_null()) {
        return -1;
    }

    UnitData new_unit;
    new_unit.id = next_unit_id++;
    new_unit.team_id = p_team_id;
    new_unit.position = p_world_pos;
    new_unit.state = IDLE;
    new_unit.target_grid = Vector2i(-1, -1);

    // 保存配置
    new_unit.stats = p_stats;

    // 从配置初始化数值
    new_unit.current_hp = p_stats->get_health_max();
    new_unit.current_shield = p_stats->get_shield_max();

    // 初始化其他
    new_unit.velocity = Vector2(0, 0);
    new_unit.last_attack_time = -100.0;
    new_unit.target_unit_id = -1;

    // 存入容器
    if (id_to_index.size() >= units.capacity()) {
        units.reserve(units.size() + 100);
    }
    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;

    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

    if (index_to_remove != last_unit_idx) {
        UnitData& last_unit = units.back();
        units[index_to_remove] = last_unit;
        id_to_index[last_unit.id] = index_to_remove;
    }

    units.pop_back();
    id_to_index.erase(p_unit_id);
}

void UnitManager::command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos) {
    if (!flow_field_manager) return;
    Vector2i target_grid_pos = flow_field_manager->world_to_grid(p_target_world_pos);
    if (!(flow_field_manager->is_in_grid(target_grid_pos))) return;
    flow_field_manager->create_flow_field(target_grid_pos, false);
    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];
            unit.target_pos = p_target_world_pos;
            unit.target_grid = target_grid_pos;
            unit.state = MOVING;
        }
    }
}

void UnitManager::update_spatial_grid() {
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].clear();
    }
    for (int i = 0; i < units.size(); ++i) {
        Vector2i rel_pos = flow_field_manager->world_to_relative(units[i].position);
        int ux = rel_pos.x / 2;
        int uy = rel_pos.y / 2;
        if (ux >= 0 && ux < unit_grid_width && uy >= 0 && uy < unit_grid_height) {
            int grid_idx = uy * unit_grid_width + ux;
            unit_grid[grid_idx].push_back(i);
        }
    }
}

std::vector<int> UnitManager::get_nearby_units(Vector2 p_world_pos, float p_radius) {
    std::vector<int> nearby_indices;
    Vector2i rel_pos = flow_field_manager->world_to_relative(p_world_pos);
    int ux = rel_pos.x / 2;
    int uy = rel_pos.y / 2;
    for (int nx = ux - 1; nx <= ux + 1; ++nx) {
        for (int ny = uy - 1; ny <= uy + 1; ++ny) {
            if (nx >= 0 && nx < unit_grid_width && ny >= 0 && ny < unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    nearby_indices.push_back(unit_idx);
                }
            }
        }
    }
    return nearby_indices;
}

void UnitManager::_physics_process(double p_delta) {
    if (!is_setup) return;
    if (!flow_field_manager) {
        flow_field_manager = get_node<FlowFieldManager>("../FlowFieldManager");
        if (!flow_field_manager) return;
    }
    update_spatial_grid();
    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        update_velocity(unit, p_delta);
        move(unit, p_delta);
    }
}

Vector2 UnitManager::get_flow(const UnitData& p_unit) {
    return flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos);
}

Vector2 UnitManager::get_separation(const UnitData& p_unit) {
    bool is_IDLE = (p_unit.state == IDLE);
    Vector2 separation;
    for (int unit_idx : get_nearby_units(p_unit.position, 1)) {
        const UnitData& nearby_unit = units[unit_idx];
        Vector2 radius_vector = nearby_unit.position - p_unit.position;
        float length_squared = radius_vector.length_squared();
        if (length_squared < 10e-12) return Vector2(0, 0);

        if (is_IDLE) {
            if (nearby_unit.state == IDLE) separation -= radius_vector / length_squared / length_squared;
            else separation -= 2 * radius_vector / length_squared;
        }
        else {
            if (nearby_unit.state == IDLE) separation -= 0.5 * radius_vector / length_squared;
            else separation -= radius_vector / length_squared;
        }
    }
    return separation.limit_length(separation_limit);
}

Vector2 UnitManager::get_friction(const UnitData& p_unit) {
    return (-p_unit.velocity);
}

Vector2 UnitManager::get_force(const UnitData& p_unit) {
    Vector2 force = Vector2(0, 0);
    switch (p_unit.state) {
    case IDLE:
        force = get_friction(p_unit) * friction_factor + get_separation(p_unit) * separation_factor;
        break;
    case MOVING:
        force = get_flow(p_unit) * flow_factor + get_separation(p_unit) * separation_factor;
        break;
    }
    return force;
}

void UnitManager::update_velocity(UnitData& p_unit, double p_delta) {
    p_unit.velocity += get_force(p_unit) * p_delta;
    p_unit.velocity = (p_unit.velocity).limit_length(p_unit.speed);
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    p_unit.position += p_unit.velocity * p_delta;
}

Vector2 UnitManager::get_unit_position(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) return units[it->second].position;
    return Vector2(0, 0);
}

int UnitManager::get_unit_state(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) return (int)(units[it->second].state);
    return (int)(IDLE);
}

int UnitManager::get_unit_team(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) return units[it->second].team_id;
    return -1;
}

void UnitManager::_bind_methods() {
    BIND_ENUM_CONSTANT(IDLE);
    BIND_ENUM_CONSTANT(MOVING);
    BIND_ENUM_CONSTANT(CHASING);
    BIND_ENUM_CONSTANT(ATTACKING);

    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);

    // [重点修改] 更新绑定参数
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "stats", "team_id"), &UnitManager::spawn_unit);

    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("get_unit_team", "unit_id"), &UnitManager::get_unit_team);
}
