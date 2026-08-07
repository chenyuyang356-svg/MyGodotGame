#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "building_stats.h"
#include "weapon_data.h"
#include "game_definitions.h"

namespace godot {
    struct BuildingData {
        int id;
        Vector2i grid_pos;   
        Ref<BuildingStats> stats;
        int team_id;
        float current_health;
        float anim_time = 0.0f;

        float prev_progress_percent = 0.0f; // 上一个逻辑帧的进度
        float next_progress_percent = 0.0f; // 目标进度（逻辑帧）

        // 建造
        float build_timer = 0.0f;
        BuildingState state = BuildingState::BUILDING;

        // 攻击
        int target_id = -1;
        bool target_is_building = false;
        float attack_cooldown = 0.0f;
        std::vector<WeaponData> weapons;

        // 产兵
        std::vector<String> production_queue; 
        float unit_production_timer = 0.0f; 

        float current_dying_time = 0.0f;
    };

}