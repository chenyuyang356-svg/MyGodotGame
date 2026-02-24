#pragma once
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/classes/ref.hpp>
#include "building_stats.h"

namespace godot {

    struct BuildingData {
        int id;
        Vector2i grid_pos;   // Footprint 左上角坐标
        Ref<BuildingStats> stats;
        int team_id;
        float current_health;
    };

}