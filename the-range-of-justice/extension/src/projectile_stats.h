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
        String projectile_name = "new_projectile";
        float speed = 20.0f;
        float damage = 10.0f;
        bool is_healing = false;

        String visual_path = "";
        int h_frames = 1;
        int v_frames = 1;
        float anim_fps = 10.0f;

        // === 炮弹/导弹特有属性 ===
        float splash_radius = 0.0f;

        // === 炮弹特有属性 ===
        float arc_height = 5.0f;

        // === 导弹特有属性 ===
        float turn_speed = 3.0f;
        float acceleration = 0.0f;  // [新增] 导弹的加速度

    protected:
        static void _bind_methods() {
            // 绑定枚举
            BIND_ENUM_CONSTANT(PROJECTILE_BULLET);
            BIND_ENUM_CONSTANT(PROJECTILE_SHELL);
            BIND_ENUM_CONSTANT(PROJECTILE_MISSILE);

            // 1.投射物类型
            ClassDB::bind_method(D_METHOD("get_projectile_type"), &ProjectileStats::get_projectile_type);
            ClassDB::bind_method(D_METHOD("set_projectile_type", "type"), &ProjectileStats::set_projectile_type);
            ADD_PROPERTY(PropertyInfo(Variant::INT, "projectile_type"), "set_projectile_type", "get_projectile_type");

            ClassDB::bind_method(D_METHOD("get_projectile_name"), &ProjectileStats::get_projectile_name);
            ClassDB::bind_method(D_METHOD("set_projectile_name", "name"), &ProjectileStats::set_projectile_name);
            ADD_PROPERTY(PropertyInfo(Variant::STRING, "projectile_name"), "set_projectile_name", "get_projectile_name");

            // 2.速度
            ClassDB::bind_method(D_METHOD("get_speed"), &ProjectileStats::get_speed);
            ClassDB::bind_method(D_METHOD("set_speed", "speed"), &ProjectileStats::set_speed);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

            // 3. 伤害
            ClassDB::bind_method(D_METHOD("get_damage"), &ProjectileStats::get_damage);
            ClassDB::bind_method(D_METHOD("set_damage", "damage"), &ProjectileStats::set_damage);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");

            ClassDB::bind_method(D_METHOD("get_is_healing"), &ProjectileStats::get_is_healing);
            ClassDB::bind_method(D_METHOD("set_is_healing", "is_healing"), &ProjectileStats::set_is_healing);
            ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_healing"), "set_is_healing", "get_is_healing");

            // 4. 特效所在路径
            ClassDB::bind_method(D_METHOD("get_visual_path"), &ProjectileStats::get_visual_path);
            ClassDB::bind_method(D_METHOD("set_visual_path", "visual_path"), &ProjectileStats::set_visual_path);
            ADD_PROPERTY(PropertyInfo(Variant::STRING, "visual_path"), "set_visual_path", "get_visual_path");

            // 5. 溅射半径
            ClassDB::bind_method(D_METHOD("get_splash_radius"), &ProjectileStats::get_splash_radius);
            ClassDB::bind_method(D_METHOD("set_splash_radius", "splash_radius"), &ProjectileStats::set_splash_radius);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "splash_radius"), "set_splash_radius", "get_splash_radius");

            // 6. 最大高度
            ClassDB::bind_method(D_METHOD("get_arc_height"), &ProjectileStats::get_arc_height);
            ClassDB::bind_method(D_METHOD("set_arc_height", "arc_height"), &ProjectileStats::set_arc_height);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "arc_height"), "set_arc_height", "get_arc_height");

            // 7. 转向速度
            ClassDB::bind_method(D_METHOD("get_turn_speed"), &ProjectileStats::get_turn_speed);
            ClassDB::bind_method(D_METHOD("set_turn_speed", "turn_speed"), &ProjectileStats::set_turn_speed);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "turn_speed"), "set_turn_speed", "get_turn_speed");

            // 8. 加速度
            ClassDB::bind_method(D_METHOD("get_acceleration"), &ProjectileStats::get_acceleration);
            ClassDB::bind_method(D_METHOD("set_acceleration", "acceleration"), &ProjectileStats::set_acceleration);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "acceleration"), "set_acceleration", "get_acceleration");

            ClassDB::bind_method(D_METHOD("get_h_frames"), &ProjectileStats::get_h_frames);
            ClassDB::bind_method(D_METHOD("set_h_frames", "count"), &ProjectileStats::set_h_frames);
            ADD_PROPERTY(PropertyInfo(Variant::INT, "h_frames"), "set_h_frames", "get_h_frames");

            ClassDB::bind_method(D_METHOD("get_v_frames"), &ProjectileStats::get_v_frames);
            ClassDB::bind_method(D_METHOD("set_v_frames", "count"), &ProjectileStats::set_v_frames);
            ADD_PROPERTY(PropertyInfo(Variant::INT, "v_frames"), "set_v_frames", "get_v_frames");

            ClassDB::bind_method(D_METHOD("get_anim_fps"), &ProjectileStats::get_anim_fps);
            ClassDB::bind_method(D_METHOD("set_anim_fps", "fps"), &ProjectileStats::set_anim_fps);
            ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "anim_fps"), "set_anim_fps", "get_anim_fps");
        }

    public:
        ProjectileStats() {}
        ~ProjectileStats() {}

        // === Getters & Setters ===

        int get_projectile_type() const { return projectile_type; }
        void set_projectile_type(int p_type) { projectile_type = p_type; }

        String get_projectile_name() const { return projectile_name; }
        void set_projectile_name(const String& p_name) { projectile_name = p_name; }

        float get_speed() const { return speed; }
        void set_speed(float p_speed) { speed = p_speed; }

        float get_damage() const { return damage; }
        void set_damage(float p_damage) { damage = p_damage; }

        void set_is_healing(bool p_val) { is_healing = p_val; }
        bool get_is_healing() const { return is_healing; }

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

        int get_h_frames() const { return h_frames; }
        void set_h_frames(int p_count) { h_frames = p_count; }

        int get_v_frames() const { return v_frames; }
        void set_v_frames(int p_count) { v_frames = p_count; }

        float get_anim_fps() const { return anim_fps; }
        void set_anim_fps(float p_fps) { anim_fps = p_fps; }
    };

} 

VARIANT_ENUM_CAST(ProjectileType);