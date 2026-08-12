#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <unordered_map>
#include <vector>
#include "unit_data.h"
#include "flow_field_manager.h"

namespace godot {

    struct UnitGroup {
        int id = -1;
        std::vector<int> unit_ids;   // 存储单位 ID
        int moving_units_count = 0; // 正在移动的单位计数器
        int units_count = 0;
        Vector2 target_pos;
        float average_integration;

        float ground_idle_radius_sq_sum = 0.0f; // 已到达地面单位的半径平方和
        float air_idle_radius_sq_sum = 0.0f; // 已到达空中单位的半径平方和

        float ground_target_integration = 0.0f;
        float air_target_integration = 0.0f;

        UnitGroup() { unit_ids.reserve(256); }

        // O(1) 删除：交换最后一个元素并弹出
        void remove_unit_id(int p_unit_id) {
            for (size_t i = 0; i < unit_ids.size(); ++i) {
                if (unit_ids[i] == p_unit_id) {
                    unit_ids[i] = unit_ids.back();
                    unit_ids.pop_back();
                    return;
                }
            }
        }

        int get_idle_units_count() {
            return units_count - moving_units_count;
        }
    };

    class GroupManager : public Node2D {
        GDCLASS(GroupManager, Node2D)

    friend class GameManager;

    public:
        static const int MAX_CONTROL_GROUPS = 10;
        int next_temp_id = 1;

        // 临时移动组 (Key: ID)
        std::unordered_map<int, UnitGroup> temp_groups;

        // 编队 (固定大小数组)
        std::vector<int> control_groups[MAX_CONTROL_GROUPS];

        double cleanup_timer = 0.0;
        float group_cleanup_interval = 1.0f;   // 每秒检查一次失效组 (可调)

        // --- 组阵型/到达判定可调参数 (game_tuning) ---
        float group_area_margin_factor = 1.0f;      // 阵型所需面积余量（默认1.0保持原行为）
        float group_radius_estimate_factor = 3.0f;  // 阵型半径估算系数（格）
        float group_integration_tolerance = 1.1f;   // 目标集成值容错倍率
        float air_height_threshold = 20.0f;         // 空中/地面判定高度阈值（与 UnitManager 同步）

    protected:
        static void _bind_methods();

    public:
        GroupManager();
        ~GroupManager() = default;

        void update_target_integrations(FlowFieldManager* ffm);

        void set_group_cleanup_interval(float p_val) { group_cleanup_interval = p_val; }
        float get_group_cleanup_interval() const { return group_cleanup_interval; }
        void set_group_area_margin_factor(float p_val) { group_area_margin_factor = p_val; }
        float get_group_area_margin_factor() const { return group_area_margin_factor; }
        void set_group_radius_estimate_factor(float p_val) { group_radius_estimate_factor = p_val; }
        float get_group_radius_estimate_factor() const { return group_radius_estimate_factor; }
        void set_group_integration_tolerance(float p_val) { group_integration_tolerance = p_val; }
        float get_group_integration_tolerance() const { return group_integration_tolerance; }
        void set_air_height_threshold(float p_val) { air_height_threshold = p_val; }
        float get_air_height_threshold() const { return air_height_threshold; }

        // --- 临时组逻辑 ---
        int create_temporary_group(Vector2 p_target_pos);
        void add_unit_to_temp_group(int p_gid, int p_unit_id);
        void remove_unit_from_temp_group(int p_gid, const UnitData& p_unit);

        // 当单位到达目的地时调用，仅减少计数，不删除 ID
        void decrement_moving_count(int p_gid, const UnitData& p_unit);

        // --- 编队逻辑 (0-9) ---
        const std::vector<int>& get_control_group_units(int p_index);

        // --- 生命周期管理 ---
        // 利用单位记录的索引进行 O(1) 到 O(GroupCount) 的快速清理
        void handle_unit_death(const UnitData& p_unit);

        UnitGroup* get_temp_group(int p_gid);
    };
}