#include "selection_manager.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace {
	Rect2 get_rect(Vector2 start_point, Vector2 end_point) {
		Vector2 position = start_point.min(end_point);
		Vector2 size = (end_point - start_point).abs();
		return Rect2(position, size);
	}
}

SelectionManager::SelectionManager() {
	mouse_position = Vector2(-1000000, -1000000);
	selecting_start_point = Vector2(0, 0);
	selecting_end_point = Vector2(0, 0);
}

SelectionManager::~SelectionManager() {}

void SelectionManager::set_mouse_position(Vector2 p_mouse_position) {
	mouse_position = p_mouse_position;
}

void SelectionManager::single_selecting() {
	state = SINGLE_SELECTING;
}

void SelectionManager::type_selecting() {
	state = TYPE_SELECTING;
}

void SelectionManager::selecting_target_position() {
	state = SELECTING_TARGET_POSITION;
}

void SelectionManager::box_selecting() {
	if (state != BOX_SELECTING) {
		state = BOX_SELECTING;
		selecting_start_point = mouse_position;
	}

	selecting_end_point = mouse_position;
	selecting_box = get_rect(selecting_start_point, selecting_end_point);
}

void SelectionManager::end_box_selecting() {
	state = BOX_SELECTION_ENDED;
	selecting_end_point = mouse_position;
	selecting_box = get_rect(selecting_start_point, selecting_end_point);
}

void SelectionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mouse_position", "mouse_position"), &SelectionManager::set_mouse_position);
	ClassDB::bind_method(D_METHOD("single_selecting"), &SelectionManager::single_selecting);
	ClassDB::bind_method(D_METHOD("type_selecting"), &SelectionManager::type_selecting);
	ClassDB::bind_method(D_METHOD("selecting_target_position"), &SelectionManager::selecting_target_position);
	ClassDB::bind_method(D_METHOD("box_selecting"), &SelectionManager::box_selecting);
	ClassDB::bind_method(D_METHOD("end_box_selecting"), &SelectionManager::end_box_selecting);
}