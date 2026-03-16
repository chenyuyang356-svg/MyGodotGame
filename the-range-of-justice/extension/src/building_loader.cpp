#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "building_loader.h"
#include "weapon_manager.h"

using namespace godot;

// 辅助：解析 "3,3" 格式的字符串到 Vector2i
Vector2i parse_v2i(String p_str) {
    PackedStringArray parts = p_str.split(",");
    if (parts.size() >= 2) {
        return Vector2i(parts[0].to_int(), parts[1].to_int());
    }
    return Vector2i(1, 1);
}

Ref<BuildingStats> BuildingLoader::load_from_txt(String p_path, WeaponManager* p_weapon_manager, Ref<BuildingStats> p_target) {
    Ref<BuildingStats> stats = p_target;
    if (stats.is_null()) stats.instantiate();

    if (!FileAccess::file_exists(p_path)) return stats;
    Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);

    while (file->get_position() < file->get_length()) {
        String line = file->get_line().strip_edges();
        if (line.is_empty() || line.begins_with("#")) continue;

        int split_idx = line.find("=");
        if (split_idx == -1) continue;

        String key = line.substr(0, split_idx).strip_edges();
        String val = line.substr(split_idx + 1).strip_edges();

        if (key == "building_type") {
            String v = val.to_lower();
            if (v == "barracks") stats->set_building_type(BUILDING_BARRACKS);
            else if (v == "turret") stats->set_building_type(BUILDING_TURRET);
            else if (v == "collector") stats->set_building_type(BUILDING_COLLECTOR);
            else if (v == "storage") stats->set_building_type(BUILDING_STORAGE);
            else stats->set_building_type(BUILDING_GENERIC);
        }

        else if (key == "placement") {
            uint32_t mask = 0;
            PackedStringArray requirements = val.to_lower().split(",");
            for (int i = 0; i < requirements.size(); ++i) {
                String req = requirements[i].strip_edges();
                if (req == "land") mask |= PLACE_LAND;
                else if (req == "water") mask |= PLACE_WATER;
                else if (req == "on_resource") mask |= PLACE_ON_RESOURCE;
            }
            // 如果是纯数字掩码也能解析
            if (mask == 0 && val.is_valid_int()) mask = val.to_int();

            stats->set_placement_requirement(mask);
        }

        else if (key == "footprint") stats->set_footprint(parse_v2i(val));
        else if (key == "clearance") stats->set_clearance_size(parse_v2i(val));
        else if (key == "producible_units") {
            PackedStringArray unit_list = val.split(",");
            for (int i = 0; i < unit_list.size(); ++i) {
                unit_list[i] = unit_list[i].strip_edges();
            }
            stats->set_producible_units(unit_list);
        }
        else if (key == "weapon_mount") {
            // 解析格式修改为: "武器名称, 本地偏移X, 本地偏移Y"
            // 例如: "HeavyCannon, 10.0, 0.0"
            PackedStringArray parts = val.split(",");
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
                    UtilityFunctions::print("[UnitLoader] 成功为建筑挂载武器: ", w_name, " 偏移: ", mount.local_position);
                }
                else {
                    UtilityFunctions::print("[UnitLoader] Error: 未找到武器资源 '", w_name, "' (请确保该武器名称存在并在建筑前完成加载)");
                }
            }
        }
        else if (key == "texture_path" || key == "building_name" || key == "resource_type") {
            stats->set(key, val);
        }
        else {
            // 处理数值
            if (val.is_valid_float()) {
                stats->set(key, val.to_float());
            }
        }
    }
    return stats;
}

void BuildingLoader::_bind_methods() {
    ClassDB::bind_static_method("BuildingLoader", D_METHOD("load_from_txt", "path", "target"), &BuildingLoader::load_from_txt, DEFVAL(Variant()));
}