#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "building_stats.h"

namespace godot {
    enum class BuildingState {
        BUILDING,
        IDLE,
        WORKING
    };

    struct BuildingData {
        int id;
        Vector2i grid_pos;   // Footprint 左上角坐标
        Ref<BuildingStats> stats;
        int team_id;
        float current_health;
        float anim_time = 0.0f;
        BuildingState state = BuildingState::BUILDING;
    };

}