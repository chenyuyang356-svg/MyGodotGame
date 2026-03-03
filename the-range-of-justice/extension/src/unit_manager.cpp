#include "unit_manager.h"
#include "attack_manager.h"
#include "selection_manager.h"

#include <queue>

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

UnitManager::UnitManager() {
    units.reserve(1000);
}

UnitManager::~UnitManager() {}

void UnitManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
    if (!flow_field_manager) {
        flow_field_manager = get_node<FlowFieldManager>("../FlowFieldManager");
        if (!flow_field_manager) return;
    }
    
    flow_field_manager->setup_grid(p_width, p_height, p_origin, p_cell_size);

    unit_grid_width = p_width / 2;
    unit_grid_height = p_height / 2;
    unit_grid_size = unit_grid_width * unit_grid_height;
    unit_grid_cell_size = p_cell_size * 2;

    unit_grid.resize(unit_grid_size);

    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].reserve(10);
    }

    is_setup = true;
}

int UnitManager::spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id, int p_forced_id) {
    if (p_stats.is_null()) return -1;

    UnitData new_unit;
    // 如果传入了有效的 forced_id (客户端模式)，则使用它；否则使用自增 ID (服务器模式)
    if (p_forced_id != -1) {
        new_unit.id = p_forced_id;
        // 更新 next_unit_id 以防冲突（可选，但安全）
        if (p_forced_id >= next_unit_id) next_unit_id = p_forced_id + 1;
    }
    else {
        new_unit.id = next_unit_id++;
    }

    new_unit.position = p_world_pos;
    new_unit.prev_position = p_world_pos;
    new_unit.next_position = p_world_pos;
    new_unit.height = p_stats->base_height;
    new_unit.stats = p_stats; //         

    new_unit.team_id = p_team_id; 

 
    new_unit.current_health = p_stats->get_health_max();
    new_unit.state = IDLE;

    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;
    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id, SelectionManager* p_selection_manager) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

    UnitData& unit_to_remove = units[index_to_remove];
    group_manager->handle_unit_death(p_unit_id, unit_to_remove.temp_group_id, unit_to_remove.control_group_indices, unit_to_remove.control_group_count);

    if (index_to_remove != last_unit_idx) {
        // 1.   ȡ   һ    λ      
        UnitData& last_unit = units.back();

        // 2.      һ    λ ƶ   Ҫɾ    λ  
        units[index_to_remove] = last_unit;

        // 3.    ±  ƶ   λ ڹ ϣ   е     
        id_to_index[last_unit.id] = index_to_remove;
    }

   
    units.pop_back();
    id_to_index.erase(p_unit_id);
    p_selection_manager->on_unit_despawned(p_unit_id);
}

void UnitManager::command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos) {
    if (!flow_field_manager) return;
    if (p_unit_ids.is_empty()) return;

    Vector2i target_grid_pos = flow_field_manager->world_to_grid(p_target_world_pos);

    // 检查目标点是否在网格内
    if (!(flow_field_manager->is_in_grid(target_grid_pos))) return;

    // 记录这一批指令中已经请求过的导航类型，避免重复调用 create_flow_field
    // 假设 NavigationType 枚举的最大值为 NAV_MAX
    bool requested_types[NAV_MAX] = { false };

    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);

        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];

            // --- 核心逻辑修改：根据单位自身的导航类型请求流场 ---
            // 假设 unit.nav_type 存储了该单位的移动类型 (NAV_LAND, NAV_SEA 等)
            int type = unit.get_nav_type();

            if (type >= 0 && type < NAV_MAX) {
                if (!requested_types[type]) {
                    // 为该类型创建/获取流场，p_overwrite 设为 false 表示如果已存在则不重置
                    flow_field_manager->create_flow_field(target_grid_pos, type, false);
                    requested_types[type] = true;
                }
            }

            // --- 更新单位状态 ---
            unit.target_grid = target_grid_pos;
            unit.target_pos = p_target_world_pos;
            unit.state = MOVING;
            unit.target_id = -1;
            unit.is_patrolling = false;
            unit.is_manual_target = false;

            // 注意：如果单位正在移动中切换了目标，
            // 它的 get_flow_direction 调用也需要传入 unit.nav_type
        }
    }
}

