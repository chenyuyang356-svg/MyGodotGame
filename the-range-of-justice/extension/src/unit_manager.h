#pragma once

#include <vector>
#include <unordered_map>
#include <string>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>

#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/transform2d.hpp>
#include <godot_cpp/core/memory.hpp>   

#include "flow_field_manager.h"
#include "group_manager.h"
#include "fog_manager.h"
#include "unit_loader.h"
#include "unit_data.h"
#include "weapon_manager.h"
#include "effect_manager.h"

namespace godot {
	class AttackManager; // 前向声明
	class SelectionManager;
	class BuildingManager;

	class UnitManager : public Node3D {
		GDCLASS(UnitManager, Node3D)

		friend class GameManager;
		friend class AttackManager;
	
	private:
		AttackManager* attack_manager = nullptr; 
		FlowFieldManager *flow_field_manager = nullptr;
		GroupManager* group_manager = nullptr;
		FogManager* fog_manager = nullptr;
		WeaponManager* weapon_manager = nullptr;
		EffectManager* effect_manager = nullptr;

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
		float lateral_separation_factor = 1.0f;		//(遗留，未在移动逻辑中使用)
		float friction_factor = 100.0f;
		float force_threshold_squared = 1.0f;
		float velocity_threshold_squared = 1.0f;
		float desired_integration = 0.1f;			//(遗留，未在移动逻辑中使用)

		// --- 以下为集中调参：由 game_tuning.tres 写入，含义见 game_tuning.gd ---
		float air_height_threshold = 20.0f;			//空中/地面判定高度阈值
		float density_limit = 4.0f;					//路径"人墙"密度阈值
		float density_update_interval = 0.5;		//密度图重建间隔(秒)
		float idle_density_factor = 5.0f;			//待机单位密度贡献倍率
		float density_next_cell_factor = 0.8f;		//移动方向下一格密度贡献系数
		float arrival_stop_distance_sq = 10.0f;		//到达死区距离(平方像素)
		float density_avoidance_strength = 0.1f;	//直线模式密度避让强度
		float density_avoidance_flow_strength = 0.02f;//流场模式密度避让强度
		float separation_extra_radius = 30.0f;		//排斥搜索半径额外余量
		float separation_min_dist_factor = 1.1f;	//排斥理想间距系数
		float sep_idle_vs_idle_k = 0.5f;			//双方待机排斥权重
		float sep_idle_vs_moving_k = 2.0f;			//待机推移动单位排斥权重
		float sep_moving_vs_idle_k = 0.1f;			//移动单位被待机顶开权重
		float sep_force_to_total_ratio = 0.5f;		//分离力计入总力比例
		float target_integration_factor = 1.1f;		//组目标集成值倍率(到达判定)
		float critical_area_integration_margin = 3.0f;//临界区判定余量
		float arrival_desired_distance_factor = 1.5f;//期望到达距离 = 半径×该值
		float soft_arrival_factor = 1.1f;			//软到达半径倍率
		float soft_arrival_neighbor_radius_factor = 3.0f;//到达判定时邻居扫描半径系数
		float stuck_check_interval = 0.5f;			//卡住检测间隔(秒)
		float stuck_threshold_move_factor = 0.025f;	//卡住位移阈值系数
		float stuck_threshold_min = 0.5f;			//卡住位移阈值下限(像素)
		float stuck_rotation_threshold_factor = 0.08f;//卡住转角阈值系数
		float stuck_give_up_time = 8.0f;			//卡住放弃时间(秒)
		float path_recheck_interval = 0.8f;			//移动中直线路径重查间隔(秒)
		float chase_path_recheck_interval = 0.5f;	//追击中路径重查间隔(秒)
		float arrival_slow_radius_factor = 5.0f;	//到达减速半径系数
		float arrival_min_speed_factor = 0.2f;		//到达最小速度比例
		float chase_slow_down_factor = 1.5f;		//追击减速起始距离系数
		float chase_min_speed_factor = 0.5f;		//追击最小速度比例
		float rubberband_sensitivity = 0.02f;		//组内速度橡皮筋灵敏度
		float rubberband_speed_min = 0.8f;			//橡皮筋速度下限
		float rubberband_speed_max = 1.2f;			//橡皮筋速度上限
		float engine_forward_ratio = 0.7f;			//引擎方向中"朝向"权重
		float propulsion_force_scale = 1000.0f;		//推进力总缩放
		float propulsion_min_power = 0.1f;			//推进力基础功率
		float idle_friction_multiplier = 3.0f;		//待机摩擦力倍率
		float turn_ramp_angle = 0.5f;				//转向线速区(弧度)
		float collision_resolve_radius_factor = 2.5f;//单位-单位碰撞解算搜索半径系数
		float idle_resistance = 4.0f;				//待机单位阻力倍率
		float collision_smoothing = 0.4f;			//重叠修正平滑系数
		float wall_collision_factor_moving = 0.5f;	//移动中撞墙回推系数
		float wall_collision_factor_idle = 1.0f;	//待机撞墙回推系数
		float target_projection_margin = 1.0f;		//目标点投影到墙边界的边距(像素)

