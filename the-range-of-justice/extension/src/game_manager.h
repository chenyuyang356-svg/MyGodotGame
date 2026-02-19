#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include "unit_manager.h"
#include "building_manager.h"
#include "flow_field_manager.h"
#include "selection_manager.h"
#include "group_manager.h"
#include "unit_loader.h"

namespace godot {

	class GameManager : public Node2D {
		GDCLASS(GameManager, Node2D)

	private:
		UnitManager* unit_manager = nullptr;
		BuildingManager* building_manager = nullptr;
		FlowFieldManager* flow_field_manager = nullptr;
		SelectionManager* selection_manager = nullptr;
		GroupManager* group_manager = nullptr;

		MultiMeshInstance2D* multimesh_instance = nullptr;

		bool is_setup = false;

	protected:
		static void _bind_methods();

	public:
		GameManager();
		~GameManager();

		virtual void _physics_process(double p_delta) override;

		void set_unit_manager(Node* p_node);
		void set_building_manager(Node* p_node);
		void set_flow_field_manager(Node* p_node);
		void set_selection_manager(Node* p_node);
		void set_group_manager(Node* p_node);
		void set_multimesh_instance(Node* p_node);
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);
	};

}