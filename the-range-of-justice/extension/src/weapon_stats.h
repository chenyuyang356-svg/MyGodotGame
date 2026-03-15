#pragma once
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {
    class WeaponStats : public Resource {
        GDCLASS(WeaponStats, Resource)

    private:
        String weapon_name = "new_weapon";

        // --- 战斗属性 ---
        String projectile_type_name = "";
        float damage = 10.0f;
        float attack_range = 100.0f;
        float attack_interval = 1.0f;
        float projectile_speed = 500.0f;
        float splash_radius = 0.0f;

        // --- 独立旋转系统属性 ---
        bool is_turret = false;         // 是否为独立旋转的武器
        float turn_speed = 3.0f;        // 武器旋转速度（弧度/秒）

        // --- 渲染与动画属性 ---
        String texture_path = "";
        int h_frames = 1;      // 图集水平分栏
        int v_frames = 1;      // 图集垂直分栏
        int idle_frames = 1;   // 待机动画帧数
        int attacking_frames = 1; // 攻击动画帧数
        int idle_row = 0;      // 待机动画所在行
        int attacking_row = 1; // 攻击动画所在行
        int anim_fps = 10;     // 动画播放帧率

    protected:
        static void _bind_methods();

    public:
        WeaponStats() {}
        ~WeaponStats() {}

        // Getters / Setters
        void set_weapon_name(const String& p_val) { weapon_name = p_val; }
        String get_weapon_name() const { return weapon_name; }

        void set_projectile_type_name(const String& p_val) { projectile_type_name = p_val; }
        String get_projectile_type_name() const { return projectile_type_name; }

        void set_damage(float p_val) { damage = p_val; }
        float get_damage() const { return damage; }

        void set_attack_range(float p_val) { attack_range = p_val; }
        float get_attack_range() const { return attack_range; }

        void set_attack_interval(float p_val) { attack_interval = p_val; }
        float get_attack_interval() const { return attack_interval; }

        void set_projectile_speed(float p_val) { projectile_speed = p_val; }
        float get_projectile_speed() const { return projectile_speed; }

        void set_splash_radius(float p_val) { splash_radius = p_val; }
        float get_splash_radius() const { return splash_radius; }

        void set_is_turret(bool p_val) { is_turret = p_val; }
        bool get_is_turret() const { return is_turret; }

        void set_turn_speed(float p_val) { turn_speed = p_val; }
        float get_turn_speed() const { return turn_speed; }

        void set_texture_path(const String& p_val) { texture_path = p_val; }
        String get_texture_path() const { return texture_path; }

        void set_h_frames(int p_val) { h_frames = p_val; }
        int get_h_frames() const { return h_frames; }

        void set_v_frames(int p_val) { v_frames = p_val; }
        int get_v_frames() const { return v_frames; }

        void set_idle_frames(int p_val) { idle_frames = p_val; }
        int get_idle_frames() const { return idle_frames; }

        void set_attacking_frames(int p_val) { attacking_frames = p_val; }
        int get_attacking_frames() const { return attacking_frames; }

        void set_idle_row(int p_val) { idle_row = p_val; }
        int get_idle_row() const { return idle_row; }

        void set_attacking_row(int p_val) { attacking_row = p_val; }
        int get_attacking_row() const { return attacking_row; }

        void set_anim_fps(int p_val) { anim_fps = p_val; }
        int get_anim_fps() const { return anim_fps; }
    };

    struct WeaponMount {
        Ref<WeaponStats> weapon_resource; // 武器的静态资源
        Vector2 local_position;           // 武器在单位上的安装偏移量(如炮台在舰首或舰尾)
    };
}