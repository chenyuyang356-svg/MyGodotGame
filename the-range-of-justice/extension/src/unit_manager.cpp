#include "unit_manager.h"
#include "attack_manager.h"

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

int UnitManager::spawn_unit(Vector2 p_world_pos, Ref<UnitStats> p_stats, int p_team_id) {
    if (p_stats.is_null()) return -1;

    UnitData new_unit;
    new_unit.id = next_unit_id++;
    new_unit.position = p_world_pos;
    new_unit.height = p_stats->base_height;
    new_unit.stats = p_stats; // ´æÈëÒıÓÃ

    new_unit.team_id = p_team_id; // ¸³ÖµÕóÓª

    // ´Ó×ÊÔ´ÖĞ³õÊ¼»¯ÊµÊ±ÊıÖµ
    new_unit.current_health = p_stats->get_health_max();
    new_unit.state = IDLE;

    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;
    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

    UnitData& unit_to_remove = units[index_to_remove];
    group_manager->handle_unit_death(p_unit_id, unit_to_remove.temp_group_id, unit_to_remove.control_group_indices, unit_to_remove.control_group_count);

    if (index_to_remove != last_unit_idx) {
        // 1. »ñÈ¡×îºóÒ»¸öµ¥Î»µÄÊı¾İ
        UnitData& last_unit = units.back();

        // 2. ½«×îºóÒ»¸öµ¥Î»ÒÆ¶¯µ½ÒªÉ¾³ıµÄÎ»ÖÃ
        units[index_to_remove] = last_unit;

        // 3. ¸üĞÂ±»ÒÆ¶¯µ¥Î»ÔÚ¹şÏ£±íÖĞµÄË÷Òı
        id_to_index[last_unit.id] = index_to_remove;
    }

    // 4. É¾³ı vector ×îºóÒ»¸öÔªËØ£¬²¢´Ó¹şÏ£±íÖĞÒÆ³ıÄ¿±ê ID
    units.pop_back();
    id_to_index.erase(p_unit_id);
}

void UnitManager::command_units_to_move(Array p_unit_ids, Vector2 p_target_world_pos) {
    if (!flow_field_manager) return;

    Vector2i target_grid_pos = flow_field_manager->world_to_grid(p_target_world_pos);

    if (!(flow_field_manager->is_in_grid(target_grid_pos))) return;

    flow_field_manager->create_flow_field(target_grid_pos, false);

    for (int i = 0; i < p_unit_ids.size(); i++) {
        int uid = p_unit_ids[i];
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];
            unit.is_patrolling = false;      
            unit.target_grid = target_grid_pos;
            unit.state = MOVING;
            unit.target_id = -1;
        }
    }
}

void UnitManager::command_units_to_patrol(Array p_unit_ids, Array p_waypoints) {
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
        }
    }
}

