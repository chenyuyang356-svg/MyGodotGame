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
        Vector2i grid_pos;   // Footprint ���Ͻ�����
        Ref<BuildingStats> stats;
        int team_id;
        float current_health;
        float anim_time = 0.0f;

        float build_timer = 0.0f;
        BuildingState state = BuildingState::BUILDING;

        // �����Ĺ�����ȴ��Ŀ����Ϣ
        int target_id = -1;
        bool target_is_building = false;
        float attack_cooldown = 0.0f;
        std::vector<String> production_queue; // ��������λ�����б�
        float unit_production_timer = 0.0f;    // ��ǰ��λ��������ʱ
    };

}