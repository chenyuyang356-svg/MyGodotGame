#pragma once
#include <godot_cpp/classes/node.hpp>
#include "unit_manager.h" 

namespace godot {
    class ProjectileManager;

    class AttackManager : public Node {
        GDCLASS(AttackManager, Node)

    private:
        UnitManager* unit_manager = nullptr;
        ProjectileManager* projectile_manager = nullptr;

    protected:
        static void _bind_methods();

    public:
        AttackManager();
        ~AttackManager();

        void setup(UnitManager* p_manager);
        void set_projectile_manager(ProjectileManager* p_proj_manager);

        void update_units(double p_delta); // 核心入口

        bool try_get_combat_force(UnitData& p_unit, Vector2& out_force);
        void apply_damage(int target_id, float damage, int attacker_id);
        void apply_aoe_damage(Vector2 p_epicenter, float p_radius, float p_damage, int p_attacker_id);

    private:
        // 状态处理器
        void _handle_idle(UnitData& p_unit);
        void _handle_chasing(UnitData& p_unit);
        void _handle_attacking(UnitData& p_unit, double p_delta);
        void _handle_patrolling(UnitData& p_unit);
        void _handle_moving(UnitData& p_unit);

        // 行为逻辑
        bool _try_find_target(UnitData& p_unit);
        bool _is_target_valid(int p_target_id);
        void _execute_attack(UnitData& attacker, UnitData& defender);
    };
}