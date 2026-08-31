#include <godot_cpp/variant/utility_functions.hpp>

#include "building_loader.h"
#include "weapon_manager.h"

using namespace godot;

// 辅助：解析 "3,3" 格式的字符串到 Vector2i
Vector2i BuildingLoader::_parse_vector2i(String p_str) {
    PackedStringArray parts = p_str.split(",");
    if (parts.size() >= 2) {
        return Vector2i(parts[0].to_int(), parts[1].to_int());
    }
    return Vector2i(1, 1);
}

// 未知字段告警
static void _warn_unknown_key(const String& p_path, const String& p_section, const String& p_key) {
    UtilityFunctions::printerr("[BuildingLoader] 未知字段 '", p_key, "'（section '", p_section, "'）in ", p_path, "，已忽略");
}

Ref<BuildingStats> BuildingLoader::load_from_cfg(String p_path, WeaponManager* p_weapon_manager, Ref<BuildingStats> p_target) {
    Ref<BuildingStats> stats = p_target;
    if (stats.is_null()) stats.instantiate();

    if (!FileAccess::file_exists(p_path)) return stats;

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

            // --- 标识 ---
            if (key == "building_name_key") stats->set_building_name_key(value);
            else if (key == "building_description_key") stats->set_building_description_key(value);
            else if (key == "building_name") stats->set_building_name(value);
            else if (key == "texture_path") stats->set_texture_path(value);

            // --- 类型与放置 ---
            else if (key == "building_type") {
                String v = value.to_lower();
                if (v == "barracks") stats->set_building_type(BUILDING_BARRACKS);
                else if (v == "turret") stats->set_building_type(BUILDING_TURRET);
                else if (v == "collector") stats->set_building_type(BUILDING_COLLECTOR);
                else if (v == "storage") stats->set_building_type(BUILDING_STORAGE);
                else stats->set_building_type(BUILDING_GENERIC);
            }
            else if (key == "placement") {
                uint32_t mask = 0;
                PackedStringArray requirements = value.to_lower().split(",");
                for (int i = 0; i < requirements.size(); ++i) {
                    String req = requirements[i].strip_edges();
                    if (req == "land") mask |= PLACE_LAND;
                    else if (req == "water") mask |= PLACE_WATER;
                    else if (req == "on_resource") mask |= PLACE_ON_RESOURCE;
                }
                if (mask == 0 && value.is_valid_int()) mask = value.to_int();
                stats->set_placement_requirement(mask);
            }

            // --- 基础属性 ---
            else if (key == "cost") stats->set_cost(value.to_int());
            else if (key == "build_time") stats->set_build_time(value.to_float());
            else if (key == "health_max") stats->set_health_max(value.to_float());
            else if (key == "base_height") stats->set_base_height(value.to_float());
            else if (key == "footprint") stats->set_footprint(_parse_vector2i(value));
            else if (key == "clearance") stats->set_clearance_size(_parse_vector2i(value));
            else if (key == "dying_time") stats->set_dying_time(value.to_float());

            // --- 渲染与动画 ---
            else if (key == "h_frames") stats->set_h_frames(value.to_int());
            else if (key == "v_frames") stats->set_v_frames(value.to_int());
            else if (key == "anim_fps") stats->set_anim_fps(value.to_int());
            else if (key == "idle_frames") stats->set_idle_frames(value.to_int());
            else if (key == "idle_row") stats->set_idle_row(value.to_int());
            else if (key == "working_frames") stats->set_working_frames(value.to_int());
            else if (key == "working_row") stats->set_working_row(value.to_int());
            else if (key == "building_frames") stats->set_building_frames(value.to_int());
            else if (key == "working_hold_time") stats->set_working_hold_time(value.to_float());
            else if (key == "finish_frames") stats->set_finish_frames(value.to_int());
            else if (key == "turret_spin") stats->set_turret_spin(value.to_lower().contains("true") || value == "1");
            else if (key == "turret_spin_speed") stats->set_turret_spin_speed(value.to_float());

            // --- 兵营 ---
            else if (key == "producible_units") {
                PackedStringArray unit_list = value.split(",");
                for (int i = 0; i < unit_list.size(); ++i) unit_list[i] = unit_list[i].strip_edges();
                stats->set_producible_units(unit_list);
            }
            else if (key == "production_speed") stats->set_production_speed(value.to_float());

            // --- 炮塔 ---
            else if (key == "attack_damage") stats->set_attack_damage(value.to_float());
            else if (key == "attack_range") stats->set_attack_range(value.to_float());
            else if (key == "attack_interval") stats->set_attack_interval(value.to_float());
            else if (key == "projectile_speed") stats->set_projectile_speed(value.to_float());
            else if (key == "splash_radius") stats->set_splash_radius(value.to_float());

            // --- 采集器 ---
            else if (key == "resource_type") stats->set_resource_type(value);
            else if (key == "collection_rate") stats->set_collection_rate(value.to_float());
            else if (key == "collection_capacity") stats->set_collection_capacity(value.to_float());

            // --- 独立武器挂载 ---
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
                            UtilityFunctions::printerr("[BuildingLoader] Error: 未找到武器资源 '", w_name, "' in ", p_path, "（请确认该武器已在建筑之前注册）");
                        }
                    }
                }
            }

            // --- 未知字段告警 ---
            else {
                _warn_unknown_key(p_path, "", key);
            }
        }

    if (stats->get_building_name().is_empty()) {
        UtilityFunctions::printerr("[BuildingLoader] 建筑配置缺少 building_name: ", p_path);
    }

    return stats;
}

void BuildingLoader::_bind_methods() {
    ClassDB::bind_static_method("BuildingLoader", D_METHOD("load_from_cfg", "path", "target"), &BuildingLoader::load_from_cfg, DEFVAL(Variant()));
}