void UnitManager::command_units_to_patrol(Array p_unit_ids, Array p_waypoints) {
    if (p_unit_ids.is_empty()) return;
    
    std::vector<Vector2> waypoints;
    for (int i = 0; i < p_waypoints.size(); ++i) {
        waypoints.push_back(p_waypoints[i]);
    }
    if (waypoints.empty()) return;

    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];
            unit.is_patrolling = true;
            unit.patrol_waypoints = waypoints;
            unit.current_waypoint_idx = 0;
            unit.state = PATROLLING;
            unit.target_id = -1; 
            unit.is_patrolling = true;
            unit.is_manual_target = false; 
            unit.target_id = -1;
        }
       
    }
}

void UnitManager::command_units_to_attack_target(Array p_unit_ids, int p_target_id) {
    if (p_unit_ids.is_empty()) return;
    if (p_target_id == -1) return;
    
    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];

            // 1.       е  ƶ   Ѳ  ״̬
            unit.is_patrolling = false;

            // 2.     Ŀ  
            unit.target_id = p_target_id;
            unit.is_manual_target = true;

            // 3. ֱ      ׷  ״̬  
            unit.state = CHASING;
        }
    }
}

void UnitManager::update_spatial_grid() {
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].clear();
    }
    for (int i = 0; i < units.size(); ++i) {
        Vector2i rel_pos = flow_field_manager->world_to_relative(units[i].position);

        int ux = rel_pos.x / 2;
        int uy = rel_pos.y / 2;

        if (ux >= 0 && ux < unit_grid_width && uy >= 0 && uy < unit_grid_height) {
            int grid_idx = uy * unit_grid_width + ux;
            unit_grid[grid_idx].push_back(i);
        }
    }
}

std::vector<int> UnitManager::get_nearby_units(Vector2 p_world_pos, float p_radius) {
    std::vector<int> nearby_indices;
    Vector2i rel_pos = flow_field_manager->world_to_relative(p_world_pos);
    int ux = rel_pos.x / 2;
    int uy = rel_pos.y / 2;
    int dx = int(p_radius / unit_grid_cell_size.x) + 1;
    int dy = int(p_radius / unit_grid_cell_size.y) + 1;

    //     3x3   Χ ڵĸ   
    for (int nx = ux - dx; nx <= ux + dx; ++nx) {
        for (int ny = uy - dy; ny <= uy + dy; ++ny) {
            if (nx >= 0 && nx < unit_grid_width && ny >= 0 && ny < unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    if (p_world_pos.distance_squared_to(units[unit_idx].position) < p_radius * p_radius) {
                        nearby_indices.push_back(unit_idx);
                    }
                }
            }
        }
    }
    return nearby_indices;
}

int UnitManager::get_unit_at_position(Vector2 p_world_pos) {
    int best_id = -1;
    float max_height = -100.0f;
    Vector2i rel_pos = flow_field_manager->world_to_relative(p_world_pos);
    int ux = rel_pos.x / 2;
    int uy = rel_pos.y / 2;

    //加入大型单位后需要单独处理
    for (int nx = ux - 1; nx <= ux + 1; ++nx) {
        for (int ny = uy - 1; ny <= uy + 1; ++ny) {
            if (nx >= 0 && nx < unit_grid_width && ny >= 0 && ny < unit_grid_height) {
                int grid_idx = ny * unit_grid_width + nx;
                const auto& cell = unit_grid[grid_idx];
                for (int unit_idx : cell) {
                    UnitData& unit = units[unit_idx];
                    if (p_world_pos.distance_squared_to(unit.position) < unit.get_squared_radius()) {
                        if (unit.height > max_height) {
                            max_height = unit.height;
                            best_id = unit.id;
                        }
                    }
                }
            }
        }
    }

    return best_id;
}

