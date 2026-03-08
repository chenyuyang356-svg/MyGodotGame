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

Ref<ProjectileStats> ProjectileLoader::load_stats_from_txt(String p_path, Ref<ProjectileStats> p_target) {
    Ref<ProjectileStats> stats = p_target;
    if (stats.is_null()) {
        stats.instantiate();
    }

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("[ProjectileLoader] Error: File not found: ", p_path);
        return stats;
    }

    Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
    if (file.is_null()) return stats;

    while (file->get_position() < file->get_length()) {
        String line = file->get_line().strip_edges();
        if (line.is_empty() || line.begins_with("#")) continue;

        int split_index = line.find("=");
        if (split_index == -1) continue;

        String key = line.substr(0, split_index).strip_edges();
        String value_str = line.substr(split_index + 1).strip_edges();

        // 1. 处理投射物类型枚举
        if (key == "projectile_type") {
            stats->set(key, _parse_enum(key, value_str));
        }
        // 2. 处理字符串（如模型/特效路径）
        else if (key == "visual_path") {
            stats->set(key, value_str);
        }
        // 3. 处理浮点数和整数（如 speed, splash_radius, turn_speed 等）
        else {
            if (value_str.is_valid_float()) {
                if (value_str.contains(".")) {
                    stats->set(key, value_str.to_float());
                }
                else {
                    stats->set(key, value_str.to_int());
                }
            }
            else {
                stats->set(key, value_str);
            }
        }
    }

    UtilityFunctions::print("[ProjectileLoader] Successfully loaded projectile: ", p_path);
    return stats;
}

void ProjectileLoader::_bind_methods() {
    ClassDB::bind_static_method("ProjectileLoader", D_METHOD("load_stats_from_txt", "path", "target_resource"), &ProjectileLoader::load_stats_from_txt, DEFVAL(Variant()));
}