#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/rect2i.hpp>
#include <unordered_map>
#include <vector>

#include "unit_manager.h"
#include "flow_field_manager.h"

namespace godot {

    struct BuildingData {
        int id;
        Vector2i grid_pos;   // 网格左上角坐标
        Vector2i size;       // 占地格子数 (例如 3x3)
        int type;
        // 可以在这里添加生命值、建造进度等
    };

    class BuildingManager : public Node2D {
        GDCLASS(BuildingManager, Node2D)

    private:
        FlowFieldManager* flow_field_manager = nullptr;
        UnitManager* unit_manager = nullptr;

        std::unordered_map<int, BuildingData> buildings;
        int next_building_id = 0;

    protected:
        static void _bind_methods();

    public:
        BuildingManager();
        ~BuildingManager();

        // 设置引用
        void set_flow_field_manager(Node* p_node);
        void set_unit_manager(Node* p_node);

        // --- 核心功能 ---

        // 检查某个区域是否可以放置建筑
        bool is_area_clear(Vector2i p_grid_pos, Vector2i p_size);

        // 放置建筑：返回建筑 ID，失败返回 -1
        int place_building(Vector2i p_grid_pos, Vector2i p_size, int p_type);

        // 移除建筑
        void remove_building(int p_building_id);

        // 根据 ID 获取建筑数据
        Vector2i get_building_grid_pos(int p_building_id) const;
    };
}