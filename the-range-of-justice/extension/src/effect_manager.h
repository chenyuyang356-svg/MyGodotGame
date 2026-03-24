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

namespace godot {

    struct ParticleInstance {
        Vector3 position;
        Vector3 velocity;
        float life = 0.0f;
        float max_life = 1.0f;
        float scale = 1.0f;
        float rotation = 0.0f;
        bool active = false;
    };

    struct EffectLayer {
        MultiMeshInstance3D* renderer = nullptr;
        std::vector<ParticleInstance> instances;
        int next_idx = 0;
        int max_count = 0;
        float gravity = -2.0f;
        float drag = 0.95f;
        float modulate = 1.0f;
    };

    class EffectManager : public Node3D {
        GDCLASS(EffectManager, Node3D)

    private:
        HashMap<String, EffectLayer*> layers;
        FogManager* fog_manager = nullptr;
        Ref<Shader> particle_shader;

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
        void register_effect_type(String p_name, Ref<Texture2D> p_texture, int p_max_count, float p_gravity, float p_drag, float p_modulate);

        // 触发粒子
        void emit_particle(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life);

        // 每一帧调用
        void update(double delta);
    };
}