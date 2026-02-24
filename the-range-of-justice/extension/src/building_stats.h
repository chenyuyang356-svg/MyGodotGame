#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/core/class_db.hpp> 

namespace godot {

    enum BuildingType {
        BUILDING_GENERIC = 0,
        BUILDING_BARRACKS,    // 兵营
        BUILDING_TURRET,      // 炮塔
        BUILDING_COLLECTOR,   // 资源采集
        BUILDING_STORAGE      // 仓库/人口
    };

    class BuildingStats : public Resource {
        GDCLASS(BuildingStats, Resource)

    private:
        // --- 基础属性 ---
        BuildingType building_type = BUILDING_GENERIC;
        String building_name = "New Building";
        int cost = 100;
        float build_time = 5.0f;
        float health_max = 500.0f;
        Vector2i footprint = Vector2i(1, 1);
        Vector2i clearance_size = Vector2i(1, 1);
        String texture_path;

        // --- 兵营类属性 ---
        PackedStringArray producible_units;
        float production_speed = 1.0f;

        // --- 炮塔类属性 ---
        float attack_damage = 10.0f;
        float attack_range = 300.0f;
        float attack_interval = 1.0f;
        int attack_target_mask = 0; // 0:地面, 1:空中, 2:全部

        // --- 采集类属性 ---
        String resource_type = "Gold";
        float collection_rate = 5.0f;
        float collection_capacity = 100.0f;

    protected:
        static void _bind_methods();

    public:
        BuildingStats();
        ~BuildingStats();

        // --- Getters & Setters ---

        // Base
        void set_building_type(BuildingType p_type) { building_type = p_type; }
        BuildingType get_building_type() const { return building_type; }

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

        void set_texture_path(String p_path) { texture_path = p_path; }
        String get_texture_path() const { return texture_path; }

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

        // Collector
        void set_resource_type(String p_type) { resource_type = p_type; }
        String get_resource_type() const { return resource_type; }

        void set_collection_rate(float p_rate) { collection_rate = p_rate; }
        float get_collection_rate() const { return collection_rate; }

        void set_collection_capacity(float p_cap) { collection_capacity = p_cap; }
        float get_collection_capacity() const { return collection_capacity; }
    };

}

VARIANT_ENUM_CAST(godot::BuildingType);