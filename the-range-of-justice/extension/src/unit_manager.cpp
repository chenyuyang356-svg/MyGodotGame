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
    new_unit.stats = p_stats; // 瀛樺叆寮曠敤

    new_unit.team_id = p_team_id; // 璧嬪€奸樀钀�

    // 浠庤祫婧愪腑鍒濆鍖栧疄鏃舵暟鍊�
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

    if (index_to_remove != last_unit_idx) {
        // 1. 鑾峰彇鏈€鍚庝竴涓崟浣嶇殑鏁版嵁
        UnitData& last_unit = units.back();

        // 2. 灏嗘渶鍚庝竴涓崟浣嶇Щ鍔ㄥ埌瑕佸垹闄ょ殑浣嶇疆
        units[index_to_remove] = last_unit;

        // 3. 鏇存柊琚Щ鍔ㄥ崟浣嶅湪鍝堝笇琛ㄤ腑鐨勭储寮�
        id_to_index[last_unit.id] = index_to_remove;
    }

    // 4. 鍒犻櫎 vector 鏈€鍚庝竴涓厓绱狅紝骞朵粠鍝堝笇琛ㄤ腑绉婚櫎鐩爣 ID
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
            unit.is_patrolling = false; // 琚己鍒剁Щ鍔ㄦ椂鎵撴柇宸￠€�
            unit.target_pos = p_target_world_pos;
            unit.target_grid = target_grid_pos;
            unit.state = MOVING;
            unit.target_id = -1; // 鏀惧純褰撳墠鐩爣
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

        // 缂╂斁鍒板崟浣嶇綉鏍硷紙鍗曚綅缃戞牸灏哄鏄祦鍦虹殑 2 鍊嶏級
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

    // 妫€鏌� 3x3 鑼冨洿鍐呯殑鏍煎瓙
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
        update_selection_state_and_target_position(unit);

        if (unit.state == MOVING) {
            if (flow_field_manager->get_integration(unit.position, unit.target_pos) <= desired_integration) {
                unit.state = IDLE;
                unit.velocity = Vector2(0, 0);
            }
        }
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
    Vector2 flow = flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos);
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
            float desired_distance = (float)((desired_integration + 1) * (flow_field_manager->get_cell_size()).x );
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

    // 璇㈤棶 AttackManager 鏄惁鎺ョ姝ゅ崟浣嶇殑鐗╃悊
    if (attack_manager) {
        is_combat_controlled = attack_manager->try_get_combat_force(p_unit, force);
    }

    // 濡傛灉娌℃湁琚垬鏂楃郴缁熸帴绠★紝鍒欎娇鐢ㄩ粯璁ょ殑娴佸満绉诲姩閫昏緫
    if (!is_combat_controlled) {
        force = get_force(p_unit);
    }
    if (force.length_squared() < force_threshold_squared) {
        force = Vector2(0, 0);
    }
    
    float max_speed = (p_unit.stats)->get_move_speed();
    switch (p_unit.state) {
    case IDLE:
        p_unit.velocity += force * p_delta;
        p_unit.velocity = (p_unit.velocity).limit_length(max_speed);
        break;
    case MOVING:
        p_unit.velocity += force * p_delta;
        p_unit.velocity = (p_unit.velocity).limit_length(max_speed);
        break;
    }

    if ((p_unit.velocity).length_squared() < velocity_threshold_squared) {
        p_unit.velocity= Vector2(0, 0);
    }
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    if (!flow_field_manager) return;

    // 璁＄畻棰勬祴浣嶇疆
    Vector2 next_pos = p_unit.position + p_unit.velocity * p_delta;
    float radius = (p_unit.stats)->get_collision_radius();
    Vector2i cell_size = flow_field_manager->get_cell_size();

    // 澶勭悊鍗曚綅闂寸殑鈥滅‖淇鈥濓紙闃叉閲嶅彔锛�
