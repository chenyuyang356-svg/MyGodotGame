// src/projectile_loader.cpp
#include "projectile_loader.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

int ProjectileLoader::_parse_enum(String p_key, String p_value) {
    p_value = p_value.strip_edges();

    if (p_key == "projectile_type") {
        if (p_value == "Bullet") return PROJECTILE_BULLET;
        if (p_value == "Shell") return PROJECTILE_SHELL;
        if (p_value == "Missile") return PROJECTILE_MISSILE;
        if (p_value.is_valid_int()) return p_value.to_int();
    }

    if (p_value.is_valid_int()) return p_value.to_int();
    return 0;
}

static void _warn_unknown_key(const String& p_path, const String& p_section, const String& p_key) {
    UtilityFunctions::printerr("[ProjectileLoader] 未知字段 '", p_key, "'（section '", p_section, "'）in ", p_path, "，已忽略");
}

Ref<ProjectileStats> ProjectileLoader::load_stats_from_cfg(String p_path, Ref<ProjectileStats> p_target) {
    Ref<ProjectileStats> stats = p_target;
    if (stats.is_null()) stats.instantiate();

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("[ProjectileLoader] Error: File not found: ", p_path);
        return stats;
    }

    // 按行解析：容忍无引号字符串值；";"/"#" 为注释、"[..." 为小节名，均忽略
    Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
    if (file.is_null()) return stats;

    while (file->get_position() < file->get_length()) {
        String line = file->get_line().strip_edges();
        if (line.is_empty() || line.begins_with(";") || line.begins_with("#") || line.begins_with("[")) continue;
        int split_index = line.find("=");
        if (split_index == -1) continue;
        String key = line.substr(0, split_index).strip_edges();
        String value = line.substr(split_index + 1).strip_edges();

            if (key == "projectile_type") stats->set_projectile_type(_parse_enum(key, value));
            else if (key == "projectile_name") stats->set_projectile_name(value);
            else if (key == "visual_path") stats->set_visual_path(value);
            else if (key == "speed") stats->set_speed(value.to_float());
            else if (key == "damage") stats->set_damage(value.to_float());
            else if (key == "is_healing") stats->set_is_healing(value.to_lower().contains("true") || value == "1");
            else if (key == "splash_radius") stats->set_splash_radius(value.to_float());
            else if (key == "arc_height") stats->set_arc_height(value.to_float());
            else if (key == "turn_speed") stats->set_turn_speed(value.to_float());
            else if (key == "acceleration") stats->set_acceleration(value.to_float());
            else if (key == "h_frames") stats->set_h_frames(value.to_int());
            else if (key == "v_frames") stats->set_v_frames(value.to_int());
            else if (key == "anim_fps") stats->set_anim_fps(value.to_float());
            else {
                _warn_unknown_key(p_path, "", key);
            }
        }

    return stats;
}

void ProjectileLoader::_bind_methods() {
    ClassDB::bind_static_method("ProjectileLoader", D_METHOD("load_stats_from_cfg", "path", "target_resource"), &ProjectileLoader::load_stats_from_cfg, DEFVAL(Variant()));
}
