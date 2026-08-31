#pragma once

#include <vector>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/core/class_db.hpp> 

#include "weapon_stats.h"
#include "game_definitions.h"

namespace godot {

    enum BuildingType {
        BUILDING_GENERIC = 0,
        BUILDING_BARRACKS,    // 兵营
        BUILDING_TURRET,      // 炮塔
        BUILDING_COLLECTOR,   // 资源采集
        BUILDING_STORAGE      // 仓库/人口
    };

    enum PlacementRequirement {
        PLACE_LAND = 1 << 0,      // 01
        PLACE_WATER = 1 << 1,     // 10
        PLACE_ON_RESOURCE = 1 << 2      // 100 
    };

    class BuildingStats : public Resource {
        GDCLASS(BuildingStats, Resource)

    private:
        // 用于翻译
        String building_name_key = "KEY_NEW_BUILDING";
        String building_description_key = "KEY_NO_DESC";
        // --- 基础属性 ---
        BuildingType building_type = BUILDING_GENERIC;
        uint32_t placement_requirement = PLACE_LAND;
        String building_name = "New Building";
        int cost = 100;
        float build_time = 5.0f;
        float health_max = 500.0f;
        float base_height = 2.0f;
        Vector2i footprint = Vector2i(1, 1);
        Vector2i clearance_size = Vector2i(1, 1);
        
        String texture_path;
        int h_frames = 1;
        int v_frames = 1;
        int anim_fps = 10;
        int idle_frames = 1;
        int idle_row = 0;
        int working_frames = 1;
        int working_row = 0;
        int building_frames = 1;
        float working_hold_time = 0.0f; // 工作动画最后一帧暂停秒数（0 = 不暂停）
        int finish_frames = -1; // 生产完成时一次性动画帧数（-1 = 使用 working_frames）

        // 工厂炮塔自转：生产单位时缓慢转动，生产结束保持当前朝向
        bool turret_spin = false;
        float turret_spin_speed = 30.0f; // 度/秒

        // --- 兵营类属性 ---
        PackedStringArray producible_units;
        float production_speed = 1.0f;

        // --- 炮塔类属性 ---
        float attack_damage = 10.0f;
        float attack_range = 300.0f;
        float attack_interval = 1.0f;
        int attack_target_mask = 0; // 0:地面, 1:空中, 2:全部
        float projectile_speed = 0.0f; // 投射物速度 (0代表瞬间命中)
        float splash_radius = 0.0f;    // 溅射半径 (0代表单体伤害)

        // --- 采集类属性 ---
        String resource_type = "Gold";
        float collection_rate = 5.0f;
        float collection_capacity = 100.0f;

        float dying_time = 5.0f;

    protected:
        static void _bind_methods();

    public:
        BuildingStats();
        ~BuildingStats();

        float sight_range = 700.0f;
        std::vector<WeaponMount> weapon_mounts;
        // --- Getters & Setters ---

        void set_building_name_key(const String p_key) { building_name_key = p_key; }
        String get_building_name_key() const { return building_name_key; }
        String get_translated_name() const {
            return TranslationServer::get_singleton()->translate(building_name_key);
        }

        void set_building_description_key(const String p_key) { building_description_key = p_key; }
        String get_building_description_key() const { return building_description_key; }
        String get_translated_description() const {
            return TranslationServer::get_singleton()->translate(building_description_key);
        }

        // Base
        void set_building_type(BuildingType p_type) { building_type = p_type; }
        BuildingType get_building_type() const { return building_type; }

        void set_placement_requirement(uint32_t p_mask) { placement_requirement = p_mask; }
        uint32_t get_placement_requirement() const { return placement_requirement; }

        void set_building_name(String p_name) { building_name = p_name; }
        String get_building_name() const { return building_name; }

        void set_cost(int p_cost) { cost = p_cost; }
        int get_cost() const { return cost; }

        void set_build_time(float p_time) { build_time = p_time; }
        float get_build_time() const { return build_time; }

        void set_health_max(float p_health) { health_max = p_health; }
        float get_health_max() const { return health_max; }

