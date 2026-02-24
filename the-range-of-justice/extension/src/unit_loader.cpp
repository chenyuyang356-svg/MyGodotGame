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
        if (p_value == "Sea") return MOVE_SEA;
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

int UnitLoader::_parse_bitfield(String p_value) {
    int result = 0;
    // 按逗号或竖线拆分
    PackedStringArray tags = p_value.split(",");
    if (tags.size() <= 1 && p_value.contains("|")) {
        tags = p_value.split("|");
    }

    for (int i = 0; i < tags.size(); i++) {
        String tag = tags[i].strip_edges();
        if (tag == "Biological") result |= TAG_BIOLOGICAL;
        else if (tag == "Mechanical") result |= TAG_MECHANICAL;
        else if (tag == "Summoned") result |= TAG_SUMMONED;
        else if (tag == "Hero") result |= TAG_HERO;
        else if (tag.is_valid_int()) result |= tag.to_int();
    }
    return result;
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
        UtilityFunctions::print("[UnitLoader] Error: File not found: ", p_path);
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

        // 1. 处理特定的位掩码和枚举 (保持不变)
        if (key == "unit_tags") {
            stats->set(key, _parse_bitfield(value_str));
        }
        else if (key == "armor_type" || key == "attack_type" ||
            key == "move_type" || key == "target_priority") {
            stats->set(key, _parse_enum(key, value_str));
        }
        // 2. [新增/修改]：显式处理字符串类型的 key
        else if (key == "texture_path") {
            stats->set(key, value_str);
        }
        // 3. [修改]：更严谨地处理数值转换
        else {
            // 只有当字符串确实是数字时才转换，否则按 String 处理
            if (value_str.is_valid_float()) {
                if (value_str.contains(".")) {
                    stats->set(key, value_str.to_float());
                }
                else {
                    stats->set(key, value_str.to_int());
                }
            }
            else {
                // 如果不是数字（比如路径、名称等），直接存为原始字符串
                stats->set(key, value_str);
            }
        }
    }

    UtilityFunctions::print("[UnitLoader] Successfully loaded: ", p_path);
    return stats;
}

void UnitLoader::_bind_methods() {
    // [修改] 绑定时记得把第二个参数也暴露出来
    ClassDB::bind_static_method("UnitLoader", D_METHOD("load_stats_from_txt", "path", "target_resource"), &UnitLoader::load_stats_from_txt, DEFVAL(Variant()));
}