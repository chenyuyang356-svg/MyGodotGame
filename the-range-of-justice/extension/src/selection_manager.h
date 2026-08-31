#pragma once
#include <vector>
#include <unordered_set>
#include <godot_cpp/classes/node2d.hpp>
#include "unit_manager.h"
#include "building_manager.h"

namespace godot {

    class SelectionManager : public Node2D {
        GDCLASS(SelectionManager, Node2D)

    public:
        enum SelectionType { NONE, UNIT, BUILDING };

    private:
        SelectionType current_selection_type = NONE;
        std::unordered_set<int> selected_unit_ids;
        std::unordered_set<int> selected_building_ids;

        // 仅用于视觉高亮的“鼠标悬停”
        int hovered_unit_id = -1;
        int hovered_building_id = -1;
        int team_id = 1;

    public:
        // 执行选择逻辑：由 GDScript 在点击或框选结束时调用
        void do_single_select(Vector2 p_mouse_pos, Node* p_um, Node* p_bm, bool p_additive = false);
        void do_type_select(Vector2 p_mouse_pos, Rect2 p_screen_rect, Node* p_um, Node* p_bm);
        void do_box_select(Rect2 p_box, Node* p_um, Node* p_bm, bool p_additive = false);
        void clear_selection();

        // 程序化设置选中（编队选择/子组切换用），会校验队伍归属
        void set_selected_units(Array p_ids, Node* p_um);

        void update_hover(Vector2 p_mouse_pos, Node* p_um, Node* p_bm);

        void handle_right_click(Vector2 p_mouse_pos, Node* p_um, Node* p_bm);

        void request_unit_production(int p_building_id, String p_unit_type);

        void on_unit_despawned(int p_id);
        void on_building_removed(int p_id);

        // 供渲染器查询接口
        bool is_unit_selected(int p_id) const { return selected_unit_ids.count(p_id); }
        bool is_unit_hovered(int p_id) const { return hovered_unit_id == p_id; }
        bool is_building_selected(int p_id) const { return selected_building_ids.count(p_id); }
        bool is_building_hovered(int p_id) const { return hovered_building_id == p_id; }

        // 获取数据给 UI
        SelectionType get_selection_type() const { return current_selection_type; }
        PackedInt32Array get_selected_unit_ids() const;
        PackedInt32Array get_selected_building_ids() const;

        int get_team_id() { return team_id; }
        void set_team_id(int p_id) { team_id = p_id; }

    protected:
        static void _bind_methods();
    };
}