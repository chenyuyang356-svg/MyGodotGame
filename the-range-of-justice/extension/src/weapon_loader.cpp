#include "weapon_loader.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

Ref<WeaponStats> WeaponLoader::load_stats_from_txt(String p_path, Ref<WeaponStats> p_target) {
    Ref<WeaponStats> stats = p_target;
    if (stats.is_null()) {
        stats.instantiate();
    }

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("[WeaponLoader] Error: File not found: ", p_path);
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

        // 字符串类型
        if (key == "weapon_name" || key == "projectile_type_name" || key == "texture_path") {
            stats->set(key, value_str);
        }
        // 布尔类型
        else if (key == "is_turret") {
            String val_str = value_str.to_lower();
            stats->set(key, val_str.contains("true") || val_str.contains("1"));
        }
        // 数值类型自动转换
        else {
            if (value_str.is_valid_float()) {
                if (value_str.contains(".")) {
                    stats->set(key, value_str.to_float());
                }
                else {
                    stats->set(key, value_str.to_int());
                }
            }
        }
    }

    UtilityFunctions::print("[WeaponLoader] Successfully loaded weapon: ", p_path);
    return stats;
}

void WeaponLoader::_bind_methods() {}