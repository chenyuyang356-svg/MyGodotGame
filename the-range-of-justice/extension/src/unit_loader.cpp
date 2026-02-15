// src/unit_loader.cpp
#include "unit_loader.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// 1. 实现字符串解析函数
int UnitLoader::_parse_enum(String p_key, String p_value) {
    p_value = p_value.strip_edges(); // 去除空格

    // --- ArmorType ---
    if (p_key == "armor_type") {
        if (p_value == "Light") return ARMOR_LIGHT;
        if (p_value == "Heavy") return ARMOR_HEAVY;
        if (p_value == "Building") return ARMOR_BUILDING;
        if (p_value == "Hero") return ARMOR_HERO;
        // 如果填了数字，尝试直接转换
        if (p_value.is_valid_int()) return p_value.to_int();
    }

    // --- AttackType ---
    else if (p_key == "attack_type") {
        if (p_value == "Physical") return ATTACK_PHYSICAL;
        if (p_value == "Magic") return ATTACK_MAGIC;
        if (p_value == "Siege") return ATTACK_SIEGE;
        if (p_value.is_valid_int()) return p_value.to_int();
    }

    // --- MoveType ---
    else if (p_key == "move_type") {
        if (p_value == "Ground") return MOVE_GROUND;
        if (p_value == "Air") return MOVE_AIR;
        if (p_value == "Hover") return MOVE_HOVER;
        if (p_value.is_valid_int()) return p_value.to_int();
    }

    // --- TargetPriority --- 
    else if (p_key == "target_priority") {
        if (p_value == "Closest") return PRIORITY_CLOSEST;
        if (p_value == "Lowest_hp") return PRIORITY_LOWEST_HP;
        if (p_value == "Highest_value") return PRIORITY_HIGHEST_VALUE;
        if (p_value.is_valid_int()) return p_value.to_int();
    }
    // 默认情况：返回一个标记值（如 -999）或者尝试直接转 int
    if (p_value.is_valid_int()) return p_value.to_int();
    return 0; // 默认 fallback
}

Ref<UnitStats> UnitLoader::load_stats_from_txt(String p_path, Ref<UnitStats> p_target) {
    // [关键点]：热重载的核心
    // 如果传入了 p_target，我们直接操作它（内存地址不变，引用它的单位会自动更新）
    // 如果没传，我们才 new 一个新的。
    Ref<UnitStats> stats = p_target;
    if (stats.is_null()) {
        stats.instantiate();
    }

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("Error: Config not found: ", p_path);
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

        // [修改] 智能设值
        // 如果是已知的枚举 key，走 _parse_enum
        if (key == "armor_type" || key == "attack_type") {
            int enum_val = _parse_enum(key, value_str);
            stats->set(key, enum_val);
        }
        else {
            // 普通属性 (float/int)，直接转换
            float val_float = value_str.to_float();
            stats->set(key, val_float);
        }
    }

    UtilityFunctions::print("Loaded/Reloaded stats: ", p_path);
    return stats;
}

void UnitLoader::_bind_methods() {
    // [修改] 绑定时记得把第二个参数也暴露出来
    ClassDB::bind_static_method("UnitLoader", D_METHOD("load_stats_from_txt", "path", "target_resource"), &UnitLoader::load_stats_from_txt, DEFVAL(Variant()));
}