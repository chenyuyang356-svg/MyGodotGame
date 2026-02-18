#pragma once
#include <godot_cpp/classes/node.hpp>
#include "unit_manager.h" 

namespace godot {

    class AttackManager : public Node {
        GDCLASS(AttackManager, Node)

    private:
        UnitManager* unit_manager = nullptr;

    protected:
        static void _bind_methods();

    public:
        AttackManager();
        ~AttackManager();

        void setup(UnitManager* p_manager);
        void update_units(double p_delta); // 核心入口

        bool try_get_combat_force(UnitManager::UnitData& p_unit, Vector2& out_force);
    private:
        // 状态处理器
        void _handle_idle(UnitManager::UnitData& p_unit);
        void _handle_chasing(UnitManager::UnitData& p_unit);
        void _handle_attacking(UnitManager::UnitData& p_unit, double p_delta);

        // 行为逻辑
        bool _try_find_target(UnitManager::UnitData& p_unit);
        bool _is_target_valid(int p_target_id);
        void _execute_attack(UnitManager::UnitData& attacker, UnitManager::UnitData& defender);
    };
}