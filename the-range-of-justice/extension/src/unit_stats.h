#pragma once

#include <godot_cpp/classes/resource.hpp>
#include "game_definitions.h" // 引入枚举定义

namespace godot {

    class UnitStats : public Resource {
        GDCLASS(UnitStats, Resource)

    private:
        // --- 生存 ---
        float health_max = 100.0f;
        float health_regen = 1.0f;
        float shield_max = 0.0f;
        float shield_regen = 0.0f;
        ArmorType armor_type = ARMOR_LIGHT;

        // --- 攻击 ---
        float attack_damage = 10.0f;
        float attack_range = 100.0f;
        float attack_interval = 1.0f;
        float splash_radius = 0.0f;
        AttackType attack_type = ATTACK_PHYSICAL;
        float projectile_speed = 500.0f;
        TargetPriority target_priority = PRIORITY_CLOSEST;

        // --- 移动 ---
        float move_speed = 200.0f;
        float turn_speed = 5.0f;
        float collision_radius = 10.0f;
        MoveType move_type = MOVE_GROUND;

        // --- 其他 ---
        float sight_range = 300.0f;
        float aggro_range = 250.0f;
        int cost = 100;
        float build_time = 5.0f;
        BitField<UnitTag> unit_tags = TAG_NONE; // 使用 BitField 包装

    protected:
        static void _bind_methods();

    public:
        UnitStats() {}
        ~UnitStats() {}

        // --- Getters / Setters ---

        void set_health_max(float p_value) { health_max = p_value; }
        float get_health_max() const { return health_max; }

        void set_health_regen(float p_value) { health_regen = p_value; }
        float get_health_regen() const { return health_regen; }

        void set_shield_max(float p_value) { shield_max = p_value; }
        float get_shield_max() const { return shield_max; }

        void set_shield_regen(float p_value) { shield_regen = p_value; }
        float get_shield_regen() const { return shield_regen; }

        void set_armor_type(ArmorType p_value) { armor_type = p_value; }
        ArmorType get_armor_type() const { return armor_type; }

        void set_attack_damage(float p_value) { attack_damage = p_value; }
        float get_attack_damage() const { return attack_damage; }

        void set_attack_range(float p_value) { attack_range = p_value; }
        float get_attack_range() const { return attack_range; }

        void set_attack_interval(float p_value) { attack_interval = p_value; }
        float get_attack_interval() const { return attack_interval; }

        void set_splash_radius(float p_value) { splash_radius = p_value; }
        float get_splash_radius() const { return splash_radius; }

        void set_attack_type(AttackType p_value) { attack_type = p_value; }
        AttackType get_attack_type() const { return attack_type; }

        void set_projectile_speed(float p_value) { projectile_speed = p_value; }
        float get_projectile_speed() const { return projectile_speed; }

        void set_target_priority(TargetPriority p_value) { target_priority = p_value; }
        TargetPriority get_target_priority() const { return target_priority; }

        void set_move_speed(float p_value) { move_speed = p_value; }
        float get_move_speed() const { return move_speed; }

        void set_turn_speed(float p_value) { turn_speed = p_value; }
        float get_turn_speed() const { return turn_speed; }

        void set_collision_radius(float p_value) { collision_radius = p_value; }
        float get_collision_radius() const { return collision_radius; }

        void set_move_type(MoveType p_value) { move_type = p_value; }
        MoveType get_move_type() const { return move_type; }

        void set_sight_range(float p_value) { sight_range = p_value; }
        float get_sight_range() const { return sight_range; }

        void set_aggro_range(float p_value) { aggro_range = p_value; }
        float get_aggro_range() const { return aggro_range; }

        void set_cost(int p_value) { cost = p_value; }
        int get_cost() const { return cost; }

        void set_build_time(float p_value) { build_time = p_value; }
        float get_build_time() const { return build_time; }

        void set_unit_tags(BitField<UnitTag> p_value) { unit_tags = p_value; }
        BitField<UnitTag> get_unit_tags() const { return unit_tags; }
    };
}