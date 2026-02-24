#pragma once

#include <godot_cpp/core/class_db.hpp>

#include "game_manager.h"

using namespace godot;

GameManager::GameManager() {}
GameManager::~GameManager() {}

void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager || !is_setup) { return; }
	unit_manager->update(p_delta);
	update_group(p_delta);
}

void GameManager::update_group(double p_delta) {
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->CLEANUP_INTERVAL) return;
	group_manager->cleanup_timer = 0.0;

	auto it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		// 如果没有单位在移动了，且 ID 列表也空了（或单位都到达了）
		// 这里可以根据需求决定：是计数器归零就删，还是单位完全清空才删
		if (it->second.moving_units_count <= 0) {
			for (int unit_id : it->second.unit_ids) {
				int index = unit_manager->get_unit_index_by_id(unit_id);
				if (index >= 0) {
					unit_manager->units[index].temp_group_id = -1;
				}
			}
			it = group_manager->temp_groups.erase(it);
		}
		else {
			++it;
		}
	}
}

void GameManager::set_unit_manager(Node* p_node) {
	unit_manager = Object::cast_to<UnitManager>(p_node);
}

void GameManager::set_building_manager(Node* p_node) {
	building_manager = Object::cast_to<BuildingManager>(p_node);
}

void GameManager::set_flow_field_manager(Node* p_node) {
	flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void GameManager::set_selection_manager(Node* p_node) {
	selection_manager = Object::cast_to<SelectionManager>(p_node);
}

void GameManager::set_group_manager(Node* p_node) {
	group_manager = Object::cast_to<GroupManager>(p_node);
}


void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_selection_manager(selection_manager);
	unit_manager->set_group_manager(group_manager);
	building_manager->set_flow_field_manager(flow_field_manager);
	building_manager->set_unit_manager(unit_manager);
	unit_manager->setup_system(p_width, p_height, p_cell_size, p_origin);
	is_setup = true;
}

void GameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &GameManager::set_unit_manager);
	ClassDB::bind_method(D_METHOD("set_building_manager", "node"), &GameManager::set_building_manager);
	ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &GameManager::set_flow_field_manager);
	ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &GameManager::set_selection_manager);
	ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &GameManager::set_group_manager);
	ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &GameManager::setup_system);
}