<<<<<<< Updated upstream
    // IDLE涓斿浜巘emp_group涓殑鍗曚綅涓嶅弬涓�
    if (p_unit.state != IDLE || p_unit.temp_group_id == -1) {
        std::vector<int> nearby = get_nearby_units(next_pos, radius * 2.0f);
        for (int other_idx : nearby) {
            UnitData& other = units[other_idx];
            if (other.id == p_unit.id) continue;
            if (other.state == IDLE) continue;

        Vector2 to_other = other.position - next_pos;
        float dist_sq = to_other.length_squared();
        float min_dist = radius + (other.stats)->get_collision_radius();

        if (dist_sq < min_dist * min_dist && dist_sq > 0.001f) {
            float dist = Math::sqrt(dist_sq);
            float overlap = min_dist - dist;
            Vector2 resolve_dir = to_other / dist;

            // 鍏抽敭鐐癸細鍙帹寮€閲嶅彔閮ㄥ垎鐨勪竴灏忛儴鍒嗭紙渚嬪 40%锛夛紝闃叉浜х敓鍓х儓鎶栧姩
            // 濡傛灉涓や釜鍗曚綅閮藉湪绉诲姩锛屽悇鎺ㄤ竴鍗�
            float push_strength = 0.4f;
            Vector2 push_vector = resolve_dir * (overlap * push_strength);

            next_pos -= push_vector;
            // 涔熷彲浠ラ『渚跨粰瀵规柟涓€涓弽鍚戠殑鎺ㄥ姏锛屼絾涓轰簡閫昏緫绠€鍗曪紝
            // 姣忎竴涓崟浣嶅湪鑷繁鐨� move 寰幆閲屽鐞嗚鎺ㄥ紑鍗冲彲
        }
    }

    // 澶勭悊澧欎綋纰版挒
    // 纭畾闇€瑕佹鏌ョ殑缃戞牸鑼冨洿 (鍗曚綅鍛ㄥ洿鐨� 2x2 鎴� 3x3 鍖哄煙)
    Vector2i min_grid = flow_field_manager->world_to_grid(next_pos - Vector2(radius, radius));
    Vector2i max_grid = flow_field_manager->world_to_grid(next_pos + Vector2(radius, radius));

    // 閬嶅巻鑼冨洿鍐呯殑鏍煎瓙杩涜纰版挒澶勭悊
    for (int gx = min_grid.x; gx <= max_grid.x; ++gx) {
        for (int gy = min_grid.y; gy <= max_grid.y; ++gy) {
            Vector2i check_grid(gx, gy);

            // 濡傛灉璇ユ牸瀛愭槸澧� (Cost == 255)
            if (flow_field_manager->get_cost(check_grid) == 255) {

                // 璁＄畻鏍煎瓙鐨勪笘鐣屽潗鏍囪竟鐣� (AABB)
                // 娉ㄦ剰锛氳繖閲屽亣璁剧綉鏍煎乏涓婅瀵归綈閫昏緫涓� world_to_grid 涓€鑷�
                float rect_left = (float)gx * cell_size.x;
                float rect_top = (float)gy * cell_size.y;
                float rect_right = rect_left + cell_size.x;
                float rect_bottom = rect_top + cell_size.y;

                // 鎵惧埌鐭╁舰鍐呯鍦嗗績鏈€杩戠殑鐐�
                float closest_x = Math::clamp(next_pos.x, rect_left, rect_right);
                float closest_y = Math::clamp(next_pos.y, rect_top, rect_bottom);
                Vector2 closest_point(closest_x, closest_y);

                // 璁＄畻鍦嗗績鍒版渶杩戠偣鐨勫悜閲�
                Vector2 diff = next_pos - closest_point;
                float distance_squared = diff.length_squared();

                // 濡傛灉璺濈灏忎簬鍗婂緞锛屽彂鐢熺鎾�

                if (distance_squared < radius * radius && distance_squared > 0.00001f) {
                    float factor = 0.5;
                    if (p_unit.state == IDLE) {
                        factor = 1.0;
                    }

                    float distance = Math::sqrt(distance_squared);
                    float overlap = radius - distance;

                    // 灏嗗崟浣嶆部纰版挒娉曠嚎鎺ㄥ紑
                    next_pos += (diff / distance) * overlap * factor;

                    // 纰版挒鍚庨€氬父闇€瑕佸墛鍑忚鏂瑰悜鐨勯€熷害锛屽疄鐜扳€滄部澧欐粦鍔ㄢ€�
                    Vector2 normal = diff / distance;
                    if (p_unit.velocity.dot(normal) < 0) {
                        // 鍑忓幓娉曠嚎鏂瑰悜鐨勯€熷害鍒嗛噺
                        p_unit.velocity -= normal * p_unit.velocity.dot(normal);
                    }
                }
                // 澶勭悊鍦嗗績姝ｅソ鍦ㄥ鍐呯殑鎯呭喌
                else if (distance_squared <= 0.00001f) {
                    // 绮楃暐澶勭悊锛氬悜鏍煎瓙涓績鐨勫弽鏂瑰悜鎺�
                    Vector2 cell_center(rect_left + cell_size.x * 0.5f, rect_top + cell_size.y * 0.5f);
                    Vector2 push_dir = (next_pos - cell_center).normalized();
                    next_pos += push_dir * radius;
                }
            }
        }
    }

    // 4. 搴旂敤鏈€缁堜綅缃�
    p_unit.position = next_pos;
}

