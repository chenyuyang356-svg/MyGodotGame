#pragma once

#include <vector>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/rect2.hpp>

namespace godot {

	class SelectionManager : public Node2D {
		GDCLASS(SelectionManager, Node2D)

	public:
		enum SelectionState {
			NOT_SELECTING,
			SINGLE_SELECTING,
			TYPE_SELECTING,
			BOX_SELECTING,
			BOX_SELECTION_ENDED,
			SELECTING_TARGET_POSITION
		};

	protected:
		static void _bind_methods();

	public:
		SelectionManager();
		~SelectionManager();

		SelectionState state = NOT_SELECTING;
		Vector2 mouse_position;
		Vector2 selecting_start_point;
		Vector2 selecting_end_point;
		Rect2 selecting_box;
		int selected_unit_id = -1;
		int selected_type;

		void set_mouse_position(Vector2 p_mouse_position);

		void single_selecting();

		void type_selecting();

		void selecting_target_position();

		void box_selecting();

		void end_box_selecting();
	};
}