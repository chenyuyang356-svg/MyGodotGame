#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>

#include "unit_manager.h"
#include "flow_field_manager.h"
#include "building_stats.h"
#include "building_loader.h"
#include "building_data.h"

namespace godot {

    class BuildingManager : public Node3D {
        GDCLASS(BuildingManager, Node3D)

    private:
        FlowFieldManager* flow_field_manager = nullptr;
        UnitManager* unit_manager = nullptr;

        std::unordered_map<int, BuildingData> buildings;
        HashMap<String, Ref<BuildingStats>> building_types_cache;
        int next_building_id = 0;

        // --- 原有的渲染器 ---
        std::unordered_map<BuildingStats*, MultiMeshInstance3D*> type_renderers;
        Ref<Shader> building_shader;

        // --- 新增：影子渲染器 ---
        std::unordered_map<BuildingStats*, MultiMeshInstance3D*> shadow_renderers;
        Ref<Shader> shadow_shader;

        // 分组缓存
        std::unordered_map<BuildingStats*, std::vector<int>> type_grouping_cache;

    protected:
        static void _bind_methods();

    public:
        BuildingManager();
        ~BuildingManager();

        void set_flow_field_manager(Node* p_node);
        void set_unit_manager(Node* p_node);

        // --- 核心功能 ---

        void update(double p_delta);
        void update_multimesh_buffer(double p_delta);

        // 注册建筑：从 txt 加载配置并缓存
        void register_building_type(String p_name, String p_path);

        // 检查某个区域是否可以放置该种类的建筑 (含双重范围逻辑)
        bool is_area_clear(Vector2i p_grid_pos, Ref<BuildingStats> p_stats);

        // 通过类型名称放置建筑
        int place_building_by_type(String p_type_name, Vector2i p_grid_pos, int p_team_id);

        void remove_building(int p_building_id);

        // 根据 ID 获取数据
        Vector2i get_building_grid_pos(int p_building_id) const;
        Ref<BuildingStats> get_building_stats(int p_building_id) const;

        // 返回所有已注册的建筑类型名称列表
        PackedStringArray get_registered_building_types() const;

        Ref<BuildingStats> get_building_stats_by_type(String p_type_name) {
            if (building_types_cache.has(p_type_name)) {
                return building_types_cache[p_type_name];
            }
            return nullptr;
        }
    };
}