		bool is_setup = false;

		double density_update_timer = 0.0;

		// 渲染器映射：每个 UnitStats 资源对应一个 MultiMeshInstance2D
		std::unordered_map<UnitStats*, MultiMeshInstance3D*> type_renderers;

		// 分组缓存：每一帧将单位按 UnitStats 指针分组
		std::unordered_map<UnitStats*, std::vector<int>> type_grouping_cache;

		// 共享的 Shader 资源，避免每个类型都创建一个新的 Shader
		Ref<Shader> unit_shader;

		// 影子渲染器
		std::unordered_map<UnitStats*, MultiMeshInstance3D*> shadow_renderers;
		Ref<Shader> shadow_shader;

		// 血条渲染器
		MultiMeshInstance3D* global_hp_bar_renderer = nullptr;
		Ref<Shader> hp_bar_shader;

		// 小地图“点”渲染器
		MultiMeshInstance3D* minimap_dot_renderer = nullptr;
		Ref<Shader> minimap_dot_shader;

		// 护盾渲染器（机甲护盾弧）
		MultiMeshInstance3D* shield_renderer = nullptr;
		Ref<Shader> shield_shader;
		void _setup_shield_renderer();

		HashMap<String, Ref<UnitStats>> unit_types_cache;

	protected:
		static void _bind_methods();

	public:
		UnitManager();
		~UnitManager();

		std::vector<UnitData> units;

		// --- 系统管理 ---
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);
		void _setup_hp_bar_system();
		void _setup_minimap_renderer(int p_width, int p_height, Vector2i p_cell_size);