std::vector<int> UnitManager::get_units_of_type_in_area(Ref<UnitStats> p_stats, Rect2 p_rect, int p_team_id) {
    std::vector<int> result;
    Vector2i unit_grid_rect_pos = flow_field_manager->world_to_relative(p_rect.position) / 2;
    Vector2i unit_grid_rect_end = flow_field_manager->world_to_relative(p_rect.get_end()) / 2 + Vector2i(1, 1);
    
    for (int nx = unit_grid_rect_pos.x; nx <= unit_grid_rect_end.x; ++nx) {
        for (int ny = unit_grid_rect_pos.y; ny <= unit_grid_rect_end.y; ++ny) {
            int grid_idx = ny * unit_grid_width + nx;
            const auto& cell = unit_grid[grid_idx];
            for (int unit_idx : cell) {
                UnitData& unit = units[unit_idx];
                if (p_rect.has_point(unit.position)) {
                    if (unit.stats == p_stats && unit.team_id == p_team_id) {
                        result.push_back(unit.id);
                    }
                }
            }
        }
    }

    return result;
}

std::vector<int> UnitManager::get_units_in_box(Rect2 p_box, int p_team_id) {
    std::vector<int> result;
    Vector2i unit_grid_rect_pos = flow_field_manager->world_to_relative(p_box.position) / 2;
    Vector2i unit_grid_rect_end = flow_field_manager->world_to_relative(p_box.get_end()) / 2 + Vector2i(1, 1);

    for (int nx = unit_grid_rect_pos.x; nx <= unit_grid_rect_end.x; ++nx) {
        for (int ny = unit_grid_rect_pos.y; ny <= unit_grid_rect_end.y; ++ny) {
            int grid_idx = ny * unit_grid_width + nx;
            const auto& cell = unit_grid[grid_idx];
            for (int unit_idx : cell) {
                UnitData& unit = units[unit_idx];
                if (p_box.has_point(unit.position)) {
                    if (unit.team_id == p_team_id) {
                        result.push_back(unit.id);
                    }
                }
            }
        }
    }

    return result;
}

void UnitManager::update(double p_delta) {

    if (!is_setup || !flow_field_manager) { return; }

    if (attack_manager) {
        attack_manager->update_units(p_delta);
    }

    update_spatial_grid();
    flow_field_manager->update(p_delta);

    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];

        unit.prev_position = unit.position;
        unit.prev_height = unit.height;
        unit.prev_rotation = unit.rotation;

        update_state(unit);
        update_velocity(unit, p_delta);
        move(unit, p_delta);

        unit.next_position = unit.position;
        unit.next_height = unit.height;
        unit.next_rotation = unit.rotation;
    }
}

Vector2 UnitManager::get_flow(UnitData& p_unit) {
    Vector2 flow;
    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        flow = (p_unit.target_pos - p_unit.position).normalized();
        return flow;
    }
    flow = flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos, p_unit.get_nav_type());
    return flow;
}

Vector2 UnitManager::get_separation(UnitData& p_unit) {
    bool is_IDLE = (p_unit.state == IDLE);
    Vector2 separation = Vector2(0, 0);

    for (int unit_idx : get_nearby_units(p_unit.position, ((p_unit.stats)->get_collision_radius()) * separation_radius_factor)) {
        const UnitData& nearby_unit = units[unit_idx];

        if ((p_unit.stats->move_type == MOVE_AIR) && (nearby_unit.stats->move_type != MOVE_AIR) ||
            (p_unit.stats->move_type != MOVE_AIR) && (nearby_unit.stats->move_type == MOVE_AIR)) {
            continue;
        }

        Vector2 radius_vector = nearby_unit.position - p_unit.position;
        float length_squared = radius_vector.length_squared();
        if (length_squared < 10e-12) {
            continue;
        }
        if (is_IDLE) {
            if (nearby_unit.state == IDLE) {
                separation -= radius_vector / length_squared;
            }
            else {
                separation -= 2 * radius_vector / length_squared;
            }
        }
        else {
            if (nearby_unit.state == IDLE) {
                separation -= 0.5 * radius_vector / length_squared;
            }
            else {
                separation -= radius_vector / length_squared;
            }
        }
    }

    separation = separation.limit_length(separation_limit);
    return separation;

}

Vector2 UnitManager::get_friction(UnitData& p_unit) {
    return (-p_unit.velocity);
}

Vector2 UnitManager::get_force(UnitData& p_unit) {
    Vector2 force = Vector2(0, 0);
    switch (p_unit.state) {
    case IDLE:
        force = get_friction(p_unit) * friction_factor + get_separation(p_unit) * separation_factor;
        break;
    case MOVING:
        force = get_flow(p_unit) * flow_factor + get_separation(p_unit) * separation_factor;
        break;
    }
    return force;
}

