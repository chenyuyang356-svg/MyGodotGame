#include "weapon_stats.h"

using namespace godot;

void WeaponStats::_bind_methods() {
    // 基础属性
    ClassDB::bind_method(D_METHOD("set_weapon_name", "val"), &WeaponStats::set_weapon_name);
    ClassDB::bind_method(D_METHOD("get_weapon_name"), &WeaponStats::get_weapon_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "weapon_name"), "set_weapon_name", "get_weapon_name");

    // 战斗属性
    ClassDB::bind_method(D_METHOD("set_projectile_type_name", "val"), &WeaponStats::set_projectile_type_name);
    ClassDB::bind_method(D_METHOD("get_projectile_type_name"), &WeaponStats::get_projectile_type_name);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "projectile_type_name"), "set_projectile_type_name", "get_projectile_type_name");

    ClassDB::bind_method(D_METHOD("set_damage", "val"), &WeaponStats::set_damage);
    ClassDB::bind_method(D_METHOD("get_damage"), &WeaponStats::get_damage);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");

    ClassDB::bind_method(D_METHOD("set_attack_range", "val"), &WeaponStats::set_attack_range);
    ClassDB::bind_method(D_METHOD("get_attack_range"), &WeaponStats::get_attack_range);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_range"), "set_attack_range", "get_attack_range");

    ClassDB::bind_method(D_METHOD("set_attack_interval", "val"), &WeaponStats::set_attack_interval);
    ClassDB::bind_method(D_METHOD("get_attack_interval"), &WeaponStats::get_attack_interval);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "attack_interval"), "set_attack_interval", "get_attack_interval");

    ClassDB::bind_method(D_METHOD("set_projectile_speed", "val"), &WeaponStats::set_projectile_speed);
    ClassDB::bind_method(D_METHOD("get_projectile_speed"), &WeaponStats::get_projectile_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "projectile_speed"), "set_projectile_speed", "get_projectile_speed");

    ClassDB::bind_method(D_METHOD("set_splash_radius", "val"), &WeaponStats::set_splash_radius);
    ClassDB::bind_method(D_METHOD("get_splash_radius"), &WeaponStats::get_splash_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "splash_radius"), "set_splash_radius", "get_splash_radius");

    ClassDB::bind_method(D_METHOD("set_can_attack_ground", "p_val"), &WeaponStats::set_can_attack_ground);
    ClassDB::bind_method(D_METHOD("get_can_attack_ground"), &WeaponStats::get_can_attack_ground);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_attack_ground"), "set_can_attack_ground", "get_can_attack_ground");

    ClassDB::bind_method(D_METHOD("set_can_attack_air", "p_val"), &WeaponStats::set_can_attack_air);
    ClassDB::bind_method(D_METHOD("get_can_attack_air"), &WeaponStats::get_can_attack_air);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "can_attack_air"), "set_can_attack_air", "get_can_attack_air");

    // 旋转系统属性
    ClassDB::bind_method(D_METHOD("set_is_turret", "val"), &WeaponStats::set_is_turret);
    ClassDB::bind_method(D_METHOD("get_is_turret"), &WeaponStats::get_is_turret);
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_turret"), "set_is_turret", "get_is_turret");

    ClassDB::bind_method(D_METHOD("set_turn_speed", "val"), &WeaponStats::set_turn_speed);
    ClassDB::bind_method(D_METHOD("get_turn_speed"), &WeaponStats::get_turn_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_speed"), "set_turn_speed", "get_turn_speed");

    ClassDB::bind_method(D_METHOD("set_rotation_center", "p_val"), &WeaponStats::set_rotation_center);
    ClassDB::bind_method(D_METHOD("get_rotation_center"), &WeaponStats::get_rotation_center);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "rotation_center"), "set_rotation_center", "get_rotation_center");

    ClassDB::bind_method(D_METHOD("set_muzzle_offset", "p_val"), &WeaponStats::set_muzzle_offset);
    ClassDB::bind_method(D_METHOD("get_muzzle_offset"), &WeaponStats::get_muzzle_offset);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "muzzle_offset"), "set_muzzle_offset", "get_muzzle_offset");

    ClassDB::bind_method(D_METHOD("set_firing_tolerance", "p_val"), &WeaponStats::set_firing_tolerance);
    ClassDB::bind_method(D_METHOD("get_firing_tolerance"), &WeaponStats::get_firing_tolerance);

    // 渲染与动画属性
    ClassDB::bind_method(D_METHOD("set_texture_path", "val"), &WeaponStats::set_texture_path);
    ClassDB::bind_method(D_METHOD("get_texture_path"), &WeaponStats::get_texture_path);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path"), "set_texture_path", "get_texture_path");

    ClassDB::bind_method(D_METHOD("set_h_frames", "val"), &WeaponStats::set_h_frames);
    ClassDB::bind_method(D_METHOD("get_h_frames"), &WeaponStats::get_h_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "h_frames"), "set_h_frames", "get_h_frames");

    ClassDB::bind_method(D_METHOD("set_v_frames", "val"), &WeaponStats::set_v_frames);
    ClassDB::bind_method(D_METHOD("get_v_frames"), &WeaponStats::get_v_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "v_frames"), "set_v_frames", "get_v_frames");

    ClassDB::bind_method(D_METHOD("set_idle_frames", "val"), &WeaponStats::set_idle_frames);
    ClassDB::bind_method(D_METHOD("get_idle_frames"), &WeaponStats::get_idle_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_frames"), "set_idle_frames", "get_idle_frames");

    ClassDB::bind_method(D_METHOD("set_attacking_frames", "val"), &WeaponStats::set_attacking_frames);
    ClassDB::bind_method(D_METHOD("get_attacking_frames"), &WeaponStats::get_attacking_frames);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "attacking_frames"), "set_attacking_frames", "get_attacking_frames");

    ClassDB::bind_method(D_METHOD("set_idle_row", "val"), &WeaponStats::set_idle_row);
    ClassDB::bind_method(D_METHOD("get_idle_row"), &WeaponStats::get_idle_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "idle_row"), "set_idle_row", "get_idle_row");

    ClassDB::bind_method(D_METHOD("set_attacking_row", "val"), &WeaponStats::set_attacking_row);
    ClassDB::bind_method(D_METHOD("get_attacking_row"), &WeaponStats::get_attacking_row);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "attacking_row"), "set_attacking_row", "get_attacking_row");

    ClassDB::bind_method(D_METHOD("set_anim_fps", "val"), &WeaponStats::set_anim_fps);
    ClassDB::bind_method(D_METHOD("get_anim_fps"), &WeaponStats::get_anim_fps);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "anim_fps"), "set_anim_fps", "get_anim_fps");
}