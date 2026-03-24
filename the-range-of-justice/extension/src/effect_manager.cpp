#include "effect_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

EffectManager::EffectManager() {
    // 预加载粒子 Shader
    particle_shader = ResourceLoader::get_singleton()->load("res://shader/simple_particle.gdshader");
}

EffectManager::~EffectManager() {
    for (auto& entry : layers) {
        delete entry.value;
    }
}

void EffectManager::setup(Node* p_fog_node) {
    fog_manager = Object::cast_to<FogManager>(p_fog_node);
}

void EffectManager::register_effect_type(String p_name, Ref<Texture2D> p_texture, int p_max_count, float p_gravity, float p_drag, float p_modulate) {
    if (layers.has(p_name)) return;

    EffectLayer* layer = new EffectLayer();
    layer->max_count = p_max_count;
    layer->gravity = p_gravity;
    layer->drag = p_drag;
    layer->modulate = p_modulate;
    layer->instances.resize(p_max_count);
    layer->next_idx = 0;

    // 配置 MultiMesh
    layer->renderer = memnew(MultiMeshInstance3D);
    layer->renderer->set_name(p_name + "_Batch");
    add_child(layer->renderer);

    Ref<MultiMesh> mm; mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_custom_data(true); // 用于传递寿命比例
    mm->set_instance_count(p_max_count);

    Ref<QuadMesh> qm; qm.instantiate();
    qm->set_size(Vector2(16.0f, 16.0f));
    mm->set_mesh(qm);
    layer->renderer->set_multimesh(mm);

    // 配置材质
    Ref<ShaderMaterial> mat; mat.instantiate();
    mat->set_shader(particle_shader);
    mat->set_shader_parameter("albedo_tex", p_texture);
    mat->set_shader_parameter("modulate", p_modulate);

    if (fog_manager) {
        mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    layer->renderer->set_material_override(mat);
    layers[p_name] = layer;
}

void EffectManager::emit_particle(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life) {
    if (!layers.has(p_type)) return;

    EffectLayer* layer = layers[p_type];
    ParticleInstance& p = layer->instances[layer->next_idx];

    p.position = p_pos;
    p.velocity = p_vel;
    p.life = 0.0f;
    p.max_life = p_max_life * (1.0f + UtilityFunctions::randf() * 0.5f); // 随机寿命
    p.scale = p_scale;
    p.rotation = UtilityFunctions::randf() * Math_PI * 2.0;
    p.active = true;

    // 环形索引增加
    layer->next_idx = (layer->next_idx + 1) % layer->max_count;
}

void EffectManager::update(double p_delta) {
    for (auto& it : layers) {
        EffectLayer* layer = it.value;
        _process_layer_physics(layer, (float)p_delta);
        _update_multimesh_buffer(layer);
    }
}

void EffectManager::_process_layer_physics(EffectLayer* p_layer, float p_delta) {
    for (int i = 0; i < p_layer->max_count; ++i) {
        ParticleInstance& p = p_layer->instances[i];
        if (!p.active) continue;

        p.life += p_delta;
        if (p.life >= p.max_life) {
            p.active = false;
            continue;
        }

        // 基础物理：重力 + 阻力 + 位移
        p.velocity.y += p_layer->gravity * p_delta;
        p.velocity *= p_layer->drag;
        p.position += p.velocity * p_delta;
    }
}

void EffectManager::_update_multimesh_buffer(EffectLayer* p_layer) {
    Ref<MultiMesh> mm = p_layer->renderer->get_multimesh();

    for (int i = 0; i < p_layer->max_count; ++i) {
        ParticleInstance& p = p_layer->instances[i];

        if (!p.active) {
            // 将不活跃的粒子缩放到0并移到地下，避免渲染
            mm->set_instance_transform(i, Transform3D().scaled(Vector3(0, 0, 0)));
            continue;
        }

        // 计算看板矩阵 (Billboard) - 简单处理让它面向相机 Y 轴
        float fake_depth_offset = p.position.z * 0.0001f;

        Transform3D xform;
        xform.origin = Vector3(p.position.x, p.position.y + fake_depth_offset, p.position.z);
        // 这里的缩放可以随寿命衰减，或者在 Shader 里做
        xform.basis = Basis().scaled(Vector3(p.scale, p.scale, p.scale)).rotated(Vector3(1, 0, 0), Math_PI / 2.0);
        xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (p.rotation + Math_PI / 2.0f));

        mm->set_instance_transform(i, xform);

        // 传递寿命百分比到 Shader (X通道)
        float life_ratio = p.life / p.max_life;
        mm->set_instance_custom_data(i, Color(life_ratio, 0, 0, 0));
    }
}

void EffectManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "fog_node"), &EffectManager::setup);
    ClassDB::bind_method(D_METHOD("register_effect_type", "name", "texture", "max_count", "gravity", "drag", "modulate"), &EffectManager::register_effect_type);
    ClassDB::bind_method(D_METHOD("emit_particle", "type", "pos", "vel", "scale", "max_life"), &EffectManager::emit_particle);
}