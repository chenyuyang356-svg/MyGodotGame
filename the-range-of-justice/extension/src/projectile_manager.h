#pragma once

#include <godot_cpp/classes/node3d.hpp> 
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <vector>
#include <projectile_stats.h>

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
        int type;               // 投射物类型 (ProjectileType)
        int target_id;
        int source_id;
        float speed;            // 当前速度 
        float acceleration;     // 导弹的加速度
        float damage;
        float splash_radius;
    };

    class ProjectileManager : public Node3D {
        GDCLASS(ProjectileManager, Node3D)

    private:
        // 存储所有子弹的数组（旧版叫 active_projectiles，现在统一叫 projectiles）
        std::vector<ProjectileData> projectiles;

        // 依赖的管理器
        UnitManager* unit_manager = nullptr;
        AttackManager* attack_manager = nullptr;

        // 渲染组件
        MultiMeshInstance3D* multimesh_instance = nullptr;
        Ref<MultiMesh> multimesh;

    protected:
        static void _bind_methods();

    public:
        ProjectileManager();
        ~ProjectileManager();

        void setup(UnitManager* p_um, AttackManager* p_am);

        // 注意这里的参数也要和 cpp 里的一致
        void spawn_projectile(
            Vector2 p_start_pos, float p_start_height,
            int p_target_id, float p_target_height,
            float p_damage, float p_speed,
            int p_source_id, float p_splash_radius,
            int p_type, float p_arc_height, float p_acceleration
        );
        virtual void _physics_process(double p_delta) override;
        void update_render_buffer();
    };
}