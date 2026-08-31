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
        bool can_attack_ground = true;
        bool can_attack_air = false;

        // --- 独立旋转系统属性 ---
        bool is_turret = false;         // 是否为独立旋转的武器
        float turn_speed = 3.0f;        // 武器旋转速度（弧度/秒）
        float idle_rotate_speed = 0.0f; // 无目标时炮塔缓慢旋转速度（弧度/秒，0=关闭）
        Vector2 rotation_center = Vector2(0, 0);
        Vector3 muzzle_offset = Vector3(0, 0, 0); // x,y 为水平偏移, z 为高度偏移 (主枪口兼容)
        PackedVector3Array muzzle_offsets;        // 支持定义多个枪口坐标 (双联装/多管发射)
        int firing_mode = 0;                      // 0 = Simultaneous (齐射), 1 = Alternating (交替轮射)
        bool muzzle_flash_enabled = true; // 开火时是否发射炮口闪光粒子
        float muzzle_flash_angle = 0.0f;  // 炮口闪光喷射方向偏移 (度, 相对炮管朝向, 正=顺时针)
        float flash_scale = 0.0f;         // 专属开火闪光尺寸 (<=0 时使用全局默认值)
        float flash_life = 0.0f;          // 专属开火闪光寿命 (<=0 时使用全局默认值)
        String flash_preset = "cannon";   // 专属开火预设 (cannon, autogun, heavy, energy)
        int flash_trigger_frame = 0;      // 开火在攻击动画第几帧触发 (0=第0帧瞬发)
        float firing_tolerance = 15.0f;

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

        void set_can_attack_ground(bool p_val) { can_attack_ground = p_val; }
        bool get_can_attack_ground() const { return can_attack_ground; }

        void set_can_attack_air(bool p_val) { can_attack_air = p_val; }
        bool get_can_attack_air() const { return can_attack_air; }


        void set_is_turret(bool p_val) { is_turret = p_val; }
        bool get_is_turret() const { return is_turret; }

        void set_turn_speed(float p_val) { turn_speed = p_val; }
        float get_turn_speed() const { return turn_speed; }

        void set_idle_rotate_speed(float p_val) { idle_rotate_speed = p_val; }
        float get_idle_rotate_speed() const { return idle_rotate_speed; }

        void set_rotation_center(Vector2 p_val) { rotation_center = p_val; }
        Vector2 get_rotation_center() const { return rotation_center; }

        void set_muzzle_offset(Vector3 p_val) {
            muzzle_offset = p_val;
            if (muzzle_offsets.is_empty()) {
                muzzle_offsets.append(p_val);
            } else {
                muzzle_offsets.set(0, p_val);
            }
        }
        Vector3 get_muzzle_offset() const { return muzzle_offset; }

        void set_muzzle_offsets(const PackedVector3Array& p_val) {
            muzzle_offsets = p_val;
            if (!muzzle_offsets.is_empty()) {
                muzzle_offset = muzzle_offsets[0];
            }
        }
        PackedVector3Array get_muzzle_offsets() const {
            if (muzzle_offsets.is_empty()) {
                PackedVector3Array arr;
                arr.append(muzzle_offset);
                return arr;
            }
            return muzzle_offsets;
        }

        void set_firing_mode(int p_val) { firing_mode = p_val; }
        int get_firing_mode() const { return firing_mode; }

        void set_muzzle_flash_enabled(bool p_val) { muzzle_flash_enabled = p_val; }
        bool get_muzzle_flash_enabled() const { return muzzle_flash_enabled; }

        void set_muzzle_flash_angle(float p_val) { muzzle_flash_angle = p_val; }
        float get_muzzle_flash_angle() const { return muzzle_flash_angle; }

        void set_flash_scale(float p_val) { flash_scale = p_val; }
        float get_flash_scale() const { return flash_scale; }

        void set_flash_life(float p_val) { flash_life = p_val; }
        float get_flash_life() const { return flash_life; }

        void set_flash_preset(const String& p_val) { flash_preset = p_val; }
        String get_flash_preset() const { return flash_preset; }

        void set_flash_trigger_frame(int p_val) { flash_trigger_frame = p_val; }
        int get_flash_trigger_frame() const { return flash_trigger_frame; }

        void set_firing_tolerance(float p_val) { firing_tolerance = p_val; }
        float get_firing_tolerance() const { return firing_tolerance; }


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