		// --- 单位生命周期 ---
		int spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id = 0, int p_forced_id = -1);
		void despawn_unit(int p_unit_id, SelectionManager* p_selection_manager);
		void handle_dead_unit(double p_delta);
		void clear_all_units(); // 主机迁移用：清空全部单位

		// --- 单位攻击逻辑 ---
		void command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos);
		void command_units_to_patrol(Array p_unit_ids, Array p_waypoints);
		void command_units_to_attack_target(Array p_unit_ids, int p_target_id, bool p_target_is_building, BuildingManager* p_building_manager);

		// --- 空间网格核心操作 ---
		void update_spatial_grid();
		std::vector<int> get_nearby_units(Vector2 p_world_pos, float p_radius);

		// 供SelectionManager调用
		int get_unit_at_position(Vector2 p_world_pos);
		std::vector<int> get_units_of_type_in_area(Ref<UnitStats> p_stats, Rect2 p_rect, int p_team_id);
		std::vector<int> get_units_in_box(Rect2 p_box, int p_team_id);
		Ref<UnitStats> get_unit_stats(int p_id) { return units[get_unit_index_by_id(p_id)].stats; }
		int get_unit_team_id(int p_id) { return units[get_unit_index_by_id(p_id)].team_id; }

		// --- 核心循环 ---
		void update(double p_delta);
		void physics_update(double p_delta);

		// --- 逻辑计算 ---
		Vector2 get_flow(UnitData& p_unit);
		Vector2 get_separation(UnitData& p_unit);
		Vector2 get_friction(UnitData& p_unit);
		Vector2 get_force(UnitData& p_unit);
		void stop_unit(UnitData& p_unit);
		void update_state(UnitData& p_unit, double p_delta);
		void update_velocity(UnitData& p_unit, double p_delta);
		void move(UnitData& p_unit, double p_delta);

		void update_multimesh_buffer(double p_delta, float p_alpha, SelectionManager* p_selection_manager);

		// 获取数据供 Godot 渲染
		Vector2 get_unit_position(int p_unit_id) const;
		float get_unit_aggro_range(int p_unit_id) const;
		float get_unit_attack_range(int p_unit_id) const;
		int get_unit_state(int p_unit_id) const;
		Ref<UnitStats> get_unit_stats_by_type(String p_type_name);
		PackedStringArray get_registered_unit_types();

		void set_flow_field_manager(Node* p_node);
		void set_group_manager(Node* p_node);
		void set_fog_manager(Node* p_node);
		void set_weapon_manager(Node* p_node);
		void set_effect_manager(Node* p_node);

		void _internal_register_stats(Ref<UnitStats> p_stats);
		void register_unit_type(String p_name, String p_path);
		void register_units_from_dir(String p_dir_path);
		int spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id, int p_forced_id = -1);

		void set_control_group(int p_index, const Array& p_unit_ids);
		Array get_control_group_units_alive(int p_index);
		void remove_unit_from_control_group(int p_unit_id);

		// 攻击模块
		int get_unit_index_by_id(int p_id); 
		void set_attack_manager(Node* p_node);

		//调试
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

		// --- 集中调参 getter/setter (game_tuning) ---
		void set_air_height_threshold(float p_val) { air_height_threshold = p_val; }
		float get_air_height_threshold() const { return air_height_threshold; }
		void set_density_limit(float p_val) { density_limit = p_val; }
		float get_density_limit() const { return density_limit; }
		void set_density_update_interval(float p_val) { density_update_interval = p_val; }
		float get_density_update_interval() const { return density_update_interval; }
		void set_idle_density_factor(float p_val) { idle_density_factor = p_val; }
		float get_idle_density_factor() const { return idle_density_factor; }
		void set_density_next_cell_factor(float p_val) { density_next_cell_factor = p_val; }
		float get_density_next_cell_factor() const { return density_next_cell_factor; }
		void set_arrival_stop_distance_sq(float p_val) { arrival_stop_distance_sq = p_val; }
		float get_arrival_stop_distance_sq() const { return arrival_stop_distance_sq; }
		void set_density_avoidance_strength(float p_val) { density_avoidance_strength = p_val; }
		float get_density_avoidance_strength() const { return density_avoidance_strength; }
		void set_density_avoidance_flow_strength(float p_val) { density_avoidance_flow_strength = p_val; }
		float get_density_avoidance_flow_strength() const { return density_avoidance_flow_strength; }
		void set_separation_extra_radius(float p_val) { separation_extra_radius = p_val; }
		float get_separation_extra_radius() const { return separation_extra_radius; }
		void set_separation_min_dist_factor(float p_val) { separation_min_dist_factor = p_val; }
		float get_separation_min_dist_factor() const { return separation_min_dist_factor; }
		void set_sep_idle_vs_idle_k(float p_val) { sep_idle_vs_idle_k = p_val; }
		float get_sep_idle_vs_idle_k() const { return sep_idle_vs_idle_k; }
		void set_sep_idle_vs_moving_k(float p_val) { sep_idle_vs_moving_k = p_val; }
		float get_sep_idle_vs_moving_k() const { return sep_idle_vs_moving_k; }
		void set_sep_moving_vs_idle_k(float p_val) { sep_moving_vs_idle_k = p_val; }
		float get_sep_moving_vs_idle_k() const { return sep_moving_vs_idle_k; }
		void set_sep_force_to_total_ratio(float p_val) { sep_force_to_total_ratio = p_val; }
		float get_sep_force_to_total_ratio() const { return sep_force_to_total_ratio; }
		void set_target_integration_factor(float p_val) { target_integration_factor = p_val; }
		float get_target_integration_factor() const { return target_integration_factor; }
		void set_critical_area_integration_margin(float p_val) { critical_area_integration_margin = p_val; }
		float get_critical_area_integration_margin() const { return critical_area_integration_margin; }
		void set_arrival_desired_distance_factor(float p_val) { arrival_desired_distance_factor = p_val; }
		float get_arrival_desired_distance_factor() const { return arrival_desired_distance_factor; }
		void set_soft_arrival_factor(float p_val) { soft_arrival_factor = p_val; }
		float get_soft_arrival_factor() const { return soft_arrival_factor; }
		void set_soft_arrival_neighbor_radius_factor(float p_val) { soft_arrival_neighbor_radius_factor = p_val; }
		float get_soft_arrival_neighbor_radius_factor() const { return soft_arrival_neighbor_radius_factor; }
		void set_stuck_check_interval(float p_val) { stuck_check_interval = p_val; }
		float get_stuck_check_interval() const { return stuck_check_interval; }
		void set_stuck_threshold_move_factor(float p_val) { stuck_threshold_move_factor = p_val; }
		float get_stuck_threshold_move_factor() const { return stuck_threshold_move_factor; }
		void set_stuck_threshold_min(float p_val) { stuck_threshold_min = p_val; }
		float get_stuck_threshold_min() const { return stuck_threshold_min; }
		void set_stuck_rotation_threshold_factor(float p_val) { stuck_rotation_threshold_factor = p_val; }
		float get_stuck_rotation_threshold_factor() const { return stuck_rotation_threshold_factor; }
		void set_stuck_give_up_time(float p_val) { stuck_give_up_time = p_val; }
		float get_stuck_give_up_time() const { return stuck_give_up_time; }
		void set_path_recheck_interval(float p_val) { path_recheck_interval = p_val; }
		float get_path_recheck_interval() const { return path_recheck_interval; }
		void set_chase_path_recheck_interval(float p_val) { chase_path_recheck_interval = p_val; }
		float get_chase_path_recheck_interval() const { return chase_path_recheck_interval; }
		void set_arrival_slow_radius_factor(float p_val) { arrival_slow_radius_factor = p_val; }
		float get_arrival_slow_radius_factor() const { return arrival_slow_radius_factor; }
		void set_arrival_min_speed_factor(float p_val) { arrival_min_speed_factor = p_val; }
		float get_arrival_min_speed_factor() const { return arrival_min_speed_factor; }
		void set_chase_slow_down_factor(float p_val) { chase_slow_down_factor = p_val; }
		float get_chase_slow_down_factor() const { return chase_slow_down_factor; }
		void set_chase_min_speed_factor(float p_val) { chase_min_speed_factor = p_val; }
		float get_chase_min_speed_factor() const { return chase_min_speed_factor; }
		void set_rubberband_sensitivity(float p_val) { rubberband_sensitivity = p_val; }
		float get_rubberband_sensitivity() const { return rubberband_sensitivity; }
		void set_rubberband_speed_min(float p_val) { rubberband_speed_min = p_val; }
		float get_rubberband_speed_min() const { return rubberband_speed_min; }
		void set_rubberband_speed_max(float p_val) { rubberband_speed_max = p_val; }
		float get_rubberband_speed_max() const { return rubberband_speed_max; }
		void set_engine_forward_ratio(float p_val) { engine_forward_ratio = p_val; }
		float get_engine_forward_ratio() const { return engine_forward_ratio; }
		void set_propulsion_force_scale(float p_val) { propulsion_force_scale = p_val; }
		float get_propulsion_force_scale() const { return propulsion_force_scale; }
		void set_propulsion_min_power(float p_val) { propulsion_min_power = p_val; }
		float get_propulsion_min_power() const { return propulsion_min_power; }
		void set_idle_friction_multiplier(float p_val) { idle_friction_multiplier = p_val; }
		float get_idle_friction_multiplier() const { return idle_friction_multiplier; }
		void set_turn_ramp_angle(float p_val) { turn_ramp_angle = p_val; }
		float get_turn_ramp_angle() const { return turn_ramp_angle; }
		void set_collision_resolve_radius_factor(float p_val) { collision_resolve_radius_factor = p_val; }
		float get_collision_resolve_radius_factor() const { return collision_resolve_radius_factor; }
		void set_idle_resistance(float p_val) { idle_resistance = p_val; }
		float get_idle_resistance() const { return idle_resistance; }
		void set_collision_smoothing(float p_val) { collision_smoothing = p_val; }
		float get_collision_smoothing() const { return collision_smoothing; }
		void set_wall_collision_factor_moving(float p_val) { wall_collision_factor_moving = p_val; }
		float get_wall_collision_factor_moving() const { return wall_collision_factor_moving; }
		void set_wall_collision_factor_idle(float p_val) { wall_collision_factor_idle = p_val; }
		float get_wall_collision_factor_idle() const { return wall_collision_factor_idle; }
		void set_target_projection_margin(float p_val) { target_projection_margin = p_val; }
		float get_target_projection_margin() const { return target_projection_margin; }
	};
}