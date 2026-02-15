#include "unit_manager.h"

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

int UnitManager::spawn_unit(Vector2 p_world_pos, UnitType p_type, int team_id) {
    // 1. 创建一个新的单位数据结构
    UnitData new_unit;

    //调试
    new_unit.radius = unit_radius;
    new_unit.selection_radius = unit_selection_radius;
    new_unit.speed = unit_speed;

    // 2. 分配唯一 ID 并自增计数器
    new_unit.id = next_unit_id++;

    // 3. 初始化物理属性
    new_unit.position = p_world_pos;

    // 4. 初始化状态(待完善，根据单位类型应有不同的初始化)
    new_unit.velocity = Vector2(0, 0);
    new_unit.state = IDLE;
    new_unit.type = p_type;
    new_unit.target_grid = Vector2i(-1, -1); // 初始没有目标

    // 5. 存入 vector
    // 注意：如果单位非常多，建议在 UnitManager 构造函数里先调用 units.reserve(1000)
    units.push_back(new_unit);
    id_to_index[new_unit.id] = units.size() - 1;

    // 6. 返回 ID，以便 GDScript 记录并关联对应的 Sprite
    return new_unit.id;
}

void UnitManager::despawn_unit(int p_unit_id) {
    auto it = id_to_index.find(p_unit_id);
    if (it == id_to_index.end()) return;

    size_t index_to_remove = it->second;
    int last_unit_idx = units.size() - 1;

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

void UnitManager::_physics_process(double p_delta) {
    if (!is_setup || !flow_field_manager || !selection_manager) { return; }

    selection_manager->selected_unit_id = -1;
    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        if (((selection_manager->mouse_position).distance_squared_to(unit.position) <
            (unit.selection_radius) * (unit.selection_radius)) &&
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
                selection_manager->selected_type = (int)(unit.type);
            }
        }
    }

    update_spatial_grid();
    flow_field_manager->update(p_delta);

    for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
        UnitData& unit = units[unit_idx];
        update_state(unit);
        update_selection_state_and_target_position(unit);
        update_velocity(unit, p_delta);
        move(unit, p_delta);
    }
    if ((selection_manager->state == selection_manager->SINGLE_SELECTING) ||
        (selection_manager->state == selection_manager->TYPE_SELECTING) ||
        (selection_manager->state == selection_manager->BOX_SELECTION_ENDED) ||
        (selection_manager->state == selection_manager->SELECTING_TARGET_POSITION)) {
        selection_manager->state = selection_manager->NOT_SELECTING;
    }

    update_multimesh_buffer();
}

Vector2 UnitManager::get_flow(UnitData& p_unit) {
    Vector2 flow = flow_field_manager->get_flow_direction(p_unit.position, p_unit.target_pos);
    return flow;
}

Vector2 UnitManager::get_separation(UnitData& p_unit) {
    bool is_IDLE = (p_unit.state == IDLE);
    Vector2 separation = Vector2(0, 0);

    for (int unit_idx : get_nearby_units(p_unit.position, p_unit.radius * separation_radius_factor)) {
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
        if (flow_field_manager->get_integration(p_unit.position, p_unit.target_pos) <= desired_integration) {
            p_unit.state = IDLE;
            p_unit.velocity = Vector2(0, 0);
        }
        break;
    }
}

void UnitManager::update_velocity(UnitData& p_unit, double p_delta) {
    Vector2 force = get_force(p_unit);
    if (force.length_squared() < force_threshold_squared) {
        force = Vector2(0, 0);
    }
    
    switch (p_unit.state) {
    case IDLE:
        p_unit.velocity += force * p_delta;
        p_unit.velocity = (p_unit.velocity).limit_length(p_unit.speed);
        break;
    case MOVING:
        p_unit.velocity += force * p_delta;
        p_unit.velocity = (p_unit.velocity).limit_length(p_unit.speed);
        break;
    }

    if ((p_unit.velocity).length_squared() < velocity_threshold_squared) {
        p_unit.velocity= Vector2(0, 0);
    }
}

void UnitManager::move(UnitData& p_unit, double p_delta) {
    p_unit.position += p_unit.velocity * p_delta;
    if (p_unit.id == 1) {
        UtilityFunctions::print(p_unit.velocity);
    }
}

