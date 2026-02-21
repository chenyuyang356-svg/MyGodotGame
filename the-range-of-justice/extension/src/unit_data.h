#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "unit_stats.h"

namespace godot {
    enum UnitState {
        IDLE,        // 待机
        MOVING,      // 移动中
        CHASING,     // 追击中
        ATTACKING,   // 攻击中
        PATROLLING,  // 巡逻中
    };

    struct UnitData {
        int id;                 // 唯一标识符
        Vector2 position;       // 当前世界坐标
        Vector2 velocity;       // 当前速度向量
        Vector2 target_pos;		// 目标的世界坐标
        Vector2i target_grid;   // 目标的网格坐标（与流场坐标一致，不同于unit_grid中的坐标）

        Ref<UnitStats> stats;

        UnitState state;        // 状态机		
        bool is_selected = false;
        bool is_mouse_on = false;
        float selection_radius;
        float current_health;

        float anim_time = 0.0f; // 累计播放时间

        // 组管理相关数据
        int temp_group_id = -1;
        int control_group_indices[3] = { -1, -1, -1 };
        int control_group_count = 0;

        int team_id = 0;
        int target_id = -1;
        float attack_cooldown = 0.1f;

        bool is_patrolling = false;
        std::vector<Vector2> patrol_waypoints;
        int current_waypoint_idx = 0;

        UnitData() : id(-1), state(IDLE), current_health(0) {}
    };
}