void UnitManager::update_multimesh_buffer(double p_delta) {
    if (type_renderers.empty()) return;

    // 1. 娓呯┖涓婁竴甯х殑鍒嗙粍
    for (auto& pair : type_grouping_cache) {
        pair.second.clear();
    }

    // 2. 鎸� UnitStats 鎸囬拡鍒嗙粍
    for (int i = 0; i < units.size(); ++i) {
        // 鐩存帴鍙� UnitStats 鐨勫師濮嬫寚閽�
        UnitStats* s_ptr = units[i].stats.ptr();
        type_grouping_cache[s_ptr].push_back(i);
    }

    // 3. 閬嶅巻娓叉煋鍣ㄥ苟濉厖鏁版嵁
    for (auto const& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& indices = type_grouping_cache[s_ptr];
        int count = indices.size();

        if (count == 0) continue;

        Ref<MultiMesh> mm = mmi->get_multimesh();
        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
        }

        // 鑾峰彇褰卞瓙娓叉煋鍣�
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();
        s_mm->set_instance_count(count);

        // 杩欓噷 s_ptr 灏辨槸 UnitStats 鎸囬拡锛屽彲浠ョ洿鎺ヨ闂厤缃暟鎹�
        for (int i = 0; i < count; ++i) {
            int u_idx = indices[i];
            UnitData& unit = units[u_idx];

            // 鏇存柊鍙樻崲
            Transform3D xform;

            // 鍧愭爣鏄犲皠锛�
            // 2D X -> 3D X
            // 2D Y -> 3D Z (娣卞害锛岀敤浜� GPU 鑷姩 Y-Sort)
            // 2D Height -> 3D Y (瑙嗚楂樺害)
            float fake_depth_offset = unit.position.y * 0.0001f;
            Vector3 pos_3d = Vector3(unit.position.x, unit.height + fake_depth_offset, unit.position.y - unit.height);
            xform.origin = pos_3d;

            // 鏃嬭浆锛氳 QuadMesh 绔嬭捣鏉ワ紙QuadMesh 榛樿鍦� XY 骞抽潰锛屾垜浠渶瑕佸畠绔嬪湪 XZ 骞抽潰涓婏級
            // 濡傛灉鎽勫儚鏈烘槸淇鐨勶紝鎴戜滑闇€瑕佺粫 X 杞存棆杞� -90 搴�
            xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (unit.rotation + Math_PI / 2.0f));

            mm->set_instance_transform(i, xform);

            // 鍔ㄧ敾閫昏緫锛堢洿鎺ヤ娇鐢� s_ptr锛�
            int frames = (unit.state == MOVING) ? s_ptr->get_move_frames() : s_ptr->get_idle_frames();
            int row = (unit.state == MOVING) ? s_ptr->get_move_row() : s_ptr->get_idle_row();
            float duration = (float)frames / s_ptr->get_anim_fps();
            int frame_idx = (int)(Math::fmod(unit.anim_time, duration) * s_ptr->get_anim_fps());

            mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            //澶勭悊棰滆壊
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


            // 鏇存柊褰卞瓙鍙樻崲 (XZ骞抽潰韬哄钩)
            Transform3D shadow_xform;

            // 璁剧疆褰卞瓙浣嶇疆锛氱◢寰亸绉讳竴鐐圭偣锛岃瀹冧粠鍧﹀厠灞ュ甫涓績寰€澶栭潬涓€鐐�
            // 鍋囪鍏夋潵鑷乏涓婃柟锛屾垜浠皢褰卞瓙鍚戝彸涓嬭鍋忕Щ
            float shadow_offset_x = 4.0f; // 鏍规嵁浣犵殑鍧﹀厠灏哄璋冩暣
            float shadow_offset_z = 4.0f;

            // 褰卞瓙鏀惧湪鍦伴潰楂樺害锛岀粰涓€涓瀬灏忕殑鍋忕Щ (0.001) 闃叉涓庡湴闈� Z-Fighting
            // 娉ㄦ剰锛氬奖瀛愮殑 origin.y 涓嶉殢 unit.height 鍙樺寲锛屽畠姘歌繙鍦ㄥ湴涓�
            shadow_xform.origin = Vector3(unit.position.x + shadow_offset_x,
                unit.height + fake_depth_offset - 0.1f,
                unit.position.y + shadow_offset_z);

            // 褰卞瓙鏄汉鐫€鐨�
            shadow_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

            shadow_xform.basis = (shadow_xform.basis).rotated(Vector3(0, -1, 0), (unit.rotation + Math_PI / 2.0f));

            // 濡傛灉浣犲笇鏈涘奖瀛愭湁鏂滃悜鎷変几鎰燂紝鍙互鍦ㄨ繖閲屽彔鍔犵缉鏀�
            // shadow_xform.basis = shadow_xform.basis.scaled(Vector3(1.0, 1.5, 1.0));

            s_mm->set_instance_transform(i, shadow_xform);

            // 鍚屾鍔ㄧ敾鏁版嵁锛堝奖瀛愪篃瑕佸姩锛�
            s_mm->set_instance_custom_data(i, Color(frame_idx, row, 0, 0));

            unit.anim_time += p_delta;
        }
    }
}

