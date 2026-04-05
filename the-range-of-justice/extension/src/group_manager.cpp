#include "group_manager.h"
#include <algorithm>

using namespace godot;

GroupManager::GroupManager() {
    temp_groups.reserve(256);
    for (int i = 0; i < MAX_CONTROL_GROUPS; ++i) {
        control_groups[i].reserve(512);
    }
}

void GroupManager::update_target_integrations(FlowFieldManager* ffm) {
    Vector2i cell_sz = ffm->get_cell_size();
    float cell_area = (float)cell_sz.x * cell_sz.y;
    int grid_w = ffm->get_width();
    int grid_h = ffm->get_height();
    Vector2i origin = ffm->get_grid_origin();

    // 临时缓冲区，用于存储和排序集成值，避免循环内重新分配
    static std::vector<float> value_cache;
    value_cache.reserve(1024);

    for (auto& pair : temp_groups) {
        UnitGroup& group = pair.second;
        Vector2i target_grid = ffm->world_to_grid(group.target_pos);

        auto calc_for_type = [&](int nav_type, float radius_sq_sum) -> float {
            if (radius_sq_sum <= 0) return 0.0f;

            const std::vector<float>* field_ptr = ffm->get_integration_field_ptr(target_grid, nav_type);
            if (!field_ptr) return 0.0f;

            // 1. 计算所需面积和大致覆盖半径
            float total_unit_area = radius_sq_sum * Math_PI;
            int needed_cells = std::ceil(total_unit_area / cell_area);

            // 估计半径 (在网格空间)，并给 1.5 倍余量处理地形障碍
            float est_r_world = std::sqrt(radius_sq_sum);
            int r_grid = std::ceil(est_r_world / cell_sz.x * 3.0f) + 1;

            // 2. 在 2R * 2R 范围内收集所有可达格子的集成值
            value_cache.clear();
            Vector2i min_bound = target_grid - Vector2i(r_grid, r_grid);
            Vector2i max_bound = target_grid + Vector2i(r_grid, r_grid);

            for (int y = min_bound.y; y <= max_bound.y; ++y) {
                for (int x = min_bound.x; x <= max_bound.x; ++x) {
                    Vector2i curr(x, y);
                    if (!ffm->is_in_grid(curr)) continue;

                    Vector2i rel = curr - origin;
                    int idx = rel.y * grid_w + rel.x;
                    float val = (*field_ptr)[idx];

                    // 只有可达的点才计入
                    if (val < 65535.0f) {
                        value_cache.push_back(val);
                    }
                }
            }

            if (value_cache.empty()) return 0.0f;

            // 3. 排序集成值（从小到大，即从近到远）
            std::sort(value_cache.begin(), value_cache.end());

            // 4. 取出能覆盖所有单位面积的那个“最远”格子的集成值
            // 如果收集到的格子不够多，取最后一个（说明单位群可能挤不下）
            int result_idx = std::min((int)value_cache.size() - 1, needed_cells);

            // 增加 10% 的容错缓冲，避免边缘单位频繁切换状态
            return value_cache[result_idx] * 1.1f;
            };

        group.ground_target_integration = calc_for_type(NAV_LAND, group.ground_idle_radius_sq_sum);
        group.air_target_integration = calc_for_type(NAV_AIR, group.air_idle_radius_sq_sum);
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