#pragma once

#include <godot_cpp/core/class_db.hpp>

#include "game_manager.h"

using namespace godot;

GameManager::GameManager() {}
GameManager::~GameManager() {}

void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !multimesh_instance || !is_setup) { return; }
	unit_manager->update(p_delta);
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

void GameManager::set_multimesh_instance(Node* p_node) {
	multimesh_instance = Object::cast_to<MultiMeshInstance2D>(p_node);
}

void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_selection_manager(selection_manager);
	unit_manager->set_multimesh_instance(multimesh_instance);
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
	ClassDB::bind_method(D_METHOD("set_multimesh_instance", "node"), &GameManager::set_multimesh_instance);
	ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &GameManager::setup_system);
}