void UnitManager::update_state(UnitData& p_unit) {
    switch (p_unit.state) {
    case IDLE:
        break;
    case MOVING:
        if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
            float desired_distance = 2 * p_unit.stats->collision_radius;
            if ((p_unit.position).distance_squared_to(p_unit.target_pos) <= desired_distance * desired_distance) {
                p_unit.state = IDLE;
                p_unit.velocity = Vector2(0, 0);
                group_manager->decrement_moving_count(p_unit.temp_group_id);
            }
        }
        else {
            if (flow_field_manager->get_integration(p_unit.position, p_unit.target_pos, p_unit.get_nav_type()) <= desired_integration) {
                p_unit.state = IDLE;
                p_unit.velocity = Vector2(0, 0);
                group_manager->decrement_moving_count(p_unit.temp_group_id);
            }
        }
        break;
    }
}

void UnitManager::update_velocity(UnitData& p_unit, double p_delta) {
    Vector2 force = get_force(p_unit);
    bool is_combat_controlled = false;

    // ѯ   AttackManager  Ƿ ӹܴ˵ λ      
    if (attack_manager) {
        is_combat_controlled = attack_manager->try_get_combat_force(p_unit, force);
    }

    //    û б ս  ϵͳ ӹܣ   ʹ  Ĭ ϵ      ƶ  ߼ 
    if (!is_combat_controlled) {
        force = get_force(p_unit);
    }
    if (force.length_squared() < force_threshold_squared) {
        force = Vector2(0, 0);
    }
    
    float max_speed = (p_unit.stats)->get_move_speed();
    float accel = p_unit.stats->get_acceleration();

    Vector2 desired_velocity = (p_unit.velocity + force * p_delta).limit_length(max_speed);

    if (desired_velocity.length_squared() > velocity_threshold_squared) {
        float target_angle = desired_velocity.angle();
        float angle_diff = UtilityFunctions::angle_difference(p_unit.rotation, target_angle);

        // ת    ٶ  ߼ 
        float turn_accel = p_unit.stats->get_turn_acceleration();
        float max_turn_v = p_unit.stats->get_turn_speed();

        //  򵥵 ת   PD    ƻ   ٶ ģ  
        float target_angular_v = Math::sign(angle_diff) * max_turn_v;
        //     ӽ Ŀ  Ƕȣ      Է ֹ  
        if (Math::abs(angle_diff) < 0.5f) {
            target_angular_v = (angle_diff / 0.5f) * max_turn_v;
        }

        p_unit.angular_velocity = UtilityFunctions::move_toward(
            p_unit.angular_velocity,
            target_angular_v,
            turn_accel * p_delta
        );
    }
    else {
        // ֹͣʱ   ٶȹ   
        p_unit.angular_velocity = UtilityFunctions::move_toward(p_unit.angular_velocity, 0.0f, p_unit.stats->get_turn_acceleration() * p_delta);
    }

    p_unit.rotation += p_unit.angular_velocity * p_delta;

    float forward_dot = 0.0f;
    Vector2 forward_vec = Vector2(Math::cos(p_unit.rotation), Math::sin(p_unit.rotation));

    if (desired_velocity.length_squared() > velocity_threshold_squared) {
        forward_dot = Math::max(0.0f, forward_vec.dot(desired_velocity.normalized()));
    }

    //    ƣ ת  ʱ ٶȻή   (     dot  ˻   0.5   ʾƫ   60   ʱ   ٶȼ   )
    float current_accel = accel * (0.5f + 0.5f * forward_dot);

    // Ӧ ü  ٶ 
    p_unit.velocity = p_unit.velocity.move_toward(desired_velocity, current_accel * p_delta);

    if ((p_unit.velocity).length_squared() < velocity_threshold_squared) {
        p_unit.velocity= Vector2(0, 0);
    }
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    if (!flow_field_manager) return;
    //     Ԥ  λ  
    Vector2 next_pos = p_unit.position + p_unit.velocity * p_delta;
    float radius = (p_unit.stats)->get_collision_radius();
    Vector2i cell_size = flow_field_manager->get_cell_size();

    //      λ  ġ Ӳ          ֹ ص   
    // IDLE Ҵ   temp_group еĵ λ      
    if (1 || p_unit.state != IDLE || p_unit.temp_group_id == -1) {
        std::vector<int> nearby = get_nearby_units(next_pos, radius * 2.0f);
        for (int other_idx : nearby) {
            UnitData& other = units[other_idx];
            if (other.id == p_unit.id) continue;
            if (0 && other.state == IDLE && p_unit.temp_group_id != -1) continue;
            if ((p_unit.stats->move_type == MOVE_AIR) && (other.stats->move_type != MOVE_AIR) ||
                (p_unit.stats->move_type != MOVE_AIR) && (other.stats->move_type == MOVE_AIR)) {
                continue;
            }

            Vector2 to_other = other.position - next_pos;
            float dist_sq = to_other.length_squared();
            float min_dist = radius + (other.stats)->get_collision_radius();

            if (dist_sq < min_dist * min_dist && dist_sq > 0.001f) {
                float dist = Math::sqrt(dist_sq);
                float overlap = min_dist - dist;
                Vector2 resolve_dir = to_other / dist;

                //  ؼ  㣺ֻ ƿ  ص    ֵ һС   ֣      40%      ֹ       Ҷ   
                //          λ     ƶ       һ  
                float push_strength = 0.4f;
                Vector2 push_vector = resolve_dir * (overlap * push_strength);

                next_pos -= push_vector;
                // Ҳ    ˳    Է һ               Ϊ   ߼  򵥣 
                // ÿһ    λ   Լ    move ѭ   ﴦ    ƿ     
            }
        }
    }

    //     ǽ    ײ
    // ȷ    Ҫ        Χ (  λ  Χ   2x2    3x3     )
    // MOVE_AIR ĵ λ      

    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        p_unit.position = next_pos;
        return;
    }

    Vector2i min_grid = flow_field_manager->world_to_grid(next_pos - Vector2(radius, radius));
    Vector2i max_grid = flow_field_manager->world_to_grid(next_pos + Vector2(radius, radius));

    //       Χ ڵĸ  ӽ     ײ    
    for (int gx = min_grid.x; gx <= max_grid.x; ++gx) {
        for (int gy = min_grid.y; gy <= max_grid.y; ++gy) {
            Vector2i check_grid(gx, gy);

            //     ø     ǽ (Cost == 255)
            if (flow_field_manager->get_cost(check_grid, p_unit.get_nav_type()) == 255) {

                //       ӵ         ߽  (AABB)
                // ע ⣺              ϽǶ    ߼    world_to_grid һ  
                float rect_left = (float)gx * cell_size.x;
                float rect_top = (float)gy * cell_size.y;
                float rect_right = rect_left + cell_size.x;
                float rect_bottom = rect_top + cell_size.y;

                //  ҵ         Բ      ĵ 
                float closest_x = Math::clamp(next_pos.x, rect_left, rect_right);
                float closest_y = Math::clamp(next_pos.y, rect_top, rect_bottom);
                Vector2 closest_point(closest_x, closest_y);

                //     Բ ĵ           
                Vector2 diff = next_pos - closest_point;
                float distance_squared = diff.length_squared();

                //        С ڰ뾶        ײ

                if (distance_squared < radius * radius && distance_squared > 0.00001f) {
                    float factor = 0.5;
                    if (p_unit.state == IDLE) {
                        factor = 1.0;
                    }

                    float distance = Math::sqrt(distance_squared);
                    float overlap = radius - distance;

                    //     λ    ײ     ƿ 
                    next_pos += (diff / distance) * overlap * factor;

                    //   ײ  ͨ    Ҫ     ÷     ٶȣ ʵ ֡   ǽ      
                    Vector2 normal = diff / distance;
                    if (p_unit.velocity.dot(normal) < 0) {
                        //   ȥ   ߷     ٶȷ   
                        p_unit.velocity -= normal * p_unit.velocity.dot(normal);
                    }
                }
                //     Բ        ǽ ڵ    
                else if (distance_squared <= 0.00001f) {
                    //    Դ            ĵķ       
                    Vector2 cell_center(rect_left + cell_size.x * 0.5f, rect_top + cell_size.y * 0.5f);
                    Vector2 push_dir = (next_pos - cell_center).normalized();
                    next_pos += push_dir * radius;
                }
            }
        }
    }

    // 4. Ӧ      λ  
    p_unit.position = next_pos;
}

