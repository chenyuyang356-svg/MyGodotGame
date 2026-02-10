#include "unit_manager.h"

#include <queue>

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace {
    float get_length_squared(Vector2 p_vector) {
        float x = p_vector.x;
        float y = p_vector.y;
        return x * x + y * y;
    }
}

UnitManager::UnitManager() {
    units.reserve(1000);
}

UnitManager::~UnitManager() {}

void UnitManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
    flow_field_manager->setup_grid(p_width, p_height, p_origin, p_cell_size);

    unit_grid_width = p_width / 2;
    unit_grid_height = p_height / 2;
    unit_grid_size = unit_grid_width * unit_grid_height;
    unit_grid_cell_size = p_cell_size / 2;

    unit_grid.resize(unit_grid_size);

    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].reserve(10);
    }
}

int UnitManager::spawn_unit(Vector2 p_world_pos, UnitType p_type) {
    // 1. 创建一个新的单位数据结构
    UnitData new_unit;

    // 2. 分配唯一 ID 并自增计数器
    new_unit.id = next_unit_id++;

    // 3. 初始化物理属性
    new_unit.position = p_world_pos;

    // 4. 初始化状态(待完善，根据单位类型应有不同的初始化)
    new_unit.velocity = Vector2(0, 0);
    new_unit.state = IDLE;
    new_unit.type = p_type;
    new_unit.target_grid = Vector2i(-1, -1); // 初始没有目标

    // 5. 存入 vector
    // 注意：如果单位非常多，建议在 UnitManager 构造函数里先调用 units.reserve(1000)
    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;

    // 6. 返回 ID，以便 GDScript 记录并关联对应的 Sprite
    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

    if (index_to_remove != last_unit_idx) {
        // 1. 获取最后一个单位的数据
        UnitData& last_unit = units.back();

        // 2. 将最后一个单位移动到要删除的位置
        units[index_to_remove] = last_unit;

        // 3. 更新被移动单位在哈希表中的索引
        id_to_index[last_unit.id] = index_to_remove;
    }

    // 4. 删除 vector 最后一个元素，并从哈希表中移除目标 ID
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

        // 使用哈希表直接定位
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

        // 缩放到单位网格（单位网格尺寸是流场的 2 倍）
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

    // 检查 3x3 范围内的格子
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
    return;
}

Vector2 UnitManager::calculate_separation(int p_unit_idx) {
    UnitData unit = units[p_unit_idx];
    bool is_IDLE = (unit.state == IDLE);
    Vector2 Separation;
    for (int unit_idx : get_nearby_units(unit.position, 1)) {
        UnitData nearby_unit = units[unit_idx];
        Vector2 radius_vector = nearby_unit.position - unit.position;
        if (get_length_squared(radius_vector) < 10e-12) {
            return Vector2(0, 0);
        }
        if (is_IDLE) {
            if (nearby_unit.state == IDLE) {
                return Vector2(0, 0);
            }
        }
    }
}

Vector2 UnitManager::get_unit_position(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    
    if (it != id_to_index.end()) {
        return units[it->second].position;
    }

    return Vector2(0, 0);
}

int UnitManager::get_unit_state(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);

    if (it != id_to_index.end()) {
        return (int)(units[it->second].state);
    }

    return (int)(IDLE);
}

void UnitManager::_bind_methods() {
    BIND_ENUM_CONSTANT(IDLE);
    BIND_ENUM_CONSTANT(MOVING);

    BIND_ENUM_CONSTANT(SQUARE);

    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("update_spatial_grid", "unit_ids"), &UnitManager::update_spatial_grid);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
}