void UnitManager::update_spatial_grid() {
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].clear();
    }
    for (int i = 0; i < units.size(); ++i) {
        Vector2i rel_pos = flow_field_manager->world_to_relative(units[i].position);

        // Ëõ·Åµ½µ¥Î»Íø¸ñ£¨µ¥Î»Íø¸ñ³ß´çÊÇÁ÷³¡µÄ 2 ±¶£©
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

    // ¼ì²é 3x3 ·¶Î§ÄÚµÄ¸ñ×Ó
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

void UnitManager::update(double p_delta) {
    if (!is_setup || !flow_field_manager || !selection_manager) { return; }

    if (attack_manager) {
        attack_manager->update_units(p_delta);
    }

    selection_manager->selected_unit_id = -1;
    float selected_unit_height = -100.0f;
    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        float selection_radius = (unit.stats)->get_collision_radius();
        if (((selection_manager->mouse_position).distance_squared_to(unit.position) <
            (selection_radius) * (selection_radius)) &&
            (selection_manager->state != selection_manager->BOX_SELECTING) &&
            (unit.height > selected_unit_height)) {
            selection_manager->selected_unit_id = unit.id;
            selection_manager->selected_unit_stats = unit.stats;
            selection_manager->selected_team_id = unit.team_id;
            selected_unit_height = unit.height;
        }
    }

    

    int new_gid = -1;
    if (selection_manager->state == selection_manager->SELECTING_TARGET_POSITION) {
        new_gid = group_manager->create_temporary_group(selection_manager->mouse_position);
    }

    update_spatial_grid();
    flow_field_manager->update(p_delta);

    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        update_state(unit);
        update_selection_state_and_target_position(unit, new_gid);
        update_velocity(unit, p_delta);
        move(unit, p_delta);
    }
    if ((selection_manager->state == selection_manager->SINGLE_SELECTING) ||
        (selection_manager->state == selection_manager->TYPE_SELECTING) ||
        (selection_manager->state == selection_manager->BOX_SELECTION_ENDED) ||
        (selection_manager->state == selection_manager->SELECTING_TARGET_POSITION)) {
        selection_manager->state = selection_manager->NOT_SELECTING;
    }

    update_multimesh_buffer(p_delta);
}

