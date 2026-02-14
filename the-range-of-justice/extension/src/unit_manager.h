#pragma once

#include <vector>
#include <unordered_map>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/array.hpp>
#include "flow_field_manager.h"
#include "selection_manager.h"

namespace godot {

	class UnitManager : public Node2D {
		GDCLASS(UnitManager, Node2D)

	public:
		enum UnitState {
			IDLE,        // 待机
			MOVING,      // 移动中
		};

		enum UnitType {
			SQUARE
		};

		struct UnitData {
			int id;                 // 唯一标识符
			Vector2 position;       // 当前世界坐标
			Vector2 velocity;       // 当前速度向量
			Vector2 target_pos;		//目标的世界坐标
			Vector2i target_grid;   // 目标的网格坐标（与流场坐标一致，不同于unit_grid中的坐标）
			float speed;            // 移动速度
			float radius;           // 碰撞半径（用于单位间排斥）
			UnitState state;        // 状态机
			UnitType type;			// 单位种类
			
			bool is_selected = false;
			bool is_mouse_on = false;
			float selection_radius;

			UnitData() : id(-1), speed(200.0f), radius(6.0f), state(IDLE), selection_radius(8.0f) {}
		};

	private:
		FlowFieldManager *flow_field_manager;
		SelectionManager *selection_manager;
		std::vector<UnitData> units;
		std::unordered_map<int, size_t> id_to_index;
		int next_unit_id = 0;

		// --- 空间网格 (Unit Grid) ---
		// 每一个格子存储该区域内的单位在 units 数组中的索引(index)
		// 使用 1D 数组模拟 2D 网格：unit_grid[y * width + x]
		// 格子的尺寸是流场中格子的两倍
		std::vector<std::vector<int>> unit_grid;

		int unit_grid_width = 0;
		int unit_grid_height = 0;
		int unit_grid_size = 0;
		Vector2i unit_grid_cell_size = Vector2i(0, 0);

		float flow_factor = 2000.0f;
		float separation_factor = 10000.0f;
		float separation_limit = 1000.0f;
		float friction_factor = 100.0f;

		bool is_setup = false;
		MultiMeshInstance2D* multimesh_instance = nullptr;

	protected:
		static void _bind_methods();

	public:
		UnitManager();
		~UnitManager();

		// --- 系统管理 ---
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);

		// --- 单位生命周期 ---
		int spawn_unit(Vector2 p_world_pos, UnitType p_type);
		void despawn_unit(int p_unit_id);
		void command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos);

		// --- 空间网格核心操作 ---
		void update_spatial_grid();
		std::vector<int> get_nearby_units(Vector2 p_world_pos, float p_radius);

		// --- 核心循环 ---
		virtual void _physics_process(double p_delta) override;

		// --- 逻辑计算 ---
		Vector2 get_flow(UnitData& p_unit);
		Vector2 get_separation(UnitData& p_unit);
		Vector2 get_friction(UnitData& p_unit);
		Vector2 get_force(UnitData& p_unit);
		void update_velocity(UnitData& p_unit, double p_delta);
		void move(UnitData& p_unit, double p_delta);

		void update_multimesh_buffer();

		void update_selection_state_and_target_position(UnitData& p_unit);

		// 获取数据供 Godot 渲染
		Vector2 get_unit_position(int p_unit_id) const;
		int get_unit_state(int p_unit_id) const;
		void set_multimesh_instance(Node* p_node);
		void set_flow_field_manager(Node* p_node);
		void set_selection_manager(Node* p_node);
	};
}

VARIANT_ENUM_CAST(UnitManager::UnitState);
VARIANT_ENUM_CAST(UnitManager::UnitType);