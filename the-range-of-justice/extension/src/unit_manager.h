#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/multi_mesh_instance2d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/core/memory.hpp>   

#include "flow_field_manager.h"
#include "selection_manager.h"
#include "unit_loader.h"

namespace godot {
	class AttackManager; // 1. 前向声明

	class UnitManager : public Node2D {
		GDCLASS(UnitManager, Node2D)

		friend class GameManager;
		friend class AttackManager;
	public:
		enum UnitState {
			IDLE,        // 待机
			MOVING,      // 移动中
			CHASING,     // 追击中
			ATTACKING,   // 攻击中
		};


		//这三个参数是为了调试而设的
		float unit_speed = 200.0f;
		float unit_radius = 28.0f;
		float unit_selection_radius = 32.0f;

		struct UnitData {
			int id;                 // 唯一标识符
			Vector2 position;       // 当前世界坐标
			Vector2 velocity;       // 当前速度向量
			Vector2 target_pos;		//目标的世界坐标
			Vector2i target_grid;   // 目标的网格坐标（与流场坐标一致，不同于unit_grid中的坐标）
			
			Ref<UnitStats> stats;

			UnitState state;        // 状态机		
			bool is_selected = false;
			bool is_mouse_on = false;
			float selection_radius;
			float current_health;

			float anim_time = 0.0f; // 累计播放时间

			int team_id;            // 0=玩家, 1=敌人
			int target_id = -1;     // 当前攻击目标的 ID
			float attack_cooldown = 0.0f;
			UnitData() : id(-1), team_id(0), state(IDLE), current_health(0), target_id(-1), attack_cooldown(0.0f) {}
		};

	
	private:
		AttackManager* attack_manager = nullptr; 
		FlowFieldManager *flow_field_manager;
		SelectionManager *selection_manager;
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
		float separation_radius_factor = 3.0f;		//排斥力半径与单位半径的比值
		float separation_limit = 1000.0f;
		float lateral_separation_factor = 1.0f;
		float friction_factor = 100.0f;
		float force_threshold_squared = 1.0f;
		float velocity_threshold_squared = 1.0f;
		float desired_integration = 0.1f;

		bool is_setup = false;

		// 渲染器映射：每个 UnitStats 资源对应一个 MultiMeshInstance2D
		std::unordered_map<UnitStats*, MultiMeshInstance2D*> type_renderers;

		// 分组缓存：每一帧将单位按 UnitStats 指针分组
		std::unordered_map<UnitStats*, std::vector<int>> type_grouping_cache;

		// 共享的 Shader 资源，避免每个类型都创建一个新的 Shader
		Ref<Shader> unit_shader;

		HashMap<String, Ref<UnitStats>> unit_types_cache;

	protected:
		static void _bind_methods();

	public:
		UnitManager();
		~UnitManager();

		std::vector<UnitData> units;

		// --- 系统管理 ---
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);

		// --- 单位生命周期 ---
		int spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id = 0);
		void despawn_unit(int p_unit_id);
		void command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos);
		void UnitManager::command_units_to_patrol(Array p_unit_ids, Array p_waypoints);

		// --- 空间网格核心操作 ---
		void update_spatial_grid();
		std::vector<int> get_nearby_units(Vector2 p_world_pos, float p_radius);

		// --- 核心循环 ---
		void update(double p_delta);

		// --- 逻辑计算 ---
		Vector2 get_flow(UnitData& p_unit);
		Vector2 get_separation(UnitData& p_unit);
		Vector2 get_friction(UnitData& p_unit);
		Vector2 get_force(UnitData& p_unit);
		void update_state(UnitData& p_unit);
		void update_velocity(UnitData& p_unit, double p_delta);
		void move(UnitData& p_unit, double p_delta);

		void update_multimesh_buffer(double p_delta);

		void update_selection_state_and_target_position(UnitData& p_unit);

		// 获取数据供 Godot 渲染
		Vector2 get_unit_position(int p_unit_id) const;
		int get_unit_state(int p_unit_id) const;
		void set_flow_field_manager(Node* p_node);
		void set_selection_manager(Node* p_node);

		void register_unit_type(String p_name, String p_path);
<<<<<<< Updated upstream
		int spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id);

		void set_control_group(int p_index, const std::vector<int>& p_unit_ids);

=======
		int spawn_unit_by_type(String p_type_name, Vector2 p_pos, int team_id);

>>>>>>> Stashed changes
		// 攻击模块
		int get_unit_index_by_id(int p_id); 
		void set_attack_manager(Node* p_node);

		//调试
		void set_unit_speed(float p_val) { unit_speed = p_val; }
		float get_unit_speed() const { return unit_speed; }

		void set_unit_radius(float p_val) { unit_radius = p_val; }
		float get_unit_radius() const { return unit_radius; }

		void set_unit_selection_radius(float p_val) { unit_selection_radius = p_val; }
		float get_unit_selection_radius() const { return unit_selection_radius; }

		void set_flow_factor(float p_val) { flow_factor = p_val; }
		float get_flow_factor() const { return flow_factor; }

		void set_separation_factor(float p_val) { separation_factor = p_val; }
		float get_separation_factor() const { return separation_factor; }

		void set_separation_limit(float p_val) { separation_limit = p_val; }
		float get_separation_limit() const { return separation_limit; }

		void set_lateral_separation_factor(float p_val) { lateral_separation_factor = p_val; }
		float get_lateral_separation_factor() const { return lateral_separation_factor; }

		void set_separation_radius_factor(float p_val) { separation_radius_factor = p_val; }
		float get_separation_radius_factor() const { return separation_radius_factor; }

		void set_friction_factor(float p_val) { friction_factor = p_val; }
		float get_friction_factor() const { return friction_factor; }

		void set_force_threshold_squared(float p_val) { force_threshold_squared = p_val; }
		float get_force_threshold_squared() const { return force_threshold_squared; }

		void set_velocity_threshold_squared(float p_val) { velocity_threshold_squared = p_val; }
		float get_velocity_threshold_squared() const { return velocity_threshold_squared; }

		void set_desired_integration(float p_val) { desired_integration = p_val; }
		float get_desired_integration() const { return desired_integration; }
	};
}

VARIANT_ENUM_CAST(UnitManager::UnitState);