Vector2 UnitManager::get_flow(UnitData& p_unit) {
    Vector2 flow;
    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        flow = (p_unit.target_pos - p_unit.position).normalized();
        return flow;
    }
    flow = flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos);
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
            if (flow_field_manager->get_integration(p_unit.position, p_unit.target_pos) <= desired_integration) {
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

    // Ñ¯ÎÊ AttackManager ÊÇ·ñ½Ó¹Ü´Ëµ¥Î»µÄÎïÀí
    if (attack_manager) {
        is_combat_controlled = attack_manager->try_get_combat_force(p_unit, force);
    }

    // Èç¹ûÃ»ÓĞ±»Õ½¶·ÏµÍ³½Ó¹Ü£¬ÔòÊ¹ÓÃÄ¬ÈÏµÄÁ÷³¡ÒÆ¶¯Âß¼­
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

        // ×ªÏò¼ÓËÙ¶ÈÂß¼­
        float turn_accel = p_unit.stats->get_turn_acceleration();
        float max_turn_v = p_unit.stats->get_turn_speed();

        // ¼òµ¥µÄ×ªÏò PD ¿ØÖÆ»ò¼ÓËÙ¶ÈÄ£Äâ
        float target_angular_v = Math::sign(angle_diff) * max_turn_v;
        // Èç¹û½Ó½üÄ¿±ê½Ç¶È£¬¼õËÙÒÔ·ÀÖ¹Õğµ´
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
        // Í£Ö¹Ê±½ÇËÙ¶È¹éÁã
        p_unit.angular_velocity = UtilityFunctions::move_toward(p_unit.angular_velocity, 0.0f, p_unit.stats->get_turn_acceleration() * p_delta);
    }

    p_unit.rotation += p_unit.angular_velocity * p_delta;

    float forward_dot = 0.0f;
    Vector2 forward_vec = Vector2(Math::cos(p_unit.rotation), Math::sin(p_unit.rotation));

    if (desired_velocity.length_squared() > velocity_threshold_squared) {
        forward_dot = Math::max(0.0f, forward_vec.dot(desired_velocity.normalized()));
    }

    // ÏŞÖÆ£º×ªÏòÊ±ËÙ¶È»á½µµÍ (¸ù¾İ dot ³Ë»ı£¬0.5 ±íÊ¾Æ«Àë 60 ¶ÈÊ±¼ÓËÙ¶È¼õ°ë)
    float current_accel = accel * (0.5f + 0.5f * forward_dot);

    // Ó¦ÓÃ¼ÓËÙ¶È
    p_unit.velocity = p_unit.velocity.move_toward(desired_velocity, current_accel * p_delta);

    if ((p_unit.velocity).length_squared() < velocity_threshold_squared) {
        p_unit.velocity= Vector2(0, 0);
    }
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    if (!flow_field_manager) return;

    // ¼ÆËãÔ¤²âÎ»ÖÃ
    Vector2 next_pos = p_unit.position + p_unit.velocity * p_delta;
    float radius = (p_unit.stats)->get_collision_radius();
    Vector2i cell_size = flow_field_manager->get_cell_size();

    // ´¦Àíµ¥Î»¼äµÄ¡°Ó²ĞŞÕı¡±£¨·ÀÖ¹ÖØµş£©
    // IDLEÇÒ´¦ÓÚtemp_groupÖĞµÄµ¥Î»²»²ÎÓë
    if (1 || p_unit.state != IDLE || p_unit.temp_group_id == -1) {
        std::vector<int> nearby = get_nearby_units(next_pos, radius * 2.0f);
        for (int other_idx : nearby) {
            UnitData& other = units[other_idx];
            if (other.id == p_unit.id) continue;
            if (other.state == IDLE && p_unit.temp_group_id != -1) continue;
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

                // ¹Ø¼üµã£ºÖ»ÍÆ¿ªÖØµş²¿·ÖµÄÒ»Ğ¡²¿·Ö£¨ÀıÈç 40%£©£¬·ÀÖ¹²úÉú¾çÁÒ¶¶¶¯
                // Èç¹ûÁ½¸öµ¥Î»¶¼ÔÚÒÆ¶¯£¬¸÷ÍÆÒ»°ë
                float push_strength = 0.4f;
                Vector2 push_vector = resolve_dir * (overlap * push_strength);

                next_pos -= push_vector;
                // Ò²¿ÉÒÔË³±ã¸ø¶Ô·½Ò»¸ö·´ÏòµÄÍÆÁ¦£¬µ«ÎªÁËÂß¼­¼òµ¥£¬
                // Ã¿Ò»¸öµ¥Î»ÔÚ×Ô¼ºµÄ move Ñ­»·Àï´¦Àí±»ÍÆ¿ª¼´¿É
            }
        }
    }

    // ´¦ÀíÇ½ÌåÅö×²
    // È·¶¨ĞèÒª¼ì²éµÄÍø¸ñ·¶Î§ (µ¥Î»ÖÜÎ§µÄ 2x2 »ò 3x3 ÇøÓò)
    // MOVE_AIRµÄµ¥Î»²»²ÎÓë

    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        p_unit.position = next_pos;
        return;
    }

    Vector2i min_grid = flow_field_manager->world_to_grid(next_pos - Vector2(radius, radius));
    Vector2i max_grid = flow_field_manager->world_to_grid(next_pos + Vector2(radius, radius));

    // ±éÀú·¶Î§ÄÚµÄ¸ñ×Ó½øĞĞÅö×²´¦Àí
    for (int gx = min_grid.x; gx <= max_grid.x; ++gx) {
        for (int gy = min_grid.y; gy <= max_grid.y; ++gy) {
            Vector2i check_grid(gx, gy);

            // Èç¹û¸Ã¸ñ×ÓÊÇÇ½ (Cost == 255)
            if (flow_field_manager->get_cost(check_grid) == 255) {

                // ¼ÆËã¸ñ×ÓµÄÊÀ½ç×ø±ê±ß½ç (AABB)
                // ×¢Òâ£ºÕâÀï¼ÙÉèÍø¸ñ×óÉÏ½Ç¶ÔÆëÂß¼­Óë world_to_grid Ò»ÖÂ
                float rect_left = (float)gx * cell_size.x;
                float rect_top = (float)gy * cell_size.y;
                float rect_right = rect_left + cell_size.x;
                float rect_bottom = rect_top + cell_size.y;

                // ÕÒµ½¾ØĞÎÄÚÀëÔ²ĞÄ×î½üµÄµã
                float closest_x = Math::clamp(next_pos.x, rect_left, rect_right);
                float closest_y = Math::clamp(next_pos.y, rect_top, rect_bottom);
                Vector2 closest_point(closest_x, closest_y);

                // ¼ÆËãÔ²ĞÄµ½×î½üµãµÄÏòÁ¿
                Vector2 diff = next_pos - closest_point;
                float distance_squared = diff.length_squared();

                // Èç¹û¾àÀëĞ¡ÓÚ°ë¾¶£¬·¢ÉúÅö×²

                if (distance_squared < radius * radius && distance_squared > 0.00001f) {
                    float factor = 0.5;
                    if (p_unit.state == IDLE) {
                        factor = 1.0;
                    }

                    float distance = Math::sqrt(distance_squared);
                    float overlap = radius - distance;

                    // ½«µ¥Î»ÑØÅö×²·¨ÏßÍÆ¿ª
                    next_pos += (diff / distance) * overlap * factor;

                    // Åö×²ºóÍ¨³£ĞèÒªÏ÷¼õ¸Ã·½ÏòµÄËÙ¶È£¬ÊµÏÖ¡°ÑØÇ½»¬¶¯¡±
                    Vector2 normal = diff / distance;
                    if (p_unit.velocity.dot(normal) < 0) {
                        // ¼õÈ¥·¨Ïß·½ÏòµÄËÙ¶È·ÖÁ¿
                        p_unit.velocity -= normal * p_unit.velocity.dot(normal);
                    }
                }
                // ´¦ÀíÔ²ĞÄÕıºÃÔÚÇ½ÄÚµÄÇé¿ö
                else if (distance_squared <= 0.00001f) {
                    // ´ÖÂÔ´¦Àí£ºÏò¸ñ×ÓÖĞĞÄµÄ·´·½ÏòÍÆ
                    Vector2 cell_center(rect_left + cell_size.x * 0.5f, rect_top + cell_size.y * 0.5f);
                    Vector2 push_dir = (next_pos - cell_center).normalized();
                    next_pos += push_dir * radius;
                }
            }
        }
    }

    // 4. Ó¦ÓÃ×îÖÕÎ»ÖÃ
    p_unit.position = next_pos;
}

