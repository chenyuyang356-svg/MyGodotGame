#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "unit_stats.h"
#include "game_definitions.h"
#include "weapon_data.h"

namespace godot {
    enum UnitState {
        IDLE,        // 待机
        MOVING,      // 移动中
        CHASING,     // 追击中
        ATTACKING,   // 攻击中
        PATROLLING,  // 巡逻中
        DYING,      // 死亡中
    };

    struct UnitData {
        int id;                 // 唯一标识符
        Vector2 position;       // 当前世界坐标
        Vector2 velocity;       // 当前速度向量
        float rotation = 0.0f;          // 当前朝向 (弧度)
        float angular_velocity = 0.0f;      // 当前角速度
        Vector2 target_pos;		// 目标的世界坐标
        Vector2 target_pos_offset = Vector2(0, 0);
        Vector2i target_grid;   // 目标的网格坐标（与流场坐标一致，不同于unit_grid中的坐标）
        bool is_manual_target = false;
        bool target_is_building = false;    // 标记当前目标是否为建筑

        Vector2 prev_position;
        Vector2 next_position;

        float prev_height;
        float next_height;

        float prev_rotation;
        float next_rotation;


        Ref<UnitStats> stats;

        UnitState state;        // 状态机		
        float current_health;
        float height = 0.0f;

        float anim_time = 0.0f; // 累计播放时间

        float current_dying_time = 0.0f;

        // 组管理相关数据
        int temp_group_id = -1;
        int control_group_indices[3] = { -1, -1, -1 };
        int control_group_count = 0;

        int team_id = 0;
        int target_id = -1;
        std::vector<float> weapon_cooldowns; //改为只有不独立于单位的武器才使用
        std::vector<WeaponData> weapons; // 这些是单位装载的独立武器

        bool is_patrolling = false;
        std::vector<Vector2> patrol_waypoints;
        int current_waypoint_idx = 0;

        bool use_direct_path = false;
        double path_recheck_timer = 0.0; // 移动中定期重新检查，防止新造的建筑挡路

        Vector2 last_visual_pos;      // 上一帧渲染的视觉位置
        float dust_accumulator = 0.0f; // 扬尘距离累加器
        int emit_count = 0;

        UnitData() : id(-1), state(IDLE), current_health(0) {}

        int get_nav_type() {
            if (stats->move_type == MOVE_GROUND) { return NAV_LAND; }
            else if (stats->move_type == MOVE_SEA) { return NAV_SEA; }
            else if (stats->move_type == MOVE_AIR) { return NAV_AIR; }
            else if (stats->move_type == MOVE_HOVER) { return NAV_HOVER; }
            else { return NAV_LAND; }
        }

        int get_squared_radius() {
            return (stats->collision_radius * stats->collision_radius);
        }
    };
}