        void set_footprint(Vector2i p_size) { footprint = p_size; }
        Vector2i get_footprint() const { return footprint; }

        void set_clearance_size(Vector2i p_size) { clearance_size = p_size; }
        Vector2i get_clearance_size() const { return clearance_size; }

        void set_base_height(float p_height) { base_height = p_height; }
        float get_base_height() const { return base_height; }

        // Barracks
        void set_producible_units(PackedStringArray p_units) { producible_units = p_units; }
        PackedStringArray get_producible_units() const { return producible_units; }

        void set_production_speed(float p_speed) { production_speed = p_speed; }
        float get_production_speed() const { return production_speed; }

        // Turret
        void set_attack_damage(float p_damage) { attack_damage = p_damage; }
        float get_attack_damage() const { return attack_damage; }

        void set_attack_range(float p_range) { attack_range = p_range; }
        float get_attack_range() const { return attack_range; }

        void set_attack_interval(float p_interval) { attack_interval = p_interval; }
        float get_attack_interval() const { return attack_interval; }

        void set_projectile_speed(float p_speed) { projectile_speed = p_speed; }
        float get_projectile_speed() const { return projectile_speed; }

        void set_splash_radius(float p_radius) { splash_radius = p_radius; }
        float get_splash_radius() const { return splash_radius; }

        // Collector
        void set_resource_type(String p_type) { resource_type = p_type; }
        String get_resource_type() const { return resource_type; }

        void set_collection_rate(float p_rate) { collection_rate = p_rate; }
        float get_collection_rate() const { return collection_rate; }

        void set_collection_capacity(float p_cap) { collection_capacity = p_cap; }
        float get_collection_capacity() const { return collection_capacity; }

        // 纹理路径
        void set_texture_path(const String p_path) { texture_path = p_path; }
        String get_texture_path() const { return texture_path; }

        // 图集分栏
        void set_h_frames(int p_val) { h_frames = p_val; }
        int get_h_frames() const { return h_frames; }

        void set_v_frames(int p_val) { v_frames = p_val; }
        int get_v_frames() const { return v_frames; }

        void set_anim_fps(int p_val) { anim_fps = p_val; }
        int get_anim_fps() const { return anim_fps; }

        void set_idle_frames(int p_val) { idle_frames = p_val; }
        int get_idle_frames() const { return idle_frames; }

        void set_idle_row(int p_val) { idle_row = p_val; }
        int get_idle_row() const { return idle_row; }

        void set_working_frames(int p_val) { working_frames = p_val; }
        int get_working_frames() const { return working_frames; }

        void set_working_row(int p_val) { working_row = p_val; }
        int get_working_row() const { return working_row; }

        void set_building_frames(int p_val) { building_frames = p_val; }
        int get_building_frames() const { return building_frames; }

        void set_working_hold_time(float p_val) { working_hold_time = p_val; }
        float get_working_hold_time() const { return working_hold_time; }

        void set_finish_frames(int p_val) { finish_frames = p_val; }
        int get_finish_frames() const { return finish_frames; }

        // 一次性动画实际使用的帧数（未配置 finish_frames 时回退 working_frames）
        int get_completion_frames() const { return finish_frames > 0 ? finish_frames : working_frames; }

        void set_turret_spin(bool p_val) { turret_spin = p_val; }
        bool get_turret_spin() const { return turret_spin; }

        void set_turret_spin_speed(float p_val) { turret_spin_speed = p_val; }
        float get_turret_spin_speed() const { return turret_spin_speed; }

        void set_dying_time(float p_val) { dying_time = p_val; }
        float get_dying_time() const { return dying_time; }
    
        int get_frames(BuildingState p_state) {
            switch (p_state) {
            case (BuildingState::BUILDING): {
                return building_frames;
            }
            case (BuildingState::IDLE): {
                return idle_frames;
            }
            case (BuildingState::WORKING): {
                return working_frames;
            }
            }
            return 1;
        }
    };

}

VARIANT_BITFIELD_CAST(godot::PlacementRequirement);
VARIANT_ENUM_CAST(godot::BuildingType);