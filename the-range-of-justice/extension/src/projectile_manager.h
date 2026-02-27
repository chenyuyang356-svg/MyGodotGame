#pragma once

#include <godot_cpp/classes/node3d.hpp> 
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <vector>

namespace godot {

    // 前向声明
    class UnitManager;
    class AttackManager;

    // 最新的子弹数据结构
    struct ProjectileData {
        Vector2 position;       // 子弹当前位置
        Vector2 target_pos;     // 目标位置（旧版叫 last_known_target_position）
        int target_id;          // 目标ID（旧版叫 target_unit_id）
        int source_id;          // 发射者ID
        float speed;            // 速度
        float damage;           // 伤害
        float splash_radius;    // 溅射半径
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
        void spawn_projectile(Vector2 p_start_pos, int p_target_id, float p_damage, float p_speed, int p_source_id, float p_splash_radius);

        virtual void _physics_process(double p_delta) override;
        void update_render_buffer();
    };
}