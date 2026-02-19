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
    }
}

void GroupManager::remove_unit_from_temp_group(int p_gid, int p_unit_id) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) {
        it->second.remove_unit_id(p_unit_id);
        // 如果该单位是在移动中被移除（去新组），减少计数
        if (it->second.moving_units_count > 0) {
            it->second.moving_units_count--;
        }
    }
}

void GroupManager::decrement_moving_count(int p_gid) {
    auto it = temp_groups.find(p_gid);
    if (it != temp_groups.end()) {
        if (it->second.moving_units_count > 0) {
            it->second.moving_units_count--;
        }
    }
}

const std::vector<int>& GroupManager::get_control_group_units(int p_index) {
    static std::vector<int> empty_vec;
    if (p_index < 0 || p_index >= MAX_CONTROL_GROUPS) return empty_vec;
    return control_groups[p_index];
}

void GroupManager::handle_unit_death(int p_unit_id, int p_temp_gid, const int* p_control_indices, int p_control_count) {
    // 快速清理临时组
    if (p_temp_gid != -1) {
        remove_unit_from_temp_group(p_temp_gid, p_unit_id);
    }

    // 快速清理编队
    for (int i = 0; i < p_control_count; ++i) {
        int g_idx = p_control_indices[i];
        if (g_idx >= 0 && g_idx < MAX_CONTROL_GROUPS) {
            auto& ids = control_groups[g_idx];
            for (size_t j = 0; j < ids.size(); ++j) {
                if (ids[j] == p_unit_id) {
                    ids[j] = ids.back();
                    ids.pop_back();
                    break;
                }
            }
        }
    }
}

void GroupManager::update_logic(double p_delta) {
    cleanup_timer += p_delta;
    if (cleanup_timer < CLEANUP_INTERVAL) return;
    cleanup_timer = 0.0;

    auto it = temp_groups.begin();
    while (it != temp_groups.end()) {
        // 如果没有单位在移动了，且 ID 列表也空了（或单位都到达了）
        // 这里可以根据需求决定：是计数器归零就删，还是单位完全清空才删
        if (it->second.moving_units_count <= 0) {
            it = temp_groups.erase(it);
        }
        else {
            ++it;
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