void UnitManager::update_multimesh_buffer(double p_delta, float p_alpha, SelectionManager* p_selection_manager) {
    if (type_renderers.empty()) return;

    // 1.      һ֡ ķ   
    for (auto& pair : type_grouping_cache) {
        pair.second.clear();
    }

    // 2.    UnitStats ָ     
    for (int i = 0; i < units.size(); ++i) {
        // ֱ  ȡ UnitStats   ԭʼָ  
        UnitStats* s_ptr = units[i].stats.ptr();
        type_grouping_cache[s_ptr].push_back(i);
    }

    // 3.       Ⱦ           
    for (auto const& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& indices = type_grouping_cache[s_ptr];
        int count = indices.size();

        if (count == 0) continue;

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
        }

        //   ȡӰ    Ⱦ  
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        s_mm->set_instance_count(count);

        //      s_ptr      UnitStats ָ 룬    ֱ ӷ           
        for (int i = 0; i < count; ++i) {
            int u_idx = indices[i];
            UnitData& unit = units[u_idx];

            //    ±任
            Transform3D xform;

            //     ӳ 䣺
            // 2D X -> 3D X
            // 2D Y -> 3D Z (  ȣ      GPU  Զ  Y-Sort)
            // 2D Height -> 3D Y ( Ӿ  ߶ )
            Vector2 visual_position = UtilityFunctions::lerp(unit.prev_position, unit.next_position, p_alpha);
            float visual_height = UtilityFunctions::lerp(unit.prev_height, unit.next_height, p_alpha);
            float visual_rotation = UtilityFunctions::lerp_angle(unit.prev_rotation, unit.next_rotation, p_alpha);

            float fake_depth_offset = visual_position.y * 0.0001f;
            Vector3 pos_3d = Vector3(visual_position.x, visual_height + fake_depth_offset, visual_position.y - visual_height);
            xform.origin = pos_3d;

            //   ת     QuadMesh         QuadMesh Ĭ     XY ƽ 棬      Ҫ       XZ ƽ   ϣ 
            //          Ǹ  ӵģ       Ҫ   X     ת -90   
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (visual_rotation + Math_PI / 2.0f));

            mm->set_instance_transform(i, xform);

            //      ߼   ֱ  ʹ   s_ptr  
            int frames = (unit.state == MOVING) ? s_ptr->get_move_frames() : s_ptr->get_idle_frames();
            int row = (unit.state == MOVING) ? s_ptr->get_move_row() : s_ptr->get_idle_row();
            float duration = (float)frames / s_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(unit.anim_time, duration) * s_ptr->get_anim_fps());

            float modulate = 1.0;
            if (p_selection_manager->is_unit_selected(unit.id)) {
                modulate = 1.5f;
            }
            else if (p_selection_manager->is_unit_hovered(unit.id)) {
                modulate = 1.2f;
            }

            mm->set_instance_custom_data(i, Color(frame_idx, row, modulate, 0));

            //      ɫ
            
            mm->set_instance_color(i, get_team_color(unit.team_id));


            //     Ӱ ӱ任 (XZƽ    ƽ)
            Transform3D shadow_xform;

            //     Ӱ  λ ã   ΢ƫ  һ  㣬      ̹   Ĵ        ⿿һ  
            //             Ϸ      ǽ Ӱ       ½ ƫ  
            float shadow_offset_x = 4.0f; //        ̹ ˳ߴ    
            float shadow_offset_z = 4.0f;

            // Ӱ ӷ  ڵ   ߶ȣ   һ    С  ƫ   (0.001)   ֹ      Z-Fighting
            // ע ⣺Ӱ ӵ  origin.y      unit.height  仯      Զ ڵ   
            shadow_xform.origin = Vector3(visual_position.x + shadow_offset_x,
                visual_height + fake_depth_offset - 0.1f,
                visual_position.y + shadow_offset_z);

            // Ӱ       ŵ 
            shadow_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            shadow_xform.basis = (shadow_xform.basis).rotated(Vector3(0, -1, 0), (visual_rotation + Math_PI / 2.0f));

            //      ϣ  Ӱ    б      У                  
            // shadow_xform.basis = shadow_xform.basis.scaled(Vector3(1.0, 1.5, 1.0));

            s_mm->set_instance_transform(i, shadow_xform);

            // ͬ         ݣ Ӱ  ҲҪ    
            s_mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            unit.anim_time += p_delta;
        }
    }
}

