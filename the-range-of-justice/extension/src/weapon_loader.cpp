#include "weapon_loader.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static void _warn_unknown_key(const String& p_path, const String& p_section, const String& p_key) {
    UtilityFunctions::printerr("[WeaponLoader] 未知字段 '", p_key, "'（section '", p_section, "'）in ", p_path, "，已忽略");
}

Ref<WeaponStats> WeaponLoader::load_stats_from_cfg(String p_path, Ref<WeaponStats> p_target) {
    Ref<WeaponStats> stats = p_target;
    if (stats.is_null()) stats.instantiate();

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("[WeaponLoader] Error: File not found: ", p_path);
        return stats;
    }

    Ref<ConfigFile> cf;
    cf.instantiate();
    Error err = cf->load(p_path);
    if (err != OK) {
        UtilityFunctions::printerr("[WeaponLoader] Error: 无法解析配置文件: ", p_path);
        return stats;
    }

    PackedStringArray sections = cf->get_sections();
    for (int s = 0; s < sections.size(); ++s) {
        String section = sections[s];
        PackedStringArray keys = cf->get_section_keys(section);

        for (int k = 0; k < keys.size(); ++k) {
            String key = keys[k];
            String value = cf->get_value(section, key);

            // --- 字符串 ---
            if (key == "weapon_name") stats->set_weapon_name(value);
            else if (key == "projectile_type_name") stats->set_projectile_type_name(value);
            else if (key == "texture_path") stats->set_texture_path(value);

            // --- 布尔 ---
            else if (key == "is_turret") stats->set_is_turret(value.to_lower().contains("true") || value.to_lower().contains("1"));
            else if (key == "can_attack_ground") stats->set_can_attack_ground(value.to_lower().contains("true") || value == "1");
            else if (key == "can_attack_air") stats->set_can_attack_air(value.to_lower().contains("true") || value == "1");

            // --- 战斗属性 ---
            else if (key == "damage") stats->set_damage(value.to_float());
            else if (key == "attack_range") stats->set_attack_range(value.to_float());
            else if (key == "attack_interval") stats->set_attack_interval(value.to_float());
            else if (key == "projectile_speed") stats->set_projectile_speed(value.to_float());
            else if (key == "splash_radius") stats->set_splash_radius(value.to_float());
            else if (key == "turn_speed") stats->set_turn_speed(value.to_float());
            else if (key == "firing_tolerance") stats->set_firing_tolerance(value.to_float());

            // --- 向量 ---
            else if (key == "rotation_center") {
                PackedStringArray parts = value.split(",");
                if (parts.size() >= 2) stats->set_rotation_center(Vector2(parts[0].to_float(), parts[1].to_float()));
            }
            else if (key == "muzzle_offset") {
                PackedStringArray parts = value.split(",");
                if (parts.size() >= 3) stats->set_muzzle_offset(Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float()));
            }

            // --- 渲染与动画 ---
            else if (key == "h_frames") stats->set_h_frames(value.to_int());
            else if (key == "v_frames") stats->set_v_frames(value.to_int());
            else if (key == "idle_frames") stats->set_idle_frames(value.to_int());
            else if (key == "attacking_frames") stats->set_attacking_frames(value.to_int());
            else if (key == "idle_row") stats->set_idle_row(value.to_int());
            else if (key == "attacking_row") stats->set_attacking_row(value.to_int());
            else if (key == "anim_fps") stats->set_anim_fps(value.to_int());

            // --- 未知字段告警 ---
            else {
                _warn_unknown_key(p_path, section, key);
            }
        }
    }

    return stats;
}

void WeaponLoader::_bind_methods() {}
