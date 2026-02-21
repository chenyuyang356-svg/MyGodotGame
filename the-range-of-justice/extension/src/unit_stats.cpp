#include "unit_stats.h"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void UnitStats::_bind_methods() {
    // 1. 绑定枚举常量
    BIND_ENUM_CONSTANT(ARMOR_LIGHT);
    BIND_ENUM_CONSTANT(ARMOR_HEAVY);
    BIND_ENUM_CONSTANT(ARMOR_BUILDING);
    BIND_ENUM_CONSTANT(ARMOR_HERO);

    BIND_ENUM_CONSTANT(ATTACK_PHYSICAL);
    BIND_ENUM_CONSTANT(ATTACK_MAGIC);
    BIND_ENUM_CONSTANT(ATTACK_SIEGE);

    BIND_ENUM_CONSTANT(MOVE_GROUND);
    BIND_ENUM_CONSTANT(MOVE_AIR);
    BIND_ENUM_CONSTANT(MOVE_HOVER);

    BIND_ENUM_CONSTANT(PRIORITY_CLOSEST);
    BIND_ENUM_CONSTANT(PRIORITY_LOWEST_HP);
    BIND_ENUM_CONSTANT(PRIORITY_HIGHEST_VALUE);

    BIND_BITFIELD_FLAG(TAG_NONE);
    BIND_BITFIELD_FLAG(TAG_BIOLOGICAL);
    BIND_BITFIELD_FLAG(TAG_MECHANICAL);
    BIND_BITFIELD_FLAG(TAG_SUMMONED);
    BIND_BITFIELD_FLAG(TAG_HERO);

    // 2. 绑定属性 (修复了 suffix:s 的位置)

    ADD_GROUP("Survival", "");
    ClassDB::bind_method(D_METHOD("set_health_max", "value"), &UnitStats::set_health_max);
    ClassDB::bind_method(D_METHOD("get_health_max"), &UnitStats::get_health_max);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "health_max", PROPERTY_HINT_RANGE, "1,10000,1"), "set_health_max", "get_health_max");

    ClassDB::bind_method(D_METHOD("set_health_regen", "value"), &UnitStats::set_health_regen);
    ClassDB::bind_method(D_METHOD("get_health_regen"), &UnitStats::get_health_regen);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "health_regen"), "set_health_regen", "get_health_regen");

    ClassDB::bind_method(D_METHOD("set_shield_max", "value"), &UnitStats::set_shield_max);
    ClassDB::bind_method(D_METHOD("get_shield_max"), &UnitStats::get_shield_max);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shield_max"), "set_shield_max", "get_shield_max");

    ClassDB::bind_method(D_METHOD("set_shield_regen", "value"), &UnitStats::set_shield_regen);
    ClassDB::bind_method(D_METHOD("get_shield_regen"), &UnitStats::get_shield_regen);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "shield_regen"), "set_shield_regen", "get_shield_regen");

    ClassDB::bind_method(D_METHOD("set_armor_type", "value"), &UnitStats::set_armor_type);
    ClassDB::bind_method(D_METHOD("get_armor_type"), &UnitStats::get_armor_type);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "armor_type", PROPERTY_HINT_ENUM, "Light,Heavy,Building,Hero"), "set_armor_type", "get_armor_type");

    ADD_GROUP("Attack", "");
    ClassDB::bind_method(D_METHOD("set_attack_damage", "value"), &UnitStats::set_attack_damage);
    ClassDB::bind_method(D_METHOD("get_attack_damage"), &UnitStats::get_attack_damage);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_damage"), "set_attack_damage", "get_attack_damage");

    ClassDB::bind_method(D_METHOD("set_attack_range", "value"), &UnitStats::set_attack_range);
    ClassDB::bind_method(D_METHOD("get_attack_range"), &UnitStats::get_attack_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_range", PROPERTY_HINT_RANGE, "0,2000,1"), "set_attack_range", "get_attack_range");

    ClassDB::bind_method(D_METHOD("set_attack_interval", "value"), &UnitStats::set_attack_interval);
    ClassDB::bind_method(D_METHOD("get_attack_interval"), &UnitStats::get_attack_interval);
    // [修复] suffix:s 放入引号内
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_interval", PROPERTY_HINT_RANGE, "0.01,10,0.01,suffix:s"), "set_attack_interval", "get_attack_interval");

    ClassDB::bind_method(D_METHOD("set_attack_type", "value"), &UnitStats::set_attack_type);
    ClassDB::bind_method(D_METHOD("get_attack_type"), &UnitStats::get_attack_type);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "attack_type", PROPERTY_HINT_ENUM, "Physical,Magic,Siege"), "set_attack_type", "get_attack_type");

    ClassDB::bind_method(D_METHOD("set_splash_radius", "value"), &UnitStats::set_splash_radius);
    ClassDB::bind_method(D_METHOD("get_splash_radius"), &UnitStats::get_splash_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "splash_radius"), "set_splash_radius", "get_splash_radius");

    ClassDB::bind_method(D_METHOD("set_projectile_speed", "value"), &UnitStats::set_projectile_speed);
    ClassDB::bind_method(D_METHOD("get_projectile_speed"), &UnitStats::get_projectile_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "projectile_speed"), "set_projectile_speed", "get_projectile_speed");

    ClassDB::bind_method(D_METHOD("set_target_priority", "value"), &UnitStats::set_target_priority);
    ClassDB::bind_method(D_METHOD("get_target_priority"), &UnitStats::get_target_priority);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "target_priority", PROPERTY_HINT_ENUM, "Closest,Lowest HP,Highest Value"), "set_target_priority", "get_target_priority");

    ADD_GROUP("Movement", "");
    ClassDB::bind_method(D_METHOD("set_move_speed", "value"), &UnitStats::set_move_speed);
    ClassDB::bind_method(D_METHOD("get_move_speed"), &UnitStats::get_move_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "move_speed"), "set_move_speed", "get_move_speed");

    ClassDB::bind_method(D_METHOD("set_acceleration", "value"), &UnitStats::set_acceleration);
    ClassDB::bind_method(D_METHOD("get_acceleration"), &UnitStats::get_acceleration);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

    ClassDB::bind_method(D_METHOD("set_turn_speed", "value"), &UnitStats::set_turn_speed);
    ClassDB::bind_method(D_METHOD("get_turn_speed"), &UnitStats::get_turn_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_speed"), "set_turn_speed", "get_turn_speed");

    ClassDB::bind_method(D_METHOD("set_turn_acceleration", "value"), &UnitStats::set_turn_acceleration);
    ClassDB::bind_method(D_METHOD("get_turn_acceleration"), &UnitStats::get_turn_acceleration);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_acceleration"), "set_turn_acceleration", "get_turn_acceleration");

    ClassDB::bind_method(D_METHOD("set_collision_radius", "value"), &UnitStats::set_collision_radius);
    ClassDB::bind_method(D_METHOD("get_collision_radius"), &UnitStats::get_collision_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_radius"), "set_collision_radius", "get_collision_radius");

    ClassDB::bind_method(D_METHOD("set_base_height", "value"), &UnitStats::set_base_height);
    ClassDB::bind_method(D_METHOD("get_base_height"), &UnitStats::get_base_height);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "base_height"), "set_base_height", "get_base_height");

    ClassDB::bind_method(D_METHOD("set_move_type", "value"), &UnitStats::set_move_type);
    ClassDB::bind_method(D_METHOD("get_move_type"), &UnitStats::get_move_type);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "move_type", PROPERTY_HINT_ENUM, "Ground,Air,Hover"), "set_move_type", "get_move_type");

    ADD_GROUP("Vision", "");
    ClassDB::bind_method(D_METHOD("set_sight_range", "value"), &UnitStats::set_sight_range);
    ClassDB::bind_method(D_METHOD("get_sight_range"), &UnitStats::get_sight_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "sight_range"), "set_sight_range", "get_sight_range");

    ClassDB::bind_method(D_METHOD("set_aggro_range", "value"), &UnitStats::set_aggro_range);
    ClassDB::bind_method(D_METHOD("get_aggro_range"), &UnitStats::get_aggro_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "aggro_range"), "set_aggro_range", "get_aggro_range");

    ADD_GROUP("Economy", "");
    ClassDB::bind_method(D_METHOD("set_cost", "value"), &UnitStats::set_cost);
    ClassDB::bind_method(D_METHOD("get_cost"), &UnitStats::get_cost);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cost"), "set_cost", "get_cost");

    ClassDB::bind_method(D_METHOD("set_build_time", "value"), &UnitStats::set_build_time);
    ClassDB::bind_method(D_METHOD("get_build_time"), &UnitStats::get_build_time);
    // [修复] suffix:s 放入引号内
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "build_time", PROPERTY_HINT_RANGE, "0,1000,0.1,suffix:s"), "set_build_time", "get_build_time");

    ADD_GROUP("Misc", "");
    ClassDB::bind_method(D_METHOD("set_unit_tags", "value"), &UnitStats::set_unit_tags);
    ClassDB::bind_method(D_METHOD("get_unit_tags"), &UnitStats::get_unit_tags);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "unit_tags", PROPERTY_HINT_FLAGS, "Biological,Mechanical,Summoned,Hero"), "set_unit_tags", "get_unit_tags");

    // 1. 绑定纹理路径和图集规格 (h_frames/v_frames)
    ClassDB::bind_method(D_METHOD("get_texture_path"), &UnitStats::get_texture_path);
    ClassDB::bind_method(D_METHOD("set_texture_path", "p_path"), &UnitStats::set_texture_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path"), "set_texture_path", "get_texture_path");

    ClassDB::bind_method(D_METHOD("get_h_frames"), &UnitStats::get_h_frames);
    ClassDB::bind_method(D_METHOD("set_h_frames", "p_val"), &UnitStats::set_h_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "h_frames"), "set_h_frames", "get_h_frames");

    ClassDB::bind_method(D_METHOD("get_v_frames"), &UnitStats::get_v_frames);
    ClassDB::bind_method(D_METHOD("set_v_frames", "p_val"), &UnitStats::set_v_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "v_frames"), "set_v_frames", "get_v_frames");

    // 2. 绑定动画帧数 (move_frames/idle_frames)
    ClassDB::bind_method(D_METHOD("get_move_frames"), &UnitStats::get_move_frames);
    ClassDB::bind_method(D_METHOD("set_move_frames", "p_val"), &UnitStats::set_move_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "move_frames"), "set_move_frames", "get_move_frames");

    ClassDB::bind_method(D_METHOD("get_idle_frames"), &UnitStats::get_idle_frames);
    ClassDB::bind_method(D_METHOD("set_idle_frames", "p_val"), &UnitStats::set_idle_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_frames"), "set_idle_frames", "get_idle_frames");

    // 3. 绑定动画行号 (move_row/idle_row)
    ClassDB::bind_method(D_METHOD("get_move_row"), &UnitStats::get_move_row);
    ClassDB::bind_method(D_METHOD("set_move_row", "p_val"), &UnitStats::set_move_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "move_row"), "set_move_row", "get_move_row");

    ClassDB::bind_method(D_METHOD("get_idle_row"), &UnitStats::get_idle_row);
    ClassDB::bind_method(D_METHOD("set_idle_row", "p_val"), &UnitStats::set_idle_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_row"), "set_idle_row", "get_idle_row");

    // 4. 绑定播放速度 (anim_fps)
    ClassDB::bind_method(D_METHOD("get_anim_fps"), &UnitStats::get_anim_fps);
    ClassDB::bind_method(D_METHOD("set_anim_fps", "p_val"), &UnitStats::set_anim_fps);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "anim_fps"), "set_anim_fps", "get_anim_fps");
}