Vector2 UnitManager::get_unit_position(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    
    if (it != id_to_index.end()) {
        return units[it->second].position;
    }

    return Vector2(0, 0);
}

int UnitManager::get_unit_state(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);

    if (it != id_to_index.end()) {
        return (int)(units[it->second].state);
    }

    return (int)(IDLE);
}

Ref<UnitStats> godot::UnitManager::get_unit_stats_by_type(String p_type_name)
{
    if (unit_types_cache.has(p_type_name)) {
        return unit_types_cache[p_type_name];
    }
    return nullptr;
}

void UnitManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void UnitManager::set_group_manager(Node* p_node) {
    group_manager = Object::cast_to<GroupManager>(p_node);
}

void UnitManager::register_unit_type(String p_name, String p_path) {
    Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(p_path);
    if (stats.is_null()) return;

    //    뻺 湩 spawn_unit_by_type ʹ  
    unit_types_cache[p_name] = stats;

    //   ȡԭʼָ    Ϊ Key
    UnitStats* stats_ptr = stats.ptr();

    //           Ѿ ע     Ⱦ    ֱ ӷ   
    if (type_renderers.find(stats_ptr) != type_renderers.end()) return;

    // ---       Ⱦ   ---
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_name + "_Renderer");
    add_child(mmi);

    //      MultiMesh
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();

    //                   С
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    //    ò   
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (unit_shader.is_null()) {
        unit_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shader.gdshader");
    }
    mat->set_shader(unit_shader);
    mat->set_shader_parameter("h_frames", stats->get_h_frames());
    mat->set_shader_parameter("v_frames", stats->get_v_frames());
    mat->set_shader_parameter("albedo_texture", tex);

    mmi->set_material_override(mat);

    //     ӳ  
    type_renderers[stats_ptr] = mmi;

    // ---     Ӱ    Ⱦ   ---
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadows");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true);

    // Ӱ ӿ   ʹ ø  򵥵  QuadMesh
    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        s_qmesh->set_size(frame_size);
    }
    // ...          С ...
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    //     Ӱ Ӳ   
    Ref<ShaderMaterial> s_mat;
    s_mat.instantiate();
    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/unit_shadow.gdshader");
    }
    s_mat->set_shader(shadow_shader);
    s_mat->set_shader_parameter("albedo_texture", tex);
    s_mat->set_shader_parameter("h_frames", stats->get_h_frames());
    s_mat->set_shader_parameter("v_frames", stats->get_v_frames());
    s_mmi->set_material_override(s_mat);

    //      Map
    shadow_renderers[stats_ptr] = s_mmi;
}

