#pragma once
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {

    // 明确划分的三种投射物类型
    enum ProjectileType {
        PROJECTILE_BULLET = 0,  // 子弹：直线飞行，速度快，单体伤害
        PROJECTILE_SHELL = 1,   // 炮弹：抛物线飞行，有溅射范围伤害 (AOE)
        PROJECTILE_MISSILE = 2  // 导弹：制导飞行，有最大转向速度，可能带AOE
    };

    class ProjectileStats : public Resource {
        GDCLASS(ProjectileStats, Resource)

    private:
        int projectile_type = PROJECTILE_BULLET;

        // === 通用属性 ===
        float speed = 20.0f;
        float damage = 10.0f;
        String visual_path = "";

        // === 炮弹/导弹特有属性 ===
        float splash_radius = 0.0f;

        // === 炮弹特有属性 ===
        float arc_height = 5.0f;

        // === 导弹特有属性 ===
        float turn_speed = 3.0f;
        float acceleration = 0.0f;  // [新增] 导弹的加速度

    protected:
        static void _bind_methods() {
            // 绑定枚举，方便在 GDScript 中直接使用
            BIND_ENUM_CONSTANT(PROJECTILE_BULLET);
            BIND_ENUM_CONSTANT(PROJECTILE_SHELL);
            BIND_ENUM_CONSTANT(PROJECTILE_MISSILE);

            // 1. projectile_type
            ClassDB::bind_method(D_METHOD("get_projectile_type"), &ProjectileStats::get_projectile_type);
            ClassDB::bind_method(D_METHOD("set_projectile_type", "type"), &ProjectileStats::set_projectile_type);
            ADD_PROPERTY(PropertyInfo(Variant::INT, "projectile_type"), "set_projectile_type", "get_projectile_type");

            // 2. speed
            ClassDB::bind_method(D_METHOD("get_speed"), &ProjectileStats::get_speed);
            ClassDB::bind_method(D_METHOD("set_speed", "speed"), &ProjectileStats::set_speed);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

            // 3. damage
            ClassDB::bind_method(D_METHOD("get_damage"), &ProjectileStats::get_damage);
            ClassDB::bind_method(D_METHOD("set_damage", "damage"), &ProjectileStats::set_damage);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");

            // 4. visual_path
            ClassDB::bind_method(D_METHOD("get_visual_path"), &ProjectileStats::get_visual_path);
            ClassDB::bind_method(D_METHOD("set_visual_path", "visual_path"), &ProjectileStats::set_visual_path);
            ADD_PROPERTY(PropertyInfo(Variant::STRING, "visual_path"), "set_visual_path", "get_visual_path");

            // 5. splash_radius
            ClassDB::bind_method(D_METHOD("get_splash_radius"), &ProjectileStats::get_splash_radius);
            ClassDB::bind_method(D_METHOD("set_splash_radius", "splash_radius"), &ProjectileStats::set_splash_radius);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "splash_radius"), "set_splash_radius", "get_splash_radius");

            // 6. arc_height
            ClassDB::bind_method(D_METHOD("get_arc_height"), &ProjectileStats::get_arc_height);
            ClassDB::bind_method(D_METHOD("set_arc_height", "arc_height"), &ProjectileStats::set_arc_height);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arc_height"), "set_arc_height", "get_arc_height");

            // 7. turn_speed
            ClassDB::bind_method(D_METHOD("get_turn_speed"), &ProjectileStats::get_turn_speed);
            ClassDB::bind_method(D_METHOD("set_turn_speed", "turn_speed"), &ProjectileStats::set_turn_speed);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_speed"), "set_turn_speed", "get_turn_speed");

            // 8. acceleration
            ClassDB::bind_method(D_METHOD("get_acceleration"), &ProjectileStats::get_acceleration);
            ClassDB::bind_method(D_METHOD("set_acceleration", "acceleration"), &ProjectileStats::set_acceleration);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");
        }

    public:
        ProjectileStats() {}
        ~ProjectileStats() {}

        // === Getters & Setters ===

        int get_projectile_type() const { return projectile_type; }
        void set_projectile_type(int p_type) { projectile_type = p_type; }

        float get_speed() const { return speed; }
        void set_speed(float p_speed) { speed = p_speed; }

        float get_damage() const { return damage; }
        void set_damage(float p_damage) { damage = p_damage; }

        String get_visual_path() const { return visual_path; }
        void set_visual_path(const String& p_visual_path) { visual_path = p_visual_path; }

        float get_splash_radius() const { return splash_radius; }
        void set_splash_radius(float p_splash_radius) { splash_radius = p_splash_radius; }

        float get_arc_height() const { return arc_height; }
        void set_arc_height(float p_arc_height) { arc_height = p_arc_height; }

        float get_turn_speed() const { return turn_speed; }
        void set_turn_speed(float p_turn_speed) { turn_speed = p_turn_speed; }

        float get_acceleration() const { return acceleration; }
        void set_acceleration(float p_acceleration) { acceleration = p_acceleration; }
    };

} // namespace godot

// 这一步非常重要，告诉 Godot 你的枚举可以通过类名访问
VARIANT_ENUM_CAST(ProjectileType);