void UnitManager::update_multimesh_buffer(double p_delta) {
    if (type_renderers.empty()) return;

    // 1. Çå¿ÕÉÏÒ»Ö¡µÄ·Ö×é
    for (auto& pair : type_grouping_cache) {
        pair.second.clear();
    }

    // 2. °´ UnitStats Ö¸Õë·Ö×é
    for (int i = 0; i < units.size(); ++i) {
        // Ö±½ÓÈ¡ UnitStats µÄÔ­Ê¼Ö¸Õë
        UnitStats* s_ptr = units[i].stats.ptr();
        type_grouping_cache[s_ptr].push_back(i);
    }

    // 3. ±éÀúäÖÈ¾Æ÷²¢Ìî³äÊı¾İ
    for (auto const& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& indices = type_grouping_cache[s_ptr];
        int count = indices.size();

        if (count == 0) continue;

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
        }

        // »ñÈ¡Ó°×ÓäÖÈ¾Æ÷
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        s_mm->set_instance_count(count);

        // ÕâÀï s_ptr ¾ÍÊÇ UnitStats Ö¸Õë£¬¿ÉÒÔÖ±½Ó·ÃÎÊÅäÖÃÊı¾İ
        for (int i = 0; i < count; ++i) {
            int u_idx = indices[i];
            UnitData& unit = units[u_idx];

            // ¸üĞÂ±ä»»
            Transform3D xform;

            // ×ø±êÓ³Éä£º
            // 2D X -> 3D X
            // 2D Y -> 3D Z (Éî¶È£¬ÓÃÓÚ GPU ×Ô¶¯ Y-Sort)
            // 2D Height -> 3D Y (ÊÓ¾õ¸ß¶È)
            float fake_depth_offset = unit.position.y * 0.0001f;
            Vector3 pos_3d = Vector3(unit.position.x, unit.height + fake_depth_offset, unit.position.y - unit.height);
            xform.origin = pos_3d;

            // Ğı×ª£ºÈÃ QuadMesh Á¢ÆğÀ´£¨QuadMesh Ä¬ÈÏÔÚ XY Æ½Ãæ£¬ÎÒÃÇĞèÒªËüÁ¢ÔÚ XZ Æ½ÃæÉÏ£©
            // Èç¹ûÉãÏñ»úÊÇ¸©ÊÓµÄ£¬ÎÒÃÇĞèÒªÈÆ X ÖáĞı×ª -90 ¶È
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (unit.rotation + Math_PI / 2.0f));

            mm->set_instance_transform(i, xform);

            // ¶¯»­Âß¼­£¨Ö±½ÓÊ¹ÓÃ s_ptr£©
            int frames = (unit.state == MOVING) ? s_ptr->get_move_frames() : s_ptr->get_idle_frames();
            int row = (unit.state == MOVING) ? s_ptr->get_move_row() : s_ptr->get_idle_row();
            float duration = (float)frames / s_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(unit.anim_time, duration) * s_ptr->get_anim_fps());

            mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            //´¦ÀíÑÕÉ«
            Color display_color;
            if (unit.is_selected) {
                if (unit.is_mouse_on) {
                    display_color = Color(1.2, 1.2, 1.2);
                }
                else {
                    display_color = Color(1.5, 1.5, 1.5);
                }
            }
            else {
                if (unit.is_mouse_on) {
                    display_color = Color(1.2, 1.2, 1.2);
                }
                else {
                    display_color = Color(1.0, 1.0, 1.0);
                }
            }
            mm->set_instance_color(i, display_color);


            // ¸üĞÂÓ°×Ó±ä»» (XZÆ½ÃæÌÉÆ½)
            Transform3D shadow_xform;

            // ÉèÖÃÓ°×ÓÎ»ÖÃ£ºÉÔÎ¢Æ«ÒÆÒ»µãµã£¬ÈÃËü´ÓÌ¹¿ËÂÄ´øÖĞĞÄÍùÍâ¿¿Ò»µã
            // ¼ÙÉè¹âÀ´×Ô×óÉÏ·½£¬ÎÒÃÇ½«Ó°×ÓÏòÓÒÏÂ½ÇÆ«ÒÆ
            float shadow_offset_x = 4.0f; // ¸ù¾İÄãµÄÌ¹¿Ë³ß´çµ÷Õû
            float shadow_offset_z = 4.0f;

            // Ó°×Ó·ÅÔÚµØÃæ¸ß¶È£¬¸øÒ»¸ö¼«Ğ¡µÄÆ«ÒÆ (0.001) ·ÀÖ¹ÓëµØÃæ Z-Fighting
            // ×¢Òâ£ºÓ°×ÓµÄ origin.y ²»Ëæ unit.height ±ä»¯£¬ËüÓÀÔ¶ÔÚµØÉÏ
            shadow_xform.origin = Vector3(unit.position.x + shadow_offset_x,
                unit.height + fake_depth_offset - 0.1f,
                unit.position.y + shadow_offset_z);

            // Ó°×ÓÊÇÌÉ×ÅµÄ
            shadow_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            shadow_xform.basis = (shadow_xform.basis).rotated(Vector3(0, -1, 0), (unit.rotation + Math_PI / 2.0f));

            // Èç¹ûÄãÏ£ÍûÓ°×ÓÓĞĞ±ÏòÀ­Éì¸Ğ£¬¿ÉÒÔÔÚÕâÀïµş¼ÓËõ·Å
            // shadow_xform.basis = shadow_xform.basis.scaled(Vector3(1.0, 1.5, 1.0));

            s_mm->set_instance_transform(i, shadow_xform);

            // Í¬²½¶¯»­Êı¾İ£¨Ó°×ÓÒ²Òª¶¯£©
            s_mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            unit.anim_time += p_delta;
        }
    }
}

