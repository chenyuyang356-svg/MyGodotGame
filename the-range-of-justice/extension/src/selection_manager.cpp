#include "selection_manager.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void SelectionManager::do_single_select(Vector2 p_mouse_pos, Node* p_um, Node* p_bm) {
    UnitManager* um = Object::cast_to<UnitManager>(p_um);
    BuildingManager* bm = Object::cast_to<BuildingManager>(p_bm);

    // 1. 优先检查单位 (由于单位小且移动，通常优先级更高)
    int u_id = um->get_unit_at_position(p_mouse_pos);
    if (u_id != -1 && um->get_unit_team_id(u_id) == team_id) {
        clear_selection();
        selected_unit_ids.insert(u_id);
        current_selection_type = UNIT;
        emit_signal("selection_changed");
        return;
    }

    // 2. 检查建筑
    // 假设 FlowFieldManager 的 world_to_grid 在内部处理
    int b_id = bm->get_building_at_position(p_mouse_pos);
    if (b_id != -1 && bm->get_building_team_id(b_id) == team_id) {
        clear_selection();
        selected_building_ids.insert(b_id);
        current_selection_type = BUILDING;
    }

    emit_signal("selection_changed");
}

void godot::SelectionManager::do_type_select(Vector2 p_mouse_pos, Rect2 p_screen_rect, Node* p_um, Node* p_bm){
    UnitManager* um = Object::cast_to<UnitManager>(p_um);
    BuildingManager* bm = Object::cast_to<BuildingManager>(p_bm);

    // 检查单位
    int target_uid = um->get_unit_at_position(p_mouse_pos);
    if (target_uid != -1 && um->get_unit_team_id(target_uid) == team_id) {
        clear_selection();

        Ref<UnitStats> target_stats = um->get_unit_stats(target_uid);
        std::vector<int> uids = um->get_units_of_type_in_area(target_stats, p_screen_rect, team_id);

        for (int uid : uids) {
            selected_unit_ids.insert(uid);
        }
        current_selection_type = UNIT;
        emit_signal("selection_changed");
        return;
    }

    // 检查建筑
    int target_bid = bm->get_building_at_position(p_mouse_pos);
    if (target_bid != -1) {
        clear_selection();

        Ref<BuildingStats> target_stats = bm->get_building_stats(target_bid);
        std::vector<int> bids = bm->get_buildings_of_type_in_area(target_stats, p_screen_rect, team_id);

        for (int bid : bids) {
            selected_building_ids.insert(bid);
        }
        current_selection_type = BUILDING;
    }

    emit_signal("selection_changed");
}

void SelectionManager::do_box_select(Rect2 p_box, Node* p_um, Node* p_bm) {
    UnitManager* um = Object::cast_to<UnitManager>(p_um);
    BuildingManager* bm = Object::cast_to<BuildingManager>(p_bm);
    clear_selection();

    //检查单位
    std::vector<int> uids = um->get_units_in_box(p_box, team_id);

    for (int uid : uids) {
        // 框选通常只选自己的单位，这里可以加判断
        selected_unit_ids.insert(uid);
    }

    if (!selected_unit_ids.empty()) {
        current_selection_type = UNIT;
        emit_signal("selection_changed");
        return;
    }

    //检查建筑
    std::vector<int> bids = bm->get_buildings_in_box(p_box, team_id);

    for (int bid : bids) {
        selected_building_ids.insert(bid);
    }

    if (!selected_building_ids.empty()) {
        current_selection_type = BUILDING;
        
    }

    emit_signal("selection_changed");
}

void SelectionManager::clear_selection() {
    // 1. 重置选择类型为 NONE
    current_selection_type = NONE;

    // 2. 清空存储单位 ID 的集合 (std::unordered_set)
    selected_unit_ids.clear();

    // 3. 清空存储建筑 ID 的集合
    selected_building_ids.clear();

    // 4. (可选) 重置悬停状态
    // 如果你希望在清除选择时，也清除鼠标当前的悬停高亮
    hovered_unit_id = -1;
    hovered_building_id = -1;
}

void SelectionManager::update_hover(Vector2 p_mouse_pos, Node* p_um, Node* p_bm) {
    UnitManager* um = Object::cast_to<UnitManager>(p_um);
    BuildingManager* bm = Object::cast_to<BuildingManager>(p_bm);

    // 1. 重置当前悬停状态
    int last_u_hover = hovered_unit_id;
    int last_b_hover = hovered_building_id;
    hovered_unit_id = -1;
    hovered_building_id = -1;

    // 2. 探测单位
    // 注意：这里调用 get_unit_at_position，它内部使用了空间网格，所以每帧调用开销很小
    hovered_unit_id = um->get_unit_at_position(p_mouse_pos);

    // 3. 如果没指着单位，探测建筑
    if (hovered_unit_id == -1) {
        hovered_building_id = bm->get_building_at_position(p_mouse_pos);
    }

    // 4. (可选) 如果悬停目标变了，可以发信号播放轻微的 UI 音效
    if (hovered_unit_id != last_u_hover || hovered_building_id != last_b_hover) {
        // emit_signal("hover_changed");
    }
}