void UnitManager::update_multimesh_buffer() {
    if (!multimesh_instance) return;

    Ref<MultiMesh> mesh_res = multimesh_instance->get_multimesh();
    if (mesh_res.is_null()) return;

    int current_unit_count = units.size();

    // 1. 如果单位数量变化，调整 MultiMesh 的实例数量
    if (mesh_res->get_instance_count() != current_unit_count) {
        mesh_res->set_instance_count(current_unit_count);
    }

    // 2. 遍历单位并更新变换矩阵
    for (int i = 0; i < current_unit_count; ++i) {
        const UnitData& unit = units[i];

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

        if (unit.is_selected) {
            if (unit.is_mouse_on) {
                mesh_res->set_instance_color(i, Color(1.5, 1.5, 1.5));
            }
            else {
                mesh_res->set_instance_color(i, Color(1.2, 1.2, 1.2));
            }
        }
        else {
            if (unit.is_mouse_on) {
                mesh_res->set_instance_color(i, Color(1.5, 1.5, 1.5));
            }
            else {
                mesh_res->set_instance_color(i, Color(1.0, 1.0, 1.0));
            }
        }
        //mesh_res->set_instance_color(i, Color(1, 0, 0)); 
    }
}

void UnitManager::update_selection_state_and_target_position(UnitData& p_unit) {
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
            if (selection_manager->selected_type == (int)(p_unit.type)) {
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

void UnitManager::set_multimesh_instance(Node* p_node) {
    multimesh_instance = Object::cast_to<MultiMeshInstance2D>(p_node);
}

void UnitManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void UnitManager::set_selection_manager(Node* p_node) {
    selection_manager = Object::cast_to<SelectionManager>(p_node);
}

void UnitManager::_bind_methods() {
    BIND_ENUM_CONSTANT(IDLE);
    BIND_ENUM_CONSTANT(MOVING);

    BIND_ENUM_CONSTANT(SQUARE);

    ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &UnitManager::setup_system);
    ClassDB::bind_method(D_METHOD("spawn_unit", "world_position", "type"), &UnitManager::spawn_unit);
    ClassDB::bind_method(D_METHOD("command_units_to_move", "unit_ids", "target_world_pos"), &UnitManager::command_units_to_move);
    ClassDB::bind_method(D_METHOD("get_unit_position", "unit_id"), &UnitManager::get_unit_position);
    ClassDB::bind_method(D_METHOD("get_unit_state", "unit_id"), &UnitManager::get_unit_state);
    ClassDB::bind_method(D_METHOD("set_multimesh_instance", "node"), &UnitManager::set_multimesh_instance);
    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &UnitManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &UnitManager::set_selection_manager);

    //调试
    // 1. 先绑定所有方法 (Getter/Setter)
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

    ClassDB::bind_method(D_METHOD("get_friction_factor"), &UnitManager::get_friction_factor);
    ClassDB::bind_method(D_METHOD("set_friction_factor", "p_val"), &UnitManager::set_friction_factor);

    ClassDB::bind_method(D_METHOD("get_force_threshold_squared"), &UnitManager::get_force_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_force_threshold_squared", "p_val"), &UnitManager::set_force_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_velocity_threshold_squared"), &UnitManager::get_velocity_threshold_squared);
    ClassDB::bind_method(D_METHOD("set_velocity_threshold_squared", "p_val"), &UnitManager::set_velocity_threshold_squared);

    ClassDB::bind_method(D_METHOD("get_desired_integration"), &UnitManager::get_desired_integration);
    ClassDB::bind_method(D_METHOD("set_desired_integration", "p_val"), &UnitManager::set_desired_integration);

    // 2. 注册属性到 Godot 属性面板

    ADD_GROUP("Unit Defaults", "unit_");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_speed"), "set_unit_speed", "get_unit_speed");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_radius"), "set_unit_radius", "get_unit_radius");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_selection_radius"), "set_unit_selection_radius", "get_unit_selection_radius");

    ADD_GROUP("Force Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flow_factor"), "set_flow_factor", "get_flow_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_factor"), "set_separation_factor", "get_separation_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_limit"), "set_separation_limit", "get_separation_limit");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_radius_factor"), "set_separation_radius_factor", "get_separation_radius_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction_factor"), "set_friction_factor", "get_friction_factor");

    ADD_GROUP("Threshold Settings", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "force_threshold_squared"), "set_force_threshold_squared", "get_force_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "velocity_threshold_squared"), "set_velocity_threshold_squared", "get_velocity_threshold_squared");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "desired_integration"), "set_desired_integration", "get_desired_integration");
}