void UnitManager::update_selection_state_and_target_position(UnitData& p_unit, int p_group_id) {
    if (selection_manager->state != selection_manager->BOX_SELECTING) {
        if (p_unit.id == selection_manager->selected_unit_id) {
            p_unit.is_mouse_on = true;
        }
        else {
            p_unit.is_mouse_on = false;
        }
    }

    switch (selection_manager->state) {
    case (selection_manager->NOT_SELECTING):
        break;
    case (selection_manager->SINGLE_SELECTING):
        if (selection_manager->selected_unit_id == -1) {
            break;
        }
        else {
            if (selection_manager->selected_unit_id == p_unit.id) {
                p_unit.is_selected = !p_unit.is_selected;
            }
            else {
                p_unit.is_selected = false;
            }
        }
        break;
    case (selection_manager->TYPE_SELECTING):
        if (selection_manager->selected_unit_id == -1) {
            break;
        }
        else {
            if ((p_unit.stats == selection_manager->selected_unit_stats) &&
                (p_unit.team_id == selection_manager->selected_team_id) && 
                (p_unit.team_id == selection_manager->team_id)) {
                p_unit.is_selected = true;
            }
            else {
                p_unit.is_selected = false;
            }
        }
        break;
    case (selection_manager->BOX_SELECTING):
        if ((selection_manager->selecting_box).has_point(p_unit.position)) {
            p_unit.is_mouse_on = true;
        }
        else {
            p_unit.is_mouse_on = false;
        }
        break;
    case (selection_manager->BOX_SELECTION_ENDED):
        if ((selection_manager->selecting_box).has_point(p_unit.position) && 
            (selection_manager->team_id == p_unit.team_id)) {
            p_unit.is_selected = true;
        }
        else {
            p_unit.is_selected = false;
        }
        break;
    case (selection_manager->SELECTING_TARGET_POSITION):
        if ((p_unit.is_selected) && (selection_manager->team_id == p_unit.team_id)) {
            Vector2i target_grid_pos = flow_field_manager->world_to_grid(selection_manager->mouse_position);
            if ((p_unit.stats)->get_move_type() != MOVE_AIR) {
                flow_field_manager->create_flow_field(target_grid_pos, false);
            }
            p_unit.target_pos = selection_manager->mouse_position;
            p_unit.target_grid = target_grid_pos;
            p_unit.state = MOVING;

            if (p_unit.temp_group_id != -1) {
                // Õâ¸öº¯Êı»á£º1.´Ó¾É×éIDÁĞ±íÒÆ³ı×Ô¼º(O(1)) 2.Èç¹ûÖ®Ç°ÔÚÒÆ¶¯£¬¼õÉÙ¾É×é¼ÆÊı
                group_manager->remove_unit_from_temp_group(p_unit.temp_group_id, p_unit.id);
            }
            p_unit.temp_group_id = p_group_id;
            group_manager->add_unit_to_temp_group(p_group_id, p_unit.id);
        }
        break;
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

void UnitManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void UnitManager::set_selection_manager(Node* p_node) {
    selection_manager = Object::cast_to<SelectionManager>(p_node);
}

void UnitManager::set_group_manager(Node* p_node) {
    group_manager = Object::cast_to<GroupManager>(p_node);
}

void UnitManager::register_unit_type(String p_name, String p_path) {
    Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(p_path);
    if (stats.is_null()) return;

    // ´æÈë»º´æ¹© spawn_unit_by_type Ê¹ÓÃ
    unit_types_cache[p_name] = stats;

    // »ñÈ¡Ô­Ê¼Ö¸Õë×÷Îª Key
    UnitStats* stats_ptr = stats.ptr();

    // Èç¹û¸ÃÀàĞÍÒÑ¾­×¢²á¹ıäÖÈ¾Æ÷£¬Ö±½Ó·µ»Ø
    if (type_renderers.find(stats_ptr) != type_renderers.end()) return;

    // --- ´´½¨äÖÈ¾Æ÷ ---
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_name + "_Renderer");
    add_child(mmi);

    // ÅäÖÃ MultiMesh
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();

    // ¼ÓÔØÎÆÀí²¢ÉèÖÃÍø¸ñ´óĞ¡
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    // ÉèÖÃ²ÄÖÊ
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

    // ½¨Á¢Ó³Éä
    type_renderers[stats_ptr] = mmi;

    // --- ´´½¨Ó°×ÓäÖÈ¾Æ÷ ---
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadows");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true);

    // Ó°×Ó¿ÉÒÔÊ¹ÓÃ¸ü¼òµ¥µÄ QuadMesh
    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        s_qmesh->set_size(frame_size);
    }
    // ... ÉèÖÃÍø¸ñ´óĞ¡ ...
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    // ÉèÖÃÓ°×Ó²ÄÖÊ
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

    // ´æÈë Map
    shadow_renderers[stats_ptr] = s_mmi;
}