int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id, int p_forced_id) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name], p_team_id, p_forced_id);
    }
    return -1;
}

void UnitManager::set_control_group(int p_index, const std::vector<int>& p_unit_ids) {
    if (p_index < 0 || p_index >= group_manager->MAX_CONTROL_GROUPS) return;

    // 1.     ɵ λ  ˫  ӳ  
    for (int old_uid : group_manager->control_groups[p_index]) {
        //֮  д ɺ   
        int index = -1;
        auto it = id_to_index.find(old_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        //  ӹ̶        Ƴ     
        for (int i = 0; i < data.control_group_count; ++i) {
            if (data.control_group_indices[i] == p_index) {
                data.control_group_indices[i] = data.control_group_indices[data.control_group_count - 1];
                data.control_group_count--;
                break;
            }
        }
    }

    // 2.    ±      
    group_manager->control_groups[p_index] = p_unit_ids;

    // 3.      µ λ  ˫  ӳ  
    for (int new_uid : p_unit_ids) {
        //֮  д ɺ   
        int index = -1;
        auto it = id_to_index.find(new_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        if (data.control_group_count < 3) { //          3
            data.control_group_indices[data.control_group_count++] = p_index;
        }
    }
}

int UnitManager::get_unit_index_by_id(int p_id) {
    auto it = id_to_index.find(p_id);
    if (it != id_to_index.end()) {
        return it->second;
    }
    return -1;
}

void UnitManager::set_attack_manager(Node* p_node) {
    attack_manager = Object::cast_to<AttackManager>(p_node);
    if (attack_manager) {
        attack_manager->setup(this); 
    }
}

float UnitManager::get_unit_aggro_range(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) {
        // ֱ Ӵӵ λ   stats  ж ȡ   غõ   ʵ    
        return units[it->second].stats->get_aggro_range();
    }
    return 0.0f;
}

float UnitManager::get_unit_attack_range(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) {
        return units[it->second].stats->get_attack_range();
    }
    return 0.0f;
}

void UnitManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type", "team_id"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos", "team_id", "force_id"), &UnitManager::spawn_unit_by_type,
        DEFVAL(0),  // 对应 p_team_id 的默认值
        DEFVAL(-1)  // 对应 p_forced_id 的默认值
    );
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("command_units_to_patrol", "unit_ids", "waypoints"), &UnitManager::command_units_to_patrol);
    ClassDB::bind_method(D_METHOD("command_units_to_attack_target", "unit_ids", "target_id"), &UnitManager::command_units_to_attack_target);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &UnitManager::set_group_manager);
    ClassDB::bind_method(D_METHOD("set_attack_manager", "node"), &UnitManager::set_attack_manager);
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
    ClassDB::bind_method(D_METHOD("get_unit_aggro_range", "unit_id"), &UnitManager::get_unit_aggro_range);
    ClassDB::bind_method(D_METHOD("get_unit_attack_range", "unit_id"), &UnitManager::get_unit_attack_range);
    

    //    
    // 1.  Ȱ    з    (Getter/Setter)
    ClassDB::bind_method(D_METHOD("get_flow_factor"), &UnitManager::get_flow_factor);
    ClassDB::bind_method(D_METHOD("set_flow_factor", "p_val"), &UnitManager::set_flow_factor);

    ClassDB::bind_method(D_METHOD("get_separation_factor"), &UnitManager::get_separation_factor);
    ClassDB::bind_method(D_METHOD("set_separation_factor", "p_val"), &UnitManager::set_separation_factor);

    ClassDB::bind_method(D_METHOD("get_separation_limit"), &UnitManager::get_separation_limit);
    ClassDB::bind_method(D_METHOD("set_separation_limit", "p_val"), &UnitManager::set_separation_limit);

    ClassDB::bind_method(D_METHOD("get_separation_radius_factor"), &UnitManager::get_separation_radius_factor);
    ClassDB::bind_method(D_METHOD("set_separation_radius_factor", "p_val"), &UnitManager::set_separation_radius_factor);

    ClassDB::bind_method(D_METHOD("get_lateral_separation_factor"), &UnitManager::get_lateral_separation_factor);
    ClassDB::bind_method(D_METHOD("set_lateral_separation_factor", "p_val"), &UnitManager::set_lateral_separation_factor);

    ClassDB::bind_method(D_METHOD("get_friction_factor"), &UnitManager::get_friction_factor);
    ClassDB::bind_method(D_METHOD("set_friction_factor", "p_val"), &UnitManager::set_friction_factor);

    ClassDB::bind_method(D_METHOD("get_force_threshold_squared"), &UnitManager::get_force_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_force_threshold_squared", "p_val"), &UnitManager::set_force_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_velocity_threshold_squared"), &UnitManager::get_velocity_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_velocity_threshold_squared", "p_val"), &UnitManager::set_velocity_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_desired_integration"), &UnitManager::get_desired_integration);
    ClassDB::bind_method(D_METHOD("set_desired_integration", "p_val"), &UnitManager::set_desired_integration);

    // 2. ע     Ե  Godot        

    ADD_GROUP("Force Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flow_factor"), "set_flow_factor", "get_flow_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_factor"), "set_separation_factor", "get_separation_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_limit"), "set_separation_limit", "get_separation_limit");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_radius_factor"), "set_separation_radius_factor", "get_separation_radius_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lateral_separation_factor"), "set_lateral_separation_factor", "get_lateral_separation_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction_factor"), "set_friction_factor", "get_friction_factor");

    ADD_GROUP("Threshold Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "force_threshold_squared"), "set_force_threshold_squared", "get_force_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity_threshold_squared"), "set_velocity_threshold_squared", "get_velocity_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "desired_integration"), "set_desired_integration", "get_desired_integration");
}




