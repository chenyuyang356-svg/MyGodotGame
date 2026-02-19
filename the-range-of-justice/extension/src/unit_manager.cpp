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
    new_unit.stats = p_stats; // 存入引用

    new_unit.team_id = p_team_id; // 赋值阵营

    // 从资源中初始化实时数值
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
        // 1. 获取最后一个单位的数据
        UnitData& last_unit = units.back();

        // 2. 将最后一个单位移动到要删除的位置
        units[index_to_remove] = last_unit;

        // 3. 更新被移动单位在哈希表中的索引
        id_to_index[last_unit.id] = index_to_remove;
    }

    // 4. 删除 vector 最后一个元素，并从哈希表中移除目标 ID
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

        // 使用哈希表直接定位
        auto it = id_to_index.find(uid);
        if (it != id_to_index.end()) {
            UnitData& unit = units[it->second];
            unit.target_pos = p_target_world_pos;
            unit.target_grid = target_grid_pos;
            unit.state = MOVING;
        }
    }
}

void UnitManager::update_spatial_grid() {
    for (int i = 0; i < unit_grid_size; ++i) {
        unit_grid[i].clear();
    }
    for (int i = 0; i < units.size(); ++i) {
        Vector2i rel_pos = flow_field_manager->world_to_relative(units[i].position);

        // 缩放到单位网格（单位网格尺寸是流场的 2 倍）
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

    // 检查 3x3 范围内的格子
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
    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        float selection_radius = (unit.stats)->get_collision_radius();
        if (((selection_manager->mouse_position).distance_squared_to(unit.position) <
            (selection_radius) * (selection_radius)) &&
            (selection_manager->state != selection_manager->BOX_SELECTING)) {
            unit.is_mouse_on = true;            
        }
        else {
            unit.is_mouse_on = false;
        }
        if ((selection_manager->state == selection_manager->SINGLE_SELECTING) ||
            (selection_manager->state == selection_manager->TYPE_SELECTING)) {
            if (unit.is_mouse_on) {
                selection_manager->selected_unit_id = unit.id;
                selection_manager->selected_unit_stats = unit.stats;
            }
        }
    }

    int new_gid = -1;
    if (selection_manager->state == selection_manager->TYPE_SELECTING) {
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

    // 询问 AttackManager 是否接管此单位的物理
    if (attack_manager) {
        is_combat_controlled = attack_manager->try_get_combat_force(p_unit, force);
    }

    // 如果没有被战斗系统接管，则使用默认的流场移动逻辑
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

    // 计算预测位置
    Vector2 next_pos = p_unit.position + p_unit.velocity * p_delta;
    float radius = (p_unit.stats)->get_collision_radius();
    Vector2i cell_size = flow_field_manager->get_cell_size();

    // 处理单位间的“硬修正”（防止重叠）
    // IDLE单位不参与
    if (p_unit.state != IDLE) {
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

                // 关键点：只推开重叠部分的一小部分（例如 40%），防止产生剧烈抖动
                // 如果两个单位都在移动，各推一半
                float push_strength = 0.4f;
                Vector2 push_vector = resolve_dir * (overlap * push_strength);

                next_pos -= push_vector;
                // 也可以顺便给对方一个反向的推力，但为了逻辑简单，
                // 每一个单位在自己的 move 循环里处理被推开即可
            }
        }
    }

    // 处理墙体碰撞
    // 确定需要检查的网格范围 (单位周围的 2x2 或 3x3 区域)
    // MOVE_AIR的单位不参与

    if ((p_unit.stats)->get_move_type() == MOVE_AIR) {
        p_unit.position = next_pos;
        return;
    }

    Vector2i min_grid = flow_field_manager->world_to_grid(next_pos - Vector2(radius, radius));
    Vector2i max_grid = flow_field_manager->world_to_grid(next_pos + Vector2(radius, radius));

    // 遍历范围内的格子进行碰撞处理
    for (int gx = min_grid.x; gx <= max_grid.x; ++gx) {
        for (int gy = min_grid.y; gy <= max_grid.y; ++gy) {
            Vector2i check_grid(gx, gy);

            // 如果该格子是墙 (Cost == 255)
            if (flow_field_manager->get_cost(check_grid) == 255) {

                // 计算格子的世界坐标边界 (AABB)
                // 注意：这里假设网格左上角对齐逻辑与 world_to_grid 一致
                float rect_left = (float)gx * cell_size.x;
                float rect_top = (float)gy * cell_size.y;
                float rect_right = rect_left + cell_size.x;
                float rect_bottom = rect_top + cell_size.y;

                // 找到矩形内离圆心最近的点
                float closest_x = Math::clamp(next_pos.x, rect_left, rect_right);
                float closest_y = Math::clamp(next_pos.y, rect_top, rect_bottom);
                Vector2 closest_point(closest_x, closest_y);

                // 计算圆心到最近点的向量
                Vector2 diff = next_pos - closest_point;
                float distance_squared = diff.length_squared();

                // 如果距离小于半径，发生碰撞

                if (distance_squared < radius * radius && distance_squared > 0.00001f) {
                    float factor = 0.5;
                    if (p_unit.state == IDLE) {
                        factor = 1.0;
                    }

                    float distance = Math::sqrt(distance_squared);
                    float overlap = radius - distance;

                    // 将单位沿碰撞法线推开
                    next_pos += (diff / distance) * overlap * factor;

                    // 碰撞后通常需要削减该方向的速度，实现“沿墙滑动”
                    Vector2 normal = diff / distance;
                    if (p_unit.velocity.dot(normal) < 0) {
                        // 减去法线方向的速度分量
                        p_unit.velocity -= normal * p_unit.velocity.dot(normal);
                    }
                }
                // 处理圆心正好在墙内的情况
                else if (distance_squared <= 0.00001f) {
                    // 粗略处理：向格子中心的反方向推
                    Vector2 cell_center(rect_left + cell_size.x * 0.5f, rect_top + cell_size.y * 0.5f);
                    Vector2 push_dir = (next_pos - cell_center).normalized();
                    next_pos += push_dir * radius;
                }
            }
        }
    }

    // 4. 应用最终位置
    p_unit.position = next_pos;
}

