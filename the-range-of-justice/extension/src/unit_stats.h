#pragma once
#include <vector>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/translation_server.hpp>
#include "game_definitions.h" // 引入枚举定义
#include "weapon_stats.h"

namespace godot {
    //目前这个结构体是供非独立武器使用的
    struct Weapon {
        String projectile_type_name; // 发射的子弹视觉类型（用于在 ProjectileManager 查找模型）
        float damage = 10.0f;        // 武器伤害
        float attack_range = 100.0f; // 武器射程
        float attack_interval = 1.0f;// 攻击间隔（冷却时间）
        float projectile_speed = 500.0f; // 子弹飞行速度
        float splash_radius = 0.0f;  // 范围溅射半径 (AOE)
    };

    struct EffectPoint {
        String effect_type;
        Vector2 local_position;
        float scale_override = -1.0f; // -1 表示使用默认
        float life_override = -1.0f;  // -1 表示使用默认
        bool is_ripple = false;
    };


    class UnitStats : public Resource {
        GDCLASS(UnitStats, Resource)

    public:
        String unit_name = "new_unit";
        // 用于翻译
        String unit_name_key = "KEY_NEW_UNIT";
        String unit_description_key = "KEY_NO_DESC";
        // --- 生存 ---
        float health_max = 100.0f;
        float health_regen = 1.0f;
        float shield_max = 0.0f;
        float shield_regen = 0.0f;
        ArmorType armor_type = ARMOR_LIGHT;

        // --- 攻击 ---
        TargetPriority target_priority = PRIORITY_CLOSEST;
        bool can_fire_on_move = false;
        std::vector<Weapon> weapons; // 这些是不独立于单位的武器
        std::vector<WeaponMount> weapon_mounts; // 替换原来的 std::vector<WeaponStats> weapons;

        // --- 移动 ---
        float mass = 1.0f;
        float move_speed = 200.0f;
        float acceleration = 1000.0f;      
        float turn_speed = 5.0f;
        float turn_acceleration = 20.0f;
        float collision_radius = 10.0f;
        float base_height = 0.0f;
        MoveType move_type = MOVE_GROUND;

        // --- 其他 ---
        float sight_range = 300.0f;
        float aggro_range = 250.0f;
        int cost = 100;
        float build_time = 5.0f;
        BitField<UnitTag> unit_tags = TAG_NONE; // 使用 BitField 包装
        PackedStringArray producible_buildings;

        // 纹理与图集基础信息
        String texture_path;
        int h_frames = 1;      // 整个图集的水平分栏
        int v_frames = 1;      // 整个图集的垂直分栏

        // 动画配置信息
        int move_frames = 1;   // 移动动画占多少帧
        int idle_frames = 1;   // 待机动画占多少帧
        int move_row = 1;      // 移动动画在图集的第几行
        int idle_row = 0;      // 待机动画在图集的第几行
        int anim_fps = 10;     // 动画播放速度

        float dying_time = 1.0f;

        // 粒子信息
        float emit_threshold_override = -1.0f;
        float particle_scale_override = -1.0f; // 全局缩放偏移
        std::vector<EffectPoint> effect_points;


    protected:
        static void _bind_methods();

    public:
        UnitStats() {}
        ~UnitStats() {}

        // --- Getters / Setters ---
        void set_unit_name(const String p_name) { unit_name = p_name; }
        String get_unit_name() const { return unit_name; }

        void set_unit_name_key(const String p_key) { unit_name_key = p_key; }
        String get_unit_name_key() const { return unit_name_key; }
        String get_translated_name() const {
            return TranslationServer::get_singleton()->translate(unit_name_key);
        }

        void set_unit_description_key(const String p_key) { unit_description_key = p_key; }
        String get_unit_description_key() const { return unit_description_key; }
        String get_translated_description() const {
            return TranslationServer::get_singleton()->translate(unit_description_key);
        }

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

        void set_target_priority(TargetPriority p_value) { target_priority = p_value; }
        TargetPriority get_target_priority() const { return target_priority; }

        void set_mass(float p_value) { mass = p_value; }
        float get_mass() const { return mass; }

        void set_move_speed(float p_value) { move_speed = p_value; }
        float get_move_speed() const { return move_speed; }

        void set_acceleration(float p_val) { acceleration = p_val; }
        float get_acceleration() const { return acceleration; }

        void set_turn_speed(float p_value) { turn_speed = p_value; }
        float get_turn_speed() const { return turn_speed; }

        void set_turn_acceleration(float p_val) { turn_acceleration = p_val; }
        float get_turn_acceleration() const { return turn_acceleration; }

        void set_collision_radius(float p_value) { collision_radius = p_value; }
        float get_collision_radius() const { return collision_radius; }

        void set_base_height(float p_value) { base_height = p_value; }
        float get_base_height() const { return base_height; }

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

        void set_can_fire_on_move(bool p_value) { can_fire_on_move = p_value; }
        bool get_can_fire_on_move() const { return can_fire_on_move; }

        void set_unit_tags(BitField<UnitTag> p_value) { unit_tags = p_value; }
        BitField<UnitTag> get_unit_tags() const { return unit_tags; }

        void set_producible_buildings(const PackedStringArray& p_buildings) { producible_buildings = p_buildings; }
        PackedStringArray get_producible_buildings() const { return producible_buildings; }

        // 纹理路径
        void set_texture_path(const String p_path) { texture_path = p_path; }
        String get_texture_path() const { return texture_path; }

        // 图集分栏
        void set_h_frames(int p_val) { h_frames = p_val; }
        int get_h_frames() const { return h_frames; }

        void set_v_frames(int p_val) { v_frames = p_val; }
        int get_v_frames() const { return v_frames; }

        // 动画帧数
        void set_move_frames(int p_val) { move_frames = p_val; }
        int get_move_frames() const { return move_frames; }

        void set_idle_frames(int p_val) { idle_frames = p_val; }
        int get_idle_frames() const { return idle_frames; }

        // 动画行号
        void set_move_row(int p_val) { move_row = p_val; }
        int get_move_row() const { return move_row; }

        void set_idle_row(int p_val) { idle_row = p_val; }
        int get_idle_row() const { return idle_row; }

        // 播放速度
        void set_anim_fps(int p_val) { anim_fps = p_val; }
        int get_anim_fps() const { return anim_fps; }

        void set_dying_time(float p_val) { dying_time = p_val; }
        float get_dying_time() const { return dying_time; }

        void set_emit_threshold_override(float p_val) { emit_threshold_override = p_val; }
        float get_emit_threshold_override() const { return emit_threshold_override; }

        void set_particle_scale_override(float p_val) { particle_scale_override = p_val; }
        float get_particle_scale_override() const { return particle_scale_override; }
    };
}