int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name], p_team_id);
    }
    return -1;
}

void UnitManager::set_control_group(int p_index, const std::vector<int>& p_unit_ids) {
    if (p_index < 0 || p_index >= group_manager->MAX_CONTROL_GROUPS) return;

    // 1. ÇåÀí¾Éµ¥Î»µÄË«ÏòÓ³Éä
    for (int old_uid : group_manager->control_groups[p_index]) {
        //Ö®ºóĞ´³Éº¯Êı
        int index = -1;
        auto it = id_to_index.find(old_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        // ´Ó¹Ì¶¨Êı×éÖĞÒÆ³ıË÷Òı
        for (int i = 0; i < data.control_group_count; ++i) {
            if (data.control_group_indices[i] == p_index) {
                data.control_group_indices[i] = data.control_group_indices[data.control_group_count - 1];
                data.control_group_count--;
                break;
            }
        }
    }

    // 2. ¸üĞÂ±à¶ÓÊı¾İ
    group_manager->control_groups[p_index] = p_unit_ids;

    // 3. ½¨Á¢ĞÂµ¥Î»µÄË«ÏòÓ³Éä
    for (int new_uid : p_unit_ids) {
        //Ö®ºóĞ´³Éº¯Êı
        int index = -1;
        auto it = id_to_index.find(new_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        if (data.control_group_count < 3) { // ¼ÙÉèÉÏÏŞ 3
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
        attack_manager->setup(this); // åˆå§‹ï¿½?AttackManager
    }
}

float UnitManager::get_unit_aggro_range(int p_unit_id) const {
    auto it = id_to_index.find(p_unit_id);
    if (it != id_to_index.end()) {
        // Ö±ï¿½Ó´Óµï¿½Î»ï¿½ï¿½ stats ï¿½Ğ¶ï¿½È¡ï¿½ï¿½ï¿½ØºÃµï¿½ï¿½ï¿½Êµï¿½ï¿½ï¿½ï¿½
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
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos", "team_id"), &UnitManager::spawn_unit_by_type);
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("command_units_to_patrol", "unit_ids", "waypoints"), &UnitManager::command_units_to_patrol);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &UnitManager::set_selection_manager);
    ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &UnitManager::set_group_manager);
    ClassDB::bind_method(D_METHOD("set_attack_manager", "node"), &UnitManager::set_attack_manager);
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
    ClassDB::bind_method(D_METHOD("get_unit_aggro_range", "unit_id"), &UnitManager::get_unit_aggro_range);
    ClassDB::bind_method(D_METHOD("get_unit_attack_range", "unit_id"), &UnitManager::get_unit_attack_range);
    

    //µ÷ÊÔ
    // 1. ÏÈ°ó¶¨ËùÓĞ·½·¨ (Getter/Setter)
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

    // 2. ×¢²áÊôĞÔµ½ Godot ÊôĞÔÃæ°å

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