void UnitManager::update_multimesh_buffer(double p_delta) {
    if (!multimesh_instance) return;

    Ref<MultiMesh> mesh_res = multimesh_instance->get_multimesh();
    if (mesh_res.is_null()) return;

    int current_unit_count = units.size();

    // 1. 如果单位数量变化，调整 MultiMesh 的实例数量
    if (mesh_res->get_instance_count() != current_unit_count) {
        mesh_res->set_instance_count(current_unit_count);
    }

    // 动画配置（可以做成成员变量）
    float fps = 10.0f;           // 每秒 10 帧
    int total_idle_frames = 2;   // 待机动画帧数
    int total_move_frames = 2;   // 移动动画帧数

    // 2. 遍历单位并更新变换矩阵
    for (int i = 0; i < current_unit_count; ++i) {
        UnitData& unit = units[i];

        // 创建变换矩阵：设置位置、旋转（可选）和缩放
        // 注意：如果你需要单位朝向移动方向，可以利用 unit.velocity.angle()
        Transform2D xform;

        // 如果单位正在移动，旋转它以指向移动方向
        if (unit.velocity.length_squared() > 0.1f) {
            float rotation_angle = unit.velocity.angle() + (Math_PI / 2.0f);
            xform.set_rotation(rotation_angle);
        }

        xform.set_origin(unit.position);

        // 将变换应用到第 i 个实例
        mesh_res->set_instance_transform_2d(i, xform);

        //计算动画帧
        int frame_index = 0;
        int row_index = 0;

        if (unit.state == MOVING) {
            frame_index = (int)(unit.anim_time * fps) % total_move_frames;
            row_index = 1; // 移动动画在第二行
        }
        else {
            frame_index = (int)(unit.anim_time * fps) % total_idle_frames;
            row_index = 0; // 待机动画在第一行
        }

        // 我们利用 Color 的四个通道传递：x: 帧索引, y: 行索引, z: 预留, w: 预留
        // 注意：在 Shader 中这对应 INSTANCE_CUSTOM
        mesh_res->set_instance_custom_data(i, Color(float(frame_index), float(row_index), 0, 0));

        //处理颜色
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
        mesh_res->set_instance_color(i, display_color);
        
        unit.anim_time += p_delta; // 更新动画计时器
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
            if (p_unit.stats == selection_manager->selected_unit_stats) {
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
        if ((selection_manager->selecting_box).has_point(p_unit.position)) {
            p_unit.is_selected = true;
        }
        else {
            p_unit.is_selected = false;
        }
        break;
    case (selection_manager->SELECTING_TARGET_POSITION):
        if (p_unit.is_selected) {
            Vector2i target_grid_pos = flow_field_manager->world_to_grid(selection_manager->mouse_position);
            if ((p_unit.stats)->get_move_type() != MOVE_AIR) {
                flow_field_manager->create_flow_field(target_grid_pos, false);
            }
            p_unit.target_pos = selection_manager->mouse_position;
            p_unit.target_grid = target_grid_pos;
            p_unit.state = MOVING;

            if (p_unit.temp_group_id != -1) {
                // 这个函数会：1.从旧组ID列表移除自己(O(1)) 2.如果之前在移动，减少旧组计数
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

void UnitManager::set_multimesh_instance(Node* p_node) {
    multimesh_instance = Object::cast_to<MultiMeshInstance2D>(p_node);
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
    // 调用 UnitLoader 加载 txt
    Ref<UnitStats> stats = UnitLoader::load_stats_from_txt(p_path);
    if (stats.is_valid()) {
        unit_types_cache[p_name] = stats;
    }
}

int UnitManager::spawn_unit_by_type(String p_type_name, Vector2 p_pos) {
    if (unit_types_cache.has(p_type_name)) {
        return spawn_unit(p_pos, unit_types_cache[p_type_name]);
    }
    return -1;
}

void UnitManager::set_control_group(int p_index, const std::vector<int>& p_unit_ids) {
    if (p_index < 0 || p_index >= group_manager->MAX_CONTROL_GROUPS) return;

    // 1. 清理旧单位的双向映射
    for (int old_uid : group_manager->control_groups[p_index]) {
        //之后写成函数
        int index = -1;
        auto it = id_to_index.find(old_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        // 从固定数组中移除索引
        for (int i = 0; i < data.control_group_count; ++i) {
            if (data.control_group_indices[i] == p_index) {
                data.control_group_indices[i] = data.control_group_indices[data.control_group_count - 1];
                data.control_group_count--;
                break;
            }
        }
    }

    // 2. 更新编队数据
    group_manager->control_groups[p_index] = p_unit_ids;

    // 3. 建立新单位的双向映射
    for (int new_uid : p_unit_ids) {
        //之后写成函数
        int index = -1;
        auto it = id_to_index.find(new_uid);
        if (it != id_to_index.end()) {
            index = it->second;
        }

        UnitData& data = units[index];
        if (data.control_group_count < 3) { // 假设上限 3
            data.control_group_indices[data.control_group_count++] = p_index;
        }
    }
}

void UnitManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type", "team_id"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("set_multimesh_instance", "node"), &UnitManager::set_multimesh_instance);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &UnitManager::set_selection_manager);
    ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &UnitManager::set_group_manager);
    ClassDB::bind_method(D_METHOD("register_unit_type", "name", "path"), &UnitManager::register_unit_type);
    ClassDB::bind_method(D_METHOD("spawn_unit_by_type", "type_name", "pos"), &UnitManager::spawn_unit_by_type);

    //调试
    // 1. 先绑定所有方法 (Getter/Setter)
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

    // 2. 注册属性到 Godot 属性面板

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
        attack_manager->setup(this); // 初始化 AttackManager
    }
}


