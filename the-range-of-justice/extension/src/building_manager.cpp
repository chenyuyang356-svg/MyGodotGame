#include "building_manager.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

BuildingManager::BuildingManager() {}
BuildingManager::~BuildingManager() {}

void BuildingManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void BuildingManager::set_unit_manager(Node* p_node) {
    unit_manager = Object::cast_to<UnitManager>(p_node);
}

bool BuildingManager::is_area_clear(Vector2i p_grid_pos, Vector2i p_size) {
    if (!flow_field_manager) return false;

    // --- 1. 地形与边界检查 ---
    for (int x = 0; x < p_size.x; ++x) {
        for (int y = 0; y < p_size.y; ++y) {
            Vector2i current_cell = p_grid_pos + Vector2i(x, y);

            // 检查边界
            if (!flow_field_manager->is_in_grid(current_cell)) return false;

            // 检查代价 (255 为墙或已有建筑)
            if (flow_field_manager->get_cost(current_cell) == 255) return false;
        }
    }

    // --- 2. 单位阻挡检查 ---
    if (unit_manager) {
        // 获取格子大小（从 FlowFieldManager 获取）
        Vector2i cell_size = flow_field_manager->get_cell_size();

        // 计算建筑在世界空间中的矩形范围 (Rect2)
        Vector2 world_pos = Vector2(p_grid_pos.x * cell_size.x, p_grid_pos.y * cell_size.y);
        Vector2 world_size = Vector2(p_size.x * cell_size.x, p_size.y * cell_size.y);
        Rect2 building_rect(world_pos, world_size);

        // 使用 UnitManager 的空间网格优化查询
        // 查询半径设定为建筑对角线的一半，确保覆盖整个矩形
        Vector2 center = world_pos + world_size * 0.5;
        float query_radius = world_size.length() * 0.5;

        std::vector<int> nearby_units = unit_manager->get_nearby_units(center, query_radius);

        for (int unit_idx : nearby_units) {
            // 获取单位位置 (需要你在 UnitManager 里确保 units 数组是可访问的，
            // 或者使用你已经实现的 get_unit_position)
            Vector2 u_pos = (unit_manager->units)[unit_idx].position;

            // 如果单位位置在建筑矩形内，则判定为阻挡
            if (building_rect.has_point(u_pos)) {
                return false;
            }
        }
    }

    return true;
}

int BuildingManager::place_building(Vector2i p_grid_pos, Vector2i p_size, int p_type) {
    if (!is_area_clear(p_grid_pos, p_size)) return -1;

    // 1. 创建建筑数据
    int b_id = next_building_id++;
    BuildingData b;
    b.id = b_id;
    b.grid_pos = p_grid_pos;
    b.size = p_size;
    b.type = p_type;
    buildings[b_id] = b;

    // 2. 修改代价地图：将建筑占用的格子设为不可通行
    for (int x = 0; x < p_size.x; ++x) {
        for (int y = 0; y < p_size.y; ++y) {
            flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255);
        }
    }

    // 3. 标记流场需要重算
    flow_field_manager->make_all_dirty();

    return b_id;
}

void BuildingManager::remove_building(int p_building_id) {
    auto it = buildings.find(p_building_id);
    if (it == buildings.end()) return;

    BuildingData& b = it->second;

    // 1. 恢复代价地图为平地 (1)
    for (int x = 0; x < b.size.x; ++x) {
        for (int y = 0; y < b.size.y; ++y) {
            flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1);
        }
    }

    // 2. 从记录中删除
    buildings.erase(it);

    // 3. 再次标记流场重算
    flow_field_manager->make_all_dirty();
}

Vector2i BuildingManager::get_building_grid_pos(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.grid_pos;
    return Vector2i(-1, -1);
}

void BuildingManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &BuildingManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &BuildingManager::set_unit_manager);
    ClassDB::bind_method(D_METHOD("is_area_clear", "grid_pos", "size"), &BuildingManager::is_area_clear);
    ClassDB::bind_method(D_METHOD("place_building", "grid_pos", "size", "type"), &BuildingManager::place_building);
    ClassDB::bind_method(D_METHOD("remove_building", "building_id"), &BuildingManager::remove_building);
    ClassDB::bind_method(D_METHOD("get_building_grid_pos", "building_id"), &BuildingManager::get_building_grid_pos);
}