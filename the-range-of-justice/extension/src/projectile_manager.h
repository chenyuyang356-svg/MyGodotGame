#pragma once

#include <godot_cpp/classes/node3d.hpp> 
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <vector>
#include "projectile_stats.h"
#include "building_manager.h" 
#include <unordered_map>

namespace godot {

    // 前向声明
    class UnitManager;
    class AttackManager;

    struct ProjectileData {
        Vector2 position;       // 当前的 2D 逻辑坐标
        Vector2 target_pos;     // 目标的 2D 坐标
        Vector2 start_pos;      // 发射时的 2D 起点坐标

        // --- 高度与3D表现相关 ---
        float start_height;     // 发射时的初始高度 (单位/炮口高度)
        float target_height;    // 目标的受击高度 (默认为0或读取目标半高)
        float current_height;   // 当前飞行高度 (Z/Y轴)
        float arc_height;       // 抛物线的最高点附加高度

        // --- 逻辑相关 ---
        int type;               // 投射物类型
        int target_id;
        bool target_is_building; // 目标是否是建筑
        int source_id;
        bool source_is_building; // 发射源是否是建筑
        float speed;            // 当前速度 
        float acceleration;     // 导弹的加速度
        float damage;
        float splash_radius;

        Ref<ProjectileStats> stats;

        float anim_time = 0.0f;
    };

    struct GodotStringHasher {
        std::size_t operator()(const String& k) const {
            return k.hash(); // 直接调用 Godot String 内部的哈希函数
        }
    };

    class ProjectileManager : public Node3D {
        GDCLASS(ProjectileManager, Node3D)

    private:
        // 存储所有子弹的数组
        std::vector<ProjectileData> projectiles;
        std::unordered_map<String, Ref<ProjectileStats>, GodotStringHasher> projectile_templates;

        // 依赖的管理器
        UnitManager* unit_manager = nullptr;
        BuildingManager* building_manager = nullptr; // 建筑管理器指针
        AttackManager* attack_manager = nullptr;

        // 渲染器映射：每个 ProjectileStats 对应一个 MultiMeshInstance3D
        std::unordered_map<ProjectileStats*, MultiMeshInstance3D*> type_renderers;
        std::unordered_map<ProjectileStats*, MultiMeshInstance3D*> shadow_renderers;

        // 分组缓存：每一帧将投射物按类型分组更新
        std::unordered_map<ProjectileStats*, std::vector<int>> type_grouping_cache;

        // 着色器资源
        Ref<Shader> projectile_shader;
        Ref<Shader> shadow_shader;

    protected:
        static void _bind_methods();

    public:
        ProjectileManager();
        ~ProjectileManager();

        void setup(UnitManager* p_um, AttackManager* p_am);
        void set_building_manager(BuildingManager* p_bm); // 设置建筑管理器

        void spawn_projectile(
            const String& p_type_name, // 直接传入注册好的类型名
            Vector2 p_start_pos, float p_start_height,
            int p_target_id, bool p_target_is_building, float p_target_height,
            int p_source_id, bool p_source_is_building,
            float p_weapon_damage
        );
        virtual void _physics_process(double p_delta) override;
        void update_render_buffer(double p_delta);

        void _internal_register_projectile(Ref<ProjectileStats> p_stats);
        void register_projectile_type(String p_type_name, String p_config_path);
        void register_projectiles_from_dir(String p_dir_path);
    };
}