#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <vector>

#include "fog_manager.h"
#include "game_definitions.h"

namespace godot {

    struct ParticleInstance {
        Vector3 position;
        Vector3 velocity;
        float life = 0.0f;
        float max_life = 1.0f;
        float scale = 1.0f;
        float rotation = 0.0f;
        float shape_angle = 0.0f;  // custom_data.z: 程序化形状朝向 (世界空间弧度, 如炮口方向)
        float custom_float = 0.0f; // 用于映射到 Shader 的 v_is_ripple
        bool active = false;
    };

    struct EffectLayer {
        MultiMeshInstance3D* renderer = nullptr;
        std::vector<ParticleInstance> instances;
        Ref<ShaderMaterial> material; // 每层材质引用 (用于运行时调节程序化参数)
        int next_idx = 0;
        int max_count = 0;
        float gravity = -2.0f;
        float drag = 0.95f;
        float modulate = 1.0f;
        // 程序化特效的缺省发射参数 (emit 传 0 时回退到这里)
        float default_scale = 7.0f;
        float default_life = 0.1f;
        float default_forward_speed = 25.0f;
    };

    class EffectManager : public Node3D {
        GDCLASS(EffectManager, Node3D)

    private:
        HashMap<String, EffectLayer*> layers;
        FogManager* fog_manager = nullptr;
        Ref<Shader> particle_shader;
        Ref<Shader> additive_shader; // 加色混合版本 (发光特效用)

        // 内部更新方法
        void _process_layer_physics(EffectLayer* layer, float delta);
        void _update_multimesh_buffer(EffectLayer* layer);

    protected:
        static void _bind_methods();

    public:
        EffectManager();
        ~EffectManager();

        void setup(Node* p_fog_node);

        // 注册特效类型 (可在 Godot 中通过脚本调用)
        // p_procedural: 启用程序化形状 (不采样贴图, 用 custom_data.z 朝向角生成星芒)
        // p_additive: 使用加色混合 shader (发光特效)
        void register_effect_type(String p_name, Ref<Texture2D> p_texture, int p_max_count, float p_gravity, float p_drag, float p_modulate, bool p_procedural = false, bool p_additive = false);

        // 触发粒子
        void emit_particle(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life, bool p_is_ripple = false);

        // 触发带朝向的粒子 (程序化形状用): 不随机旋转 mesh, 朝向角通过 custom_data.z 传入 shader
        // p_angle_rad: 世界空间 2D 朝向角 (弧度), 与 2D 游戏坐标的 dir.angle() 一致
        // p_scale/p_max_life 传 0 时使用层默认值; p_vel 为零向量时按朝向 * 默认前冲速度计算
        void emit_particle_rot(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life, float p_angle_rad);

        // 运行时调节程序化特效的外观参数 (如炮口闪光)
        // 后三个参数为发射缺省值: 传 -1 表示不修改
        void set_procedural_params(String p_type, Color p_color, float p_core_radius, float p_spike_power, float p_spike_length, float p_side_strength, float p_scale = -1.0f, float p_life = -1.0f, float p_forward_speed = -1.0f);

        // 升级版多层次炮口开火特效调参: 包含火舌长度/锐度、制退器侧喷角/长、火星与光晕
        void set_flash_params(String p_type, Color p_color, float p_core_radius, float p_cone_length, float p_cone_width, float p_side_strength, float p_side_angle, float p_side_length, float p_spark_intensity, float p_glow_radius, float p_scale = -1.0f, float p_life = -1.0f, float p_forward_speed = -1.0f);

        // 像素风格炮口开火特效接口: 支持像素网格块大小(pixel_size)与色阶调色板类型(palette_type)
        void set_pixel_flash_params(String p_type, Color p_color, float p_pixel_size, int p_palette_type, float p_core_radius, float p_cone_length, float p_side_strength, float p_side_angle, float p_side_length, float p_spark_intensity, float p_scale = -1.0f, float p_life = -1.0f, float p_forward_speed = -1.0f);

        // 通用材质参数设置接口
        void set_shader_param(String p_type, String p_param_name, Variant p_value);

        // 每一帧调用
        void update(double delta);
    };
}