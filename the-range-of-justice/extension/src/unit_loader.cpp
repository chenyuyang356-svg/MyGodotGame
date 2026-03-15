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

    // 默认情况：返回一个标
    // 记值（如 -999）或者尝试直接转 int
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

Ref<UnitStats> UnitLoader::load_stats_from_txt(String p_path, WeaponManager* p_weapon_manager, Ref<UnitStats> p_target) {
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

        else if (key == "can_fire_on_move") {
            String val_str = value_str.strip_edges().to_lower();
            bool can_fire = val_str.contains("true") || val_str.contains("1");
            UtilityFunctions::print("[UnitLoader] 修复后，读到字符串: '", val_str, "'，最终判定结果为: ", can_fire);
            stats->set_can_fire_on_move(can_fire);
        }

        else if (key == "weapon") {
            // 解析由逗号分隔的武器字符串，例如: "Bullet, 15.0, 200.0, 0.5, 500.0, 0.0"
            PackedStringArray parts = value_str.split(",");
            if (parts.size() >= 4) {
                Weapon w;
                w.projectile_type_name = parts[0].strip_edges();
                w.damage = parts[1].to_float();
                w.attack_range = parts[2].to_float();
                w.attack_interval = parts[3].to_float();

                // 可选参数
                if (parts.size() >= 5) w.projectile_speed = parts[4].to_float();
                if (parts.size() >= 6) w.splash_radius = parts[5].to_float();

                stats->weapons.push_back(w);
                UtilityFunctions::print("[UnitLoader] 成功为单位挂载武器: ", w.projectile_type_name, " 伤害: ", w.damage);
            }
        }
        
        else if (key == "weapon_mount") {
            // 解析格式修改为: "武器名称, 本地偏移X, 本地偏移Y"
            // 例如: "HeavyCannon, 10.0, 0.0"
            PackedStringArray parts = value_str.split(",");
            if (parts.size() >= 1) {
                String w_name = parts[0].strip_edges();
                Ref<WeaponStats> w_stats = p_weapon_manager->get_weapon(w_name);

                if (w_stats.is_valid()) {
                    WeaponMount mount;
                    mount.weapon_resource = w_stats;

                    // 解析局部位移 (可选参数，默认为 0,0)
                    if (parts.size() >= 3) {
                        mount.local_position = Vector2(parts[1].to_float(), parts[2].to_float());
                    }
                    else {
                        mount.local_position = Vector2(0, 0);
                    }

                    stats->weapon_mounts.push_back(mount);
                    UtilityFunctions::print("[UnitLoader] 成功为单位挂载武器: ", w_name, " 偏移: ", mount.local_position);
                }
                else {
                    UtilityFunctions::print("[UnitLoader] Error: 未找到武器资源 '", w_name, "' (请确保该武器名称存在并在单位前完成加载)");
                }
            }
        }

    
        // 2. [新增/修改]：显式处理字符串类型的 key
        else if (key == "texture_path") {
            stats->set(key, value_str);
        }
        else if (key == "unit_name") {
            stats->set(key, value_str);
        }

        // 3. [修改]：更严谨地处理数值转换 (保持不变)
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

    UtilityFunctions::print("[UnitLoader] Successfully loaded: ", p_path);
    return stats;
}

void UnitLoader::_bind_methods() {}