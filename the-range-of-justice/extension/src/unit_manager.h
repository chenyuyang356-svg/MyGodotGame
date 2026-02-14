#pragma once

#include <vector>
#include <unordered_map>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/multi_mesh_instance2d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>

#include "flow_field_manager.h"
#include "unit_stats.h" // 引用重命名后的头文件

namespace godot {

    class UnitManager : public Node2D {
        GDCLASS(UnitManager, Node2D)

    public:
        enum UnitState {
            IDLE,
            MOVING,
            CHASING,
            ATTACKING
        };

        struct UnitData {
            int id;
            Vector2 position;
            Vector2 velocity;
            Vector2 target_pos;
            Vector2i target_grid;
            float speed;
            float radius;

            int team_id;
            float hp;
            float max_hp;
            float attack_range;
            float attack_damage;
            float attack_interval;
            double last_attack_time;
            int target_unit_id;

            // 配置引用
            Ref<UnitStats> stats;

            float current_rotation;
            float current_hp;
            float current_shield;

            UnitState state;
            double last_regen_time;

            UnitData() :
                id(-1),
                speed(200.0f),
                radius(6.0f),
                state(IDLE),
                team_id(0),
                hp(100.0f),
                max_hp(100.0f),
                attack_range(50.0f),
                attack_damage(10.0f),
                attack_interval(1.0f),
                last_attack_time(-100.0),
                target_unit_id(-1)
            {
            }
        };

    private:
        FlowFieldManager* flow_field_manager;
        std::vector<UnitData> units;
        std::unordered_map<int, size_t> id_to_index;
        int next_unit_id = 0;
        std::vector<std::vector<int>> unit_grid;
        int unit_grid_width = 0;
        int unit_grid_height = 0;
        int unit_grid_size = 0;
        Vector2i unit_grid_cell_size = Vector2i(0, 0);
        float flow_factor = 2000;
        float separation_factor = 10000;
        float separation_limit = 1000;
        float friction_factor = 100;
        bool is_setup = false;

		bool is_setup = false;
		MultiMeshInstance2D* multimesh_instance = nullptr;
    protected:
        static void _bind_methods();

    public:
        UnitManager();
        ~UnitManager();

        void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);

        // [重点修改] 这里改成了 Ref<UnitStats>
        int spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id);

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
		Vector2 get_flow(const UnitData& p_unit);
		Vector2 get_separation(const UnitData& p_unit);
		Vector2 get_friction(const UnitData& p_unit);
		Vector2 get_force(const UnitData& p_unit);
		void update_velocity(UnitData& p_unit, double p_delta);
		void move(UnitData& p_unit, double p_delta);
		void update_multimesh_buffer();

		// 获取数据供 Godot 渲染
		Vector2 get_unit_position(int p_unit_id) const;
		int get_unit_state(int p_unit_id) const;
		void set_multimesh_instance(Node* p_node);
		void set_flow_field_manager(Node* p_node);

        int get_unit_state(int p_unit_id) const;
        int get_unit_team(int p_unit_id) const;
    };
}

// 注意：这里不需要注册 UnitType 了，因为它被 UnitStats 取代了
VARIANT_ENUM_CAST(UnitManager::UnitState);