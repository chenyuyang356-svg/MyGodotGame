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
        int projectile_type = PROJECTILE_BULLET; // 投射物类型

        // === 通用属性 ===
        float speed = 20.0f;        // 飞行速度
        float damage = 10.0f;       // 基础伤害
        String visual_path = "";    // 绑定的视觉特效/模型路径

        // === 炮弹/导弹特有属性 ===
        float splash_radius = 0.0f; // 溅射半径 (AOE范围，子弹通常为0)

        // === 炮弹特有属性 ===
        float arc_height = 5.0f;    // 抛物线最高点的高度 (用于表现3D抛物线)

        // === 导弹特有属性 ===
        float turn_speed = 3.0f;    // 制导转向速度 (弧度/秒)

    protected:
        static void _bind_methods() {
            // 绑定 projectile_type
            ClassDB::bind_method(D_METHOD("get_projectile_type"), &ProjectileStats::get_projectile_type);
            ClassDB::bind_method(D_METHOD("set_projectile_type", "type"), &ProjectileStats::set_projectile_type);
            ADD_PROPERTY(PropertyInfo(Variant::INT, "projectile_type"), "set_projectile_type", "get_projectile_type");

            // 绑定 speed
            ClassDB::bind_method(D_METHOD("get_speed"), &ProjectileStats::get_speed);
            ClassDB::bind_method(D_METHOD("set_speed", "speed"), &ProjectileStats::set_speed);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

            // （此处省略其他属性的 bind_method 和 ADD_PROPERTY，按相同格式补充即可）
        }

    public:
        ProjectileStats() {}
        ~ProjectileStats() {}

        int get_projectile_type() const { return projectile_type; }
        void set_projectile_type(int p_type) { projectile_type = p_type; }

        float get_speed() const { return speed; }
        void set_speed(float p_speed) { speed = p_speed; }

        // ... 其他 getter/setter
    };
}