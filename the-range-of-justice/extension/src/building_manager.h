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
#include "economy_manager.h"
#include "fog_manager.h"
#include "weapon_manager.h"

namespace godot {
    class SelectionManager;
    class AttackManager;

    struct GhostBuildingData {
        Ref<BuildingStats> stats;
        Vector2i grid_pos;
        int team_id;
        std::vector<WeaponData> weapons;
    };

    class BuildingManager : public Node3D {
        GDCLASS(BuildingManager, Node3D)

        friend class AttackManager;
        friend class ProjectileManager;

    private:
        FlowFieldManager* flow_field_manager = nullptr;
        UnitManager* unit_manager = nullptr;
        EconomyManager* economy_manager = nullptr;
        FogManager* fog_manager = nullptr;
        WeaponManager* weapon_manager = nullptr;

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

        // --- 残影渲染器 ---
        std::unordered_map<BuildingStats*, MultiMeshInstance3D*> ghost_renderers;
        Ref<Shader> ghost_shader;
        std::unordered_map<BuildingStats*, std::vector<int>> ghost_grouping_cache;

        float ghost_cleanup_timer = 0.0f; // 限制 CPU 抓取屏幕的频率

        // --- 进度条渲染器 ---
        MultiMeshInstance3D* global_progress_bar_renderer = nullptr;
        Ref<Shader> progress_bar_shader;
        void _setup_progress_bar_system();

        // --- 小地图"点"渲染器 ---
        MultiMeshInstance3D* minimap_dot_renderer = nullptr;
        Ref<Shader> minimap_dot_shader;
        std::vector<Vector2> cached_resource_positions; // 缓存地图上的资源点位置
        bool resources_gathered = false;
        void _setup_minimap_renderer(int p_width, int p_height, Vector2i p_cell_size);
        void _gather_resource_positions(); // 扫描并记录资源点

    protected:
        static void _bind_methods();

    public:
        std::unordered_map<int, BuildingData> buildings;
        std::unordered_map<int, GhostBuildingData> ghost_buildings;

        BuildingManager();
        ~BuildingManager();

        void set_flow_field_manager(Node* p_node);
        void set_unit_manager(Node* p_node);
        void set_economy_manager(Node* p_node);
        void set_fog_manager(Node* p_node);
        void set_weapon_manager(Node* p_node);
        void setup_system(int p_width, int p_height, Vector2i p_cell_size);

        // --- 核心功能 ---

        void update(double p_delta);
        void update_multimesh_buffer(double p_delta, float p_alpha, SelectionManager* p_selection_manager);

        void handle_dead_buildings(double p_delta);
        void maintain_ghosts(double p_delta);

        // 注册建筑：从 txt 加载配置并缓存
        void _internal_register_building(Ref<BuildingStats> p_stats);
        void register_building_type(String p_name, String p_path);
        void register_buildings_from_dir(String p_dir_path);

        // 检查某个区域是否可以放置该种类的建筑 (含双重范围逻辑)
        bool is_area_clear(Vector2i p_grid_pos, Ref<BuildingStats> p_stats);

        // 供SelectionManager调用
        int get_building_at_position(Vector2 p_world_pos);
        std::vector<int> get_buildings_of_type_in_area(Ref<BuildingStats> p_stats, Rect2 p_rect, int p_team_id);
        std::vector<int> get_buildings_in_box(Rect2 p_box, int p_team_id);

        // 通过类型名称放置建筑
        int place_building_by_type(String p_type_name, Vector2i p_grid_pos, int p_team_id, int p_forced_id = -1);

        void remove_building(int p_building_id, SelectionManager* p_selection_manager);

        void add_unit_to_production_queue(int p_building_id, String p_unit_type);

        void request_spawn_unit(String p_unit_type, Vector2 p_pos, int p_team_id);

        // 根据 ID 获取数据
        int get_building_team_id(int p_building_id) const;
        Vector2i get_building_grid_pos(int p_building_id) const;
        Ref<BuildingStats> get_building_stats(int p_building_id) const;
        Vector2i get_cell_size() { return flow_field_manager->get_cell_size(); }

        // 返回所有已注册的建筑类型名称列表
        PackedStringArray get_registered_building_types() const;

        Ref<BuildingStats> get_building_stats_by_type(String p_type_name) {
            if (building_types_cache.has(p_type_name)) {
                return building_types_cache[p_type_name];
            }
            return nullptr;
        }

        // 联机相关
        void prepare_interpolation_snapshot(); // 主机专用：捕获当前状态用于渲染插值
    };
}