void UnitManager::update_selection_state_and_target_position(UnitData& p_unit, int p_group_id) {
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
            flow_field_manager->create_flow_field(target_grid_pos, false);
            p_unit.target_pos = selection_manager->mouse_position;
            p_unit.target_grid = target_grid_pos;
            p_unit.state = MOVING;
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

void UnitManager::register_unit_type(String p_name, String p_path) {
    Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(p_path);
    if (stats.is_null()) return;

    // 瀛樺叆缂撳瓨渚� spawn_unit_by_type 浣跨敤
    unit_types_cache[p_name] = stats;

    // 鑾峰彇鍘熷鎸囬拡浣滀负 Key
    UnitStats* stats_ptr = stats.ptr();

    // 濡傛灉璇ョ被鍨嬪凡缁忔敞鍐岃繃娓叉煋鍣紝鐩存帴杩斿洖
    if (type_renderers.find(stats_ptr) != type_renderers.end()) return;

    // --- 鍒涘缓娓叉煋鍣� ---
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_name + "_Renderer");
    add_child(mmi);

    // 閰嶇疆 MultiMesh
    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();

    // 鍔犺浇绾圭悊骞惰缃綉鏍煎ぇ灏�
    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        qmesh->set_size(frame_size);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    // 璁剧疆鏉愯川
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

    // 寤虹珛鏄犲皠
    type_renderers[stats_ptr] = mmi;

    // --- 鍒涘缓褰卞瓙娓叉煋鍣� ---
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadows");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true);

    // 褰卞瓙鍙互浣跨敤鏇寸畝鍗曠殑 QuadMesh
    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(stats->get_h_frames(), stats->get_v_frames());
        s_qmesh->set_size(frame_size);
    }
    // ... 璁剧疆缃戞牸澶у皬 ...
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    // 璁剧疆褰卞瓙鏉愯川
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

    // 瀛樺叆 Map
    shadow_renderers[stats_ptr] = s_mmi;
}

<<<<<<< Updated upstream
int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos, int p_team_id) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name], p_team_id);
=======
int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos,int team_id) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name], team_id);
>>>>>>> Stashed changes
    }
    return -1;
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
        attack_manager->setup(this); // 鍒濆鍖� AttackManager
    }
}

void UnitManager::_bind_methods() {
    BIND_ENUM_CONSTANT(IDLE);
    BIND_ENUM_CONSTANT(MOVING);

    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type", "team_id"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos", "team_id"), &UnitManager::spawn_unit_by_type);
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("command_units_to_patrol", "unit_ids", "waypoints"), &UnitManager::command_units_to_patrol);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &UnitManager::set_selection_manager);
<<<<<<< Updated upstream
    ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &UnitManager::set_group_manager);
    ClassDB::bind_method(D_METHOD("set_attack_manager", "node"), &UnitManager::set_attack_manager);
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
   
    
=======
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos", "team_id"), &UnitManager::spawn_unit_by_type);
>>>>>>> Stashed changes

    //璋冭瘯
    // 1. 鍏堢粦瀹氭墍鏈夋柟娉� (Getter/Setter)
    ClassDB::bind_method(D_METHOD("get_unit_speed"), &UnitManager::get_unit_speed);
    ClassDB::bind_method(D_METHOD("set_unit_speed", "p_val"), &UnitManager::set_unit_speed);

    ClassDB::bind_method(D_METHOD("get_unit_radius"), &UnitManager::get_unit_radius);
    ClassDB::bind_method(D_METHOD("set_unit_radius", "p_val"), &UnitManager::set_unit_radius);

    ClassDB::bind_method(D_METHOD("get_unit_selection_radius"), &UnitManager::get_unit_selection_radius);
    ClassDB::bind_method(D_METHOD("set_unit_selection_radius", "p_val"), &UnitManager::set_unit_selection_radius);

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

    // 2. 娉ㄥ唽灞炴€у埌 Godot 灞炴€ч潰鏉�

    ADD_GROUP("Unit Defaults", "unit_");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_speed"), "set_unit_speed", "get_unit_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_radius"), "set_unit_radius", "get_unit_radius");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_selection_radius"), "set_unit_selection_radius", "get_unit_selection_radius");

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




