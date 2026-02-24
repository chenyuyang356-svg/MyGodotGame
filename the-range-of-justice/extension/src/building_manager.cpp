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

void BuildingManager::register_building_type(String p_name, String p_path) {
    Ref<BuildingStats> stats = BuildingLoader::load_from_txt(p_path);
    if (stats.is_valid()) {
        building_types_cache[p_name] = stats;
    }
}

bool BuildingManager::is_area_clear(Vector2i p_grid_pos, Ref<BuildingStats> p_stats) {
    if (!flow_field_manager || p_stats.is_null()) return false;

    Vector2i footprint = p_stats->get_footprint();
    Vector2i clearance = p_stats->get_clearance_size();

    // 1. 计算 Clearance 检查的起始点
    // 假设 p_grid_pos 是建筑实际占用(footprint)的左上角
    // 偏移量 = (Clearance尺寸 - Footprint尺寸) / 2
    Vector2i offset = (clearance - footprint) / 2;
    Vector2i check_start = p_grid_pos - offset;

    // --- 逻辑检查 A: 地形与 Clearance 范围 ---
    // 这个范围内不能有任何已存在的建筑 (Cost 255)
    for (int x = 0; x < clearance.x; ++x) {
        for (int y = 0; y < clearance.y; ++y) {
            Vector2i current_cell = check_start + Vector2i(x, y);

            if (!flow_field_manager->is_in_grid(current_cell)) return false;

            // 只要 Clearance 范围内碰到了 Cost 255，就说明离别的建筑太近了
            if (flow_field_manager->get_cost(current_cell) == 255) return false;
        }
    }

    // --- 逻辑检查 B: 单位阻挡 (仅检查 Footprint 范围) ---
    if (unit_manager) {
        Vector2i cell_size = flow_field_manager->get_cell_size();
        Vector2 world_pos = Vector2(p_grid_pos.x * cell_size.x, p_grid_pos.y * cell_size.y);
        Vector2 world_size = Vector2(footprint.x * cell_size.x, footprint.y * cell_size.y);
        Rect2 building_rect(world_pos, world_size);

        Vector2 center = world_pos + world_size * 0.5;
        float query_radius = world_size.length() * 0.5;

        std::vector<int> nearby_units = unit_manager->get_nearby_units(center, query_radius);
        for (int unit_idx : nearby_units) {
            if (building_rect.has_point(unit_manager->units[unit_idx].position)) {
                return false;
            }
        }
    }

    return true;
}

int BuildingManager::place_building_by_type(String p_type_name, Vector2i p_grid_pos, int p_team_id) {
    if (!building_types_cache.has(p_type_name)) return -1;

    Ref<BuildingStats> stats = building_types_cache[p_type_name];
    if (!is_area_clear(p_grid_pos, stats)) return -1;

    // 1. 创建建筑
    int b_id = next_building_id++;
    BuildingData b;
    b.id = b_id;
    b.grid_pos = p_grid_pos;
    b.stats = stats;
    b.team_id = p_team_id;
    b.current_health = stats->get_health_max();
    buildings[b_id] = b;

    // 2. 修改代价地图：注意这里只设置 Footprint 的格子
    Vector2i footprint = stats->get_footprint();
    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255);
        }
    }

    flow_field_manager->make_all_dirty();
    return b_id;
}

void BuildingManager::remove_building(int p_building_id) {
    auto it = buildings.find(p_building_id);
    if (it == buildings.end()) return;

    BuildingData& b = it->second;
    Vector2i footprint = b.stats->get_footprint();

    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1);
        }
    }

    buildings.erase(it);
    flow_field_manager->make_all_dirty();
}

Vector2i BuildingManager::get_building_grid_pos(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.grid_pos;
    return Vector2i(-1, -1);
}

Ref<BuildingStats> BuildingManager::get_building_stats(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.stats;
    return nullptr;
}

PackedStringArray BuildingManager::get_registered_building_types() const {
    PackedStringArray list;
    // 遍历 HashMap 的所有键
    for (const auto& E : building_types_cache) {
        list.append(E.key);
    }
    return list;
}

void BuildingManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_building_type", "name", "path"), &BuildingManager::register_building_type);
    ClassDB::bind_method(D_METHOD("place_building_by_type", "type_name", "grid_pos", "team_id"), &BuildingManager::place_building_by_type);
    ClassDB::bind_method(D_METHOD("is_area_clear", "grid_pos", "building_stats"), &BuildingManager::is_area_clear);

    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &BuildingManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &BuildingManager::set_unit_manager);
    ClassDB::bind_method(D_METHOD("remove_building", "building_id"), &BuildingManager::remove_building);
    ClassDB::bind_method(D_METHOD("get_building_grid_pos", "building_id"), &BuildingManager::get_building_grid_pos);
    ClassDB::bind_method(D_METHOD("get_building_stats", "building_id"), &BuildingManager::get_building_stats);

    ClassDB::bind_method(D_METHOD("get_registered_building_types"), &BuildingManager::get_registered_building_types);
    ClassDB::bind_method(D_METHOD("get_building_stats_by_type", "type_name"), &BuildingManager::get_building_stats_by_type);
}