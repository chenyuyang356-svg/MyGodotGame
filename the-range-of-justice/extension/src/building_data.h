#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "building_stats.h"

namespace godot {
    enum class BuildingState {
        BUILDING,
        IDLE,
        WORKING,
        DYING
    };

    struct BuildingData {
        int id;
        Vector2i grid_pos;   // Footprint ×óÉÏ½Ç×ø±ê
        Ref<BuildingStats> stats;
        int team_id;
        float current_health;
        float anim_time = 0.0f;

        float build_timer = 0.0f;
        BuildingState state = BuildingState::BUILDING;

        // ½¨ÖþµÄ¹¥»÷ÀäÈ´ºÍÄ¿±êÐÅÏ¢
        int target_id = -1;
        bool target_is_building = false;
        float attack_cooldown = 0.0f;
        std::vector<String> production_queue; // ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Î»ï¿½ï¿½ï¿½ï¿½ï¿½Ð±ï¿½
        float unit_production_timer = 0.0f;    // ï¿½ï¿½Ç°ï¿½ï¿½Î»ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½Ê±

        float current_dying_time = 0.0f;
    };

}