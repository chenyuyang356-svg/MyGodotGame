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

    for (auto& pair : temp_groups) {
        UnitGroup& group = pair.second;
        Vector2i target_grid = ffm->world_to_grid(group.target_pos);

        auto calc_for_type = [&](int nav_type, float radius_sq_sum) -> float {
            if (radius_sq_sum <= 0) return 0.5f; // 默认值

            const std::vector<float>* field_ptr = ffm->get_integration_field_ptr(target_grid, nav_type);
            if (!field_ptr) return 0.5f;

            float total_unit_area = radius_sq_sum * Math_PI;
            float accumulated_area = 0.0f;
            float max_int = 0.0f;

            // BFS 扩散计算覆盖面积
            std::queue<Vector2i> q;
            std::unordered_set<uint64_t> visited;
            auto get_key = [](Vector2i p) { return ((uint64_t)p.x << 32) | (uint32_t)p.y; };

            q.push(target_grid);
            visited.insert(get_key(target_grid));

            int grid_w = ffm->get_width();
            int grid_h = ffm->get_height();
            Vector2i origin = ffm->get_grid_origin();

            while (!q.empty() && accumulated_area < total_unit_area) {
                Vector2i curr = q.front();
                q.pop();

                Vector2i rel = curr - origin;
                if (rel.x < 0 || rel.x >= grid_w || rel.y < 0 || rel.y >= grid_h) continue;

                int idx = rel.y * grid_w + rel.x;
                float val = (*field_ptr)[idx];

                // 忽略不可达点
                if (val >= 65535.0f) continue;

                accumulated_area += cell_area;
                max_int = std::max(max_int, val);

                // 检查 4 邻域
                Vector2i dirs[] = { Vector2i(0,1), Vector2i(0,-1), Vector2i(1,0), Vector2i(-1,0) };
                for (auto& d : dirs) {
                    Vector2i next = curr + d;
                    if (ffm->is_in_grid(next) && visited.find(get_key(next)) == visited.end()) {
                        visited.insert(get_key(next));
                        q.push(next);
                    }
                }
            }
            // 返回最大集成值作为判定线，稍微加宽 10% 容错
            return max_int * 1.1f;
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