#pragma once
#include <godot_cpp/classes/node.hpp>
#include "unit_manager.h" 
#include "building_manager.h"

namespace godot {
    class ProjectileManager;

    class AttackManager : public Node {
        GDCLASS(AttackManager, Node)

    private:
        UnitManager* unit_manager = nullptr;
        BuildingManager* building_manager = nullptr;
        ProjectileManager* projectile_manager = nullptr;

    protected:
        static void _bind_methods();

    public:
        AttackManager();
        ~AttackManager();

        void setup(UnitManager* p_manager);
        void set_building_manager(BuildingManager* p_bmanager);
        void set_projectile_manager(ProjectileManager* p_proj_manager);

        // 核心入口
        void update_units(double p_delta);
        void update_buildings(double p_delta); // 新增：更新建筑的攻击状态

        bool try_get_combat_force(UnitData& p_unit, Vector2& out_force);

        // 伤害接口
        void apply_damage(int target_id, bool is_building, float damage, int attacker_id, bool attacker_is_building);
        void apply_aoe_damage(Vector2 p_epicenter, float p_radius, float p_damage, int p_attacker_id, bool attacker_is_building);

    private:
        // 状态处理器
        void _handle_idle(UnitData& p_unit);
        void _handle_chasing(UnitData& p_unit);
        void _handle_attacking(UnitData& p_unit, double p_delta);
        void _handle_patrolling(UnitData& p_unit);
        void _handle_moving(UnitData& p_unit);

        // 行为逻辑
        bool _try_find_target(UnitData& p_unit);
        bool _try_find_target_for_building(BuildingData& p_building); // 新增：建筑索敌

        bool _is_target_valid(int p_target_id, bool p_is_building);

        // 获取目标的位置和碰撞半径（屏蔽单位和建筑的差异）
        bool _get_target_info(int p_target_id, bool p_is_building, Vector2& out_pos, float& out_radius);

        void _execute_attack(UnitData& attacker, int target_id, bool target_is_building);
        void _execute_building_attack(BuildingData& attacker, int target_id, bool target_is_building);
    };
}