void SelectionManager::handle_right_click(Vector2 p_mouse_pos, Node* p_um, Node* p_bm) {
    if (selected_unit_ids.empty()) return;

    UnitManager* um = Object::cast_to<UnitManager>(p_um);
    BuildingManager* bm = Object::cast_to<BuildingManager>(p_bm);
    if (!um || !bm) return;

    PackedInt32Array ids = get_selected_unit_ids();

    // 1. 探测单位 (优先级最高)
    int target_u_id = um->get_unit_at_position(p_mouse_pos);
    if (target_u_id != -1) {
        if (um->get_unit_team_id(target_u_id) != team_id) {
            // 发出攻击单位信号
            emit_signal("attack_unit_requested", ids, target_u_id);
        }
        return;
    }

    // 2. 探测建筑
    int target_b_id = bm->get_building_at_position(p_mouse_pos);
    if (target_b_id != -1) {
        // 假设 BuildingManager 有 get_building_team_id
        if (bm->get_building_team_id(target_b_id) != team_id) {
            // 发出攻击建筑信号
            emit_signal("attack_building_requested", ids, target_b_id);
        }
        return;
    }

    // 3. 点中空地
    emit_signal("move_requested", ids, p_mouse_pos);
}

void SelectionManager::request_unit_production(int p_building_id, String p_unit_type) {
    // 简单的本地校验：确保建筑确实在选中列表中（防止非法 UI 操作）
    if (selected_building_ids.count(p_building_id)) {
        emit_signal("unit_production_requested", p_building_id, p_unit_type);
    }
}

void SelectionManager::on_unit_despawned(int p_id) {
    // 如果该单位在选中列表中，移除它
    if (selected_unit_ids.erase(p_id) > 0) {
        // 如果移除后集合空了，且当前选择类型是单位，重置状态
        if (selected_unit_ids.empty() && current_selection_type == UNIT) {
            current_selection_type = NONE;
        }
        // 这里可以 emit_signal("selection_changed") 通知 UI 更新
    }

    // 如果该单位正处于悬停状态，重置悬停
    if (hovered_unit_id == p_id) {
        hovered_unit_id = -1;
    }
}

void SelectionManager::on_building_removed(int p_id) {
    if (selected_building_ids.erase(p_id) > 0) {
        if (selected_building_ids.empty() && current_selection_type == BUILDING) {
            current_selection_type = NONE;
        }
    }

    if (hovered_building_id == p_id) {
        hovered_building_id = -1;
    }
}

PackedInt32Array SelectionManager::get_selected_unit_ids() const {
    PackedInt32Array result;
    // 将 unordered_set 转换为 Godot 能够理解的数组
    for (int id : selected_unit_ids) {
        result.append(id);
    }
    return result;
}

PackedInt32Array SelectionManager::get_selected_building_ids() const {
    PackedInt32Array result;
    // 将 unordered_set 转换为 Godot 能够理解的数组
    for (int id : selected_building_ids) {
        result.append(id);
    }
    return result;
}

void SelectionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update_hover", "mouse_position", "unit_manager", "building_manager"), &SelectionManager::update_hover);
    ClassDB::bind_method(D_METHOD("handle_right_click", "mouse_position", "unit_manager", "building_manager"), &SelectionManager::handle_right_click);
	ClassDB::bind_method(D_METHOD("do_single_select", "mouse_position", "unit_manager", "building_manager"), &SelectionManager::do_single_select);
	ClassDB::bind_method(D_METHOD("do_type_select", "mouse_position", "screen_rect", "unit_manager", "building_manager"), &SelectionManager::do_type_select);
	ClassDB::bind_method(D_METHOD("do_box_select", "box", "unit_manager", "building_manager"), &SelectionManager::do_box_select);

    ClassDB::bind_method(D_METHOD("request_unit_production", "building_id", "unit_type"), &SelectionManager::request_unit_production);

    ClassDB::bind_method(D_METHOD("get_selected_building_ids"), &SelectionManager::get_selected_building_ids);

	ClassDB::bind_method(D_METHOD("get_team_id"), &SelectionManager::get_team_id);
	ClassDB::bind_method(D_METHOD("set_team_id", "p_id"), &SelectionManager::set_team_id);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "team_id"), "set_team_id", "get_team_id");

    ADD_SIGNAL(MethodInfo("move_requested", PropertyInfo(Variant::PACKED_INT32_ARRAY, "unit_ids"), PropertyInfo(Variant::VECTOR2, "target_pos")));
    ADD_SIGNAL(MethodInfo("attack_unit_requested", PropertyInfo(Variant::PACKED_INT32_ARRAY, "unit_ids"), PropertyInfo(Variant::INT, "target_id")));
    ADD_SIGNAL(MethodInfo("attack_building_requested", PropertyInfo(Variant::PACKED_INT32_ARRAY, "unit_ids"), PropertyInfo(Variant::INT, "target_id")));
    ADD_SIGNAL(MethodInfo("unit_production_requested", PropertyInfo(Variant::INT, "building_id"), PropertyInfo(Variant::STRING, "unit_type")));
    
    ADD_SIGNAL(MethodInfo("selection_changed"));
}