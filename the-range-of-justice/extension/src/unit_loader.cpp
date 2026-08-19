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

    // 默认情况：尝试直接转 int
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
        else if (tag == "Builder") result |= TAG_BUILDER;
        else if (tag == "Summoned") result |= TAG_SUMMONED;
        else if (tag == "Hero") result |= TAG_HERO;
        else if (tag == "Dummy") result |= TAG_DUMMY;
        else if (tag.is_valid_int()) result |= tag.to_int();
    }
    return result;
}

// 未知字段告警：杜绝"配置写了等于没写"的静默失败
static void _warn_unknown_key(const String& p_path, const String& p_section, const String& p_key) {
    UtilityFunctions::printerr("[UnitLoader] 未知字段 '", p_key, "'（section '", p_section, "'）in ", p_path, "，已忽略");
}

Ref<UnitStats> UnitLoader::load_stats_from_cfg(String p_path, WeaponManager* p_weapon_manager, Ref<UnitStats> p_target) {
    Ref<UnitStats> stats = p_target;
    if (stats.is_null()) {
        stats.instantiate();
    }

    if (!FileAccess::file_exists(p_path)) {
        UtilityFunctions::print("[UnitLoader] Error: File not found: ", p_path);
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

            // --- 字符串 ---
            if (key == "unit_name") stats->set_unit_name(value);
            else if (key == "unit_name_key") stats->set_unit_name_key(value);
            else if (key == "unit_description_key") stats->set_unit_description_key(value);
            else if (key == "texture_path") stats->set_texture_path(value);

            // --- 生存 ---
            else if (key == "health_max") stats->set_health_max(value.to_float());
            else if (key == "health_regen") stats->set_health_regen(value.to_float());
            else if (key == "shield_max") stats->set_shield_max(value.to_float());
            else if (key == "shield_regen") stats->set_shield_regen(value.to_float());
            else if (key == "armor_type") stats->set_armor_type((ArmorType)_parse_enum(key, value));

            // --- 攻击 ---
            else if (key == "target_priority") stats->set_target_priority((TargetPriority)_parse_enum(key, value));
            else if (key == "can_fire_on_move") stats->set_can_fire_on_move(value.to_lower().contains("true") || value.to_lower().contains("1"));
            else if (key == "deploy_time") stats->set_deploy_time(value.to_float());

            // --- 移动 ---
            else if (key == "mass") stats->set_mass(value.to_float());
            else if (key == "move_speed") stats->set_move_speed(value.to_float());
            else if (key == "acceleration") stats->set_acceleration(value.to_float());
            else if (key == "turn_speed") stats->set_turn_speed(value.to_float());
            else if (key == "turn_acceleration") stats->set_turn_acceleration(value.to_float());
            else if (key == "collision_radius") stats->set_collision_radius(value.to_float());
            else if (key == "base_height") stats->set_base_height(value.to_float());
            else if (key == "move_type") stats->set_move_type((MoveType)_parse_enum(key, value));

            // --- 视野/经济 ---
            else if (key == "sight_range") stats->set_sight_range(value.to_float());
            else if (key == "aggro_range") stats->set_aggro_range(value.to_float());
            else if (key == "cost") stats->set_cost(value.to_int());
            else if (key == "build_time") stats->set_build_time(value.to_float());

            // --- 标签与可生产 ---
            else if (key == "unit_tags") stats->set("unit_tags", _parse_bitfield(value));
            else if (key == "producible_buildings") {
                PackedStringArray list = value.split(",");
                for (int i = 0; i < list.size(); ++i) list[i] = list[i].strip_edges();
                stats->set_producible_buildings(list);
            }

            // --- 渲染与动画 ---
            else if (key == "h_frames") stats->set_h_frames(value.to_int());
            else if (key == "v_frames") stats->set_v_frames(value.to_int());
            else if (key == "move_frames") stats->set_move_frames(value.to_int());
            else if (key == "idle_frames") stats->set_idle_frames(value.to_int());
            else if (key == "move_row") stats->set_move_row(value.to_int());
            else if (key == "idle_row") stats->set_idle_row(value.to_int());
            else if (key == "deploy_frames") stats->set_deploy_frames(value.to_int());
            else if (key == "deploy_row") stats->set_deploy_row(value.to_int());
            else if (key == "anim_fps") stats->set_anim_fps(value.to_int());
            else if (key == "dying_time") stats->set_dying_time(value.to_float());

            // --- 粒子 ---
            else if (key == "emit_threshold") stats->set_emit_threshold_override(value.to_float());
            else if (key == "particle_scale") stats->set_particle_scale_override(value.to_float());
            else if (key == "effect_point") {
                // 格式: "特效名, X, Y, 缩放, 寿命, 是否波纹"
                PackedStringArray parts = value.split(",");
                if (parts.size() >= 3) {
                    EffectPoint ep;
                    ep.effect_type = parts[0].strip_edges();
                    ep.local_position = Vector2(parts[1].to_float(), parts[2].to_float());
                    if (parts.size() >= 4) ep.scale_override = parts[3].to_float();
                    if (parts.size() >= 5) ep.life_override = parts[4].to_float();
                    if (parts.size() >= 6) ep.is_ripple = (parts[5].strip_edges().to_lower() == "true");
                    stats->effect_points.push_back(ep);
                }
            }

            // --- 独立武器挂载 (引用 WeaponManager 已注册武器) ---
            else if (key == "weapon_mount") {
                PackedStringArray parts = value.split(",");
                if (parts.size() >= 1) {
                    String w_name = parts[0].strip_edges();
                    if (p_weapon_manager) {
                        Ref<WeaponStats> w_stats = p_weapon_manager->get_weapon(w_name);
                        if (w_stats.is_valid()) {
                            WeaponMount mount;
                            mount.weapon_resource = w_stats;

                            if (parts.size() >= 3) mount.local_position = Vector2(parts[1].to_float(), parts[2].to_float());
                            else mount.local_position = Vector2(0, 0);

                            stats->weapon_mounts.push_back(mount);
                        }
                        else {
                            UtilityFunctions::printerr("[UnitLoader] Error: 未找到武器资源 '", w_name, "' in ", p_path, "（请确认该武器已在单位之前注册）");
                        }
                    }
                }
            }

            // --- 未知字段告警 ---
            else {
                _warn_unknown_key(p_path, "", key);
            }
        }

    if (stats->unit_name.is_empty()) {
        UtilityFunctions::printerr("[UnitLoader] 单位配置缺少 unit_name: ", p_path);
    }

    return stats;
}

void UnitLoader::_bind_methods() {}
