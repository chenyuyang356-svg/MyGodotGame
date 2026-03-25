#include "group_manager.h"
#include <algorithm>

using namespace godot;

GroupManager::GroupManager() {
    temp_groups.reserve(256);
    for (int i = 0; i < MAX_CONTROL_GROUPS; ++i) {
        control_groups[i].reserve(512);
    }
}

int GroupManager::create_temporary_group(Vector2 p_target_pos) {
    int gid = next_temp_id++;
    UnitGroup& group = temp_groups[gid];
    group.id = gid;
    group.target_pos = p_target_pos;
    group.moving_units_count = 0;
    group.unit_ids.clear();
    return gid;
}

void GroupManager::add_unit_to_temp_group(int p_gid, int p_unit_id) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) {
        it->second.unit_ids.push_back(p_unit_id);
        it->second.moving_units_count++;
        it->second.units_count++;
    }
}

void GroupManager::remove_unit_from_temp_group(int p_gid, const UnitData& p_unit) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) {
        UnitGroup& group = it->second;
        group.remove_unit_id(p_unit.id);

        // 使用 UnitData 内部的属性判断
        if (p_unit.state == IDLE) {
            float r_sq = p_unit.stats->get_collision_radius() * p_unit.stats->get_collision_radius();
            if (p_unit.height > 20.0f) { // AIR_HEIGHT_THRESHOLD
                group.air_idle_radius_sq_sum = std::max(0.0f, group.air_idle_radius_sq_sum - r_sq);
            }
            else {
                group.ground_idle_radius_sq_sum = std::max(0.0f, group.ground_idle_radius_sq_sum - r_sq);
            }
        }
        else {
            if (group.moving_units_count > 0) group.moving_units_count--;
        }

        if (group.units_count > 0) group.units_count--;
    }
}

void GroupManager::decrement_moving_count(int p_gid, const UnitData& p_unit) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) {
        UnitGroup& group = it->second;
        if (group.moving_units_count > 0) {
            group.moving_units_count--;

            float r_sq = p_unit.stats->get_collision_radius() * p_unit.stats->get_collision_radius();
            if (p_unit.height > 20.0f) {
                group.air_idle_radius_sq_sum += r_sq;
            }
            else {
                group.ground_idle_radius_sq_sum += r_sq;
            }
        }
    }
}

const std::vector<int>& GroupManager::get_control_group_units(int p_index) {
    static std::vector<int> empty_vec;
    if (p_index < 0 || p_index >= MAX_CONTROL_GROUPS) return empty_vec;
    return control_groups[p_index];
}

void GroupManager::handle_unit_death(const UnitData& p_unit) {
    // 快速清理临时组
    if (p_unit.temp_group_id != -1) {
        remove_unit_from_temp_group(p_unit.temp_group_id, p_unit);
    }

    // 清理编队
    for (int i = 0; i < p_unit.control_group_count; ++i) {
        int g_idx = p_unit.control_group_indices[i];
        if (g_idx >= 0 && g_idx < MAX_CONTROL_GROUPS) {
            auto& ids = control_groups[g_idx];
            for (size_t j = 0; j < ids.size(); ++j) {
                if (ids[j] == p_unit.id) {
                    ids[j] = ids.back();
                    ids.pop_back();
                    break;
                }
            }
        }
    }
}

UnitGroup* GroupManager::get_temp_group(int p_gid) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) return &it->second;
    return nullptr;
}

void GroupManager::_bind_methods() {
    // 绑定 set_control_group 等给脚本调用
}