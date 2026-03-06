#include "building_stats.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

BuildingStats::BuildingStats() {}
BuildingStats::~BuildingStats() {}

void BuildingStats::_bind_methods() {
    // 绑定枚举
    BIND_ENUM_CONSTANT(BUILDING_GENERIC);
    BIND_ENUM_CONSTANT(BUILDING_BARRACKS);
    BIND_ENUM_CONSTANT(BUILDING_TURRET);
    BIND_ENUM_CONSTANT(BUILDING_COLLECTOR);
    BIND_ENUM_CONSTANT(BUILDING_STORAGE);

    BIND_ENUM_CONSTANT(PLACE_LAND);
    BIND_ENUM_CONSTANT(PLACE_WATER);
    BIND_ENUM_CONSTANT(PLACE_ON_RESOURCE);

    // --- 基础属性绑定 ---
    ClassDB::bind_method(D_METHOD("get_building_type"), &BuildingStats::get_building_type);
    ClassDB::bind_method(D_METHOD("set_building_type", "p_val"), &BuildingStats::set_building_type);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "building_type", PROPERTY_HINT_ENUM, "Generic,Barracks,Turret,Collector,Storage"), "set_building_type", "get_building_type");

    ClassDB::bind_method(D_METHOD("get_placement_requirement"), &BuildingStats::get_placement_requirement);
    ClassDB::bind_method(D_METHOD("set_placement_requirement", "p_mask"), &BuildingStats::set_placement_requirement);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "placement_requirement", PROPERTY_HINT_FLAGS, "Land:1,Water:2,Coast:4"), "set_placement_requirement", "get_placement_requirement");

    ClassDB::bind_method(D_METHOD("get_building_name"), &BuildingStats::get_building_name);
    ClassDB::bind_method(D_METHOD("set_building_name", "p_val"), &BuildingStats::set_building_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "building_name"), "set_building_name", "get_building_name");

    ClassDB::bind_method(D_METHOD("get_cost"), &BuildingStats::get_cost);
    ClassDB::bind_method(D_METHOD("set_cost", "p_val"), &BuildingStats::set_cost);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cost"), "set_cost", "get_cost");

    ClassDB::bind_method(D_METHOD("get_build_time"), &BuildingStats::get_build_time);
    ClassDB::bind_method(D_METHOD("set_build_time", "p_val"), &BuildingStats::set_build_time);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "build_time", PROPERTY_HINT_RANGE, "0,600,0.1,suffix:s"), "set_build_time", "get_build_time");

    ClassDB::bind_method(D_METHOD("get_health_max"), &BuildingStats::get_health_max);
    ClassDB::bind_method(D_METHOD("set_health_max", "p_val"), &BuildingStats::set_health_max);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "health_max"), "set_health_max", "get_health_max");

    ClassDB::bind_method(D_METHOD("get_footprint"), &BuildingStats::get_footprint);
    ClassDB::bind_method(D_METHOD("set_footprint", "p_val"), &BuildingStats::set_footprint);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "footprint"), "set_footprint", "get_footprint");

    ClassDB::bind_method(D_METHOD("get_clearance_size"), &BuildingStats::get_clearance_size);
    ClassDB::bind_method(D_METHOD("set_clearance_size", "p_val"), &BuildingStats::set_clearance_size);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "clearance_size"), "set_clearance_size", "get_clearance_size");

    ClassDB::bind_method(D_METHOD("get_base_height"), &BuildingStats::get_base_height);
    ClassDB::bind_method(D_METHOD("set_base_height", "p_val"), &BuildingStats::set_base_height);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_height"), "set_base_height", "get_base_height");

    // --- 兵营属性绑定 ---
    ADD_GROUP("Barracks", "production_");
    ClassDB::bind_method(D_METHOD("get_producible_units"), &BuildingStats::get_producible_units);
    ClassDB::bind_method(D_METHOD("set_producible_units", "p_val"), &BuildingStats::set_producible_units);
    ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "producible_units"), "set_producible_units", "get_producible_units");

    ClassDB::bind_method(D_METHOD("get_production_speed"), &BuildingStats::get_production_speed);
    ClassDB::bind_method(D_METHOD("set_production_speed", "p_val"), &BuildingStats::set_production_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "production_speed"), "set_production_speed", "get_production_speed");

    // --- 炮塔属性绑定 ---
    ADD_GROUP("Turret", "attack_");
    ClassDB::bind_method(D_METHOD("get_attack_damage"), &BuildingStats::get_attack_damage);
    ClassDB::bind_method(D_METHOD("set_attack_damage", "p_val"), &BuildingStats::set_attack_damage);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_damage"), "set_attack_damage", "get_attack_damage");

    ClassDB::bind_method(D_METHOD("get_attack_range"), &BuildingStats::get_attack_range);
    ClassDB::bind_method(D_METHOD("set_attack_range", "p_val"), &BuildingStats::set_attack_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_range"), "set_attack_range", "get_attack_range");

    ClassDB::bind_method(D_METHOD("get_attack_interval"), &BuildingStats::get_attack_interval);
    ClassDB::bind_method(D_METHOD("set_attack_interval", "p_val"), &BuildingStats::set_attack_interval);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_interval"), "set_attack_interval", "get_attack_interval");

    ClassDB::bind_method(D_METHOD("get_projectile_speed"), &BuildingStats::get_projectile_speed);
    ClassDB::bind_method(D_METHOD("set_projectile_speed", "p_val"), &BuildingStats::set_projectile_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "projectile_speed"), "set_projectile_speed", "get_projectile_speed");

    ClassDB::bind_method(D_METHOD("get_splash_radius"), &BuildingStats::get_splash_radius);
    ClassDB::bind_method(D_METHOD("set_splash_radius", "p_val"), &BuildingStats::set_splash_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "splash_radius"), "set_splash_radius", "get_splash_radius");

    // --- 采集属性绑定 ---
    ADD_GROUP("Collector", "collection_");
    ClassDB::bind_method(D_METHOD("get_resource_type"), &BuildingStats::get_resource_type);
    ClassDB::bind_method(D_METHOD("set_resource_type", "p_val"), &BuildingStats::set_resource_type);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "resource_type"), "set_resource_type", "get_resource_type");

    ClassDB::bind_method(D_METHOD("get_collection_rate"), &BuildingStats::get_collection_rate);
    ClassDB::bind_method(D_METHOD("set_collection_rate", "p_val"), &BuildingStats::set_collection_rate);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collection_rate"), "set_collection_rate", "get_collection_rate");

    ClassDB::bind_method(D_METHOD("get_collection_capacity"), &BuildingStats::get_collection_capacity);
    ClassDB::bind_method(D_METHOD("set_collection_capacity", "p_val"), &BuildingStats::set_collection_capacity);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collection_capacity"), "set_collection_capacity", "get_collection_capacity");

    ClassDB::bind_method(D_METHOD("get_texture_path"), &BuildingStats::get_texture_path);
    ClassDB::bind_method(D_METHOD("set_texture_path", "p_path"), &BuildingStats::set_texture_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path"), "set_texture_path", "get_texture_path");

    ClassDB::bind_method(D_METHOD("get_h_frames"), &BuildingStats::get_h_frames);
    ClassDB::bind_method(D_METHOD("set_h_frames", "p_val"), &BuildingStats::set_h_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "h_frames"), "set_h_frames", "get_h_frames");

    ClassDB::bind_method(D_METHOD("get_v_frames"), &BuildingStats::get_v_frames);
    ClassDB::bind_method(D_METHOD("set_v_frames", "p_val"), &BuildingStats::set_v_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "v_frames"), "set_v_frames", "get_v_frames");

    ClassDB::bind_method(D_METHOD("get_anim_fps"), &BuildingStats::get_anim_fps);
    ClassDB::bind_method(D_METHOD("set_anim_fps", "p_val"), &BuildingStats::set_anim_fps);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "anim_fps"), "set_anim_fps", "get_anim_fps");

    ClassDB::bind_method(D_METHOD("get_idle_frames"), &BuildingStats::get_idle_frames);
    ClassDB::bind_method(D_METHOD("set_idle_frames", "p_val"), &BuildingStats::set_idle_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_frames"), "set_idle_frames", "get_idle_frames");

    ClassDB::bind_method(D_METHOD("get_idle_row"), &BuildingStats::get_idle_row);
    ClassDB::bind_method(D_METHOD("set_idle_row", "p_val"), &BuildingStats::set_idle_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_row"), "set_idle_row", "get_idle_row");

    ClassDB::bind_method(D_METHOD("get_working_frames"), &BuildingStats::get_working_frames);
    ClassDB::bind_method(D_METHOD("set_working_frames", "p_val"), &BuildingStats::set_working_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "working_frames"), "set_working_frames", "get_working_frames");

    ClassDB::bind_method(D_METHOD("get_working_row"), &BuildingStats::get_working_row);
    ClassDB::bind_method(D_METHOD("set_working_row", "p_val"), &BuildingStats::set_working_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "working_row"), "set_working_row", "get_working_row");
}