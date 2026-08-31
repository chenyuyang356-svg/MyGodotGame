#include "effect_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

EffectManager::EffectManager() {
    // 预加载粒子 Shader
    particle_shader = ResourceLoader::get_singleton()->load("res://shader/simple_particle.gdshader");
    additive_shader = ResourceLoader::get_singleton()->load("res://shader/particle_add.gdshader");
}

EffectManager::~EffectManager() {
    for (auto& entry : layers) {
        delete entry.value;
    }
}

void EffectManager::setup(Node* p_fog_node) {
    fog_manager = Object::cast_to<FogManager>(p_fog_node);
}

void EffectManager::register_effect_type(String p_name, Ref<Texture2D> p_texture, int p_max_count, float p_gravity, float p_drag, float p_modulate, bool p_procedural, bool p_additive) {
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
    if (p_texture.is_valid()) {
        Vector2 size = p_texture->get_size();
        qm->set_size(size);
    }
    // 程序化模式不依赖贴图: 贴图为 null 时 QuadMesh 默认 1x1, 尺寸由 emit 的 scale 控制
    mm->set_mesh(qm);
    layer->renderer->set_multimesh(mm);

    // 配置材质
    Ref<ShaderMaterial> mat; mat.instantiate();
    mat->set_shader(p_additive ? additive_shader : particle_shader);
    mat->set_shader_parameter("albedo_tex", p_texture);
    mat->set_shader_parameter("modulate", p_modulate);
    if (p_procedural) {
        mat->set_shader_parameter("procedural", true);
    }
    layer->material = mat;

    if (fog_manager) {
        mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    layer->renderer->set_material_override(mat);
    layers[p_name] = layer;
}

void EffectManager::emit_particle(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life, bool p_is_ripple) {
    if (!layers.has(p_type)) return;

    EffectLayer* layer = layers[p_type];
    ParticleInstance& p = layer->instances[layer->next_idx];

    p.position = p_pos;
    p.velocity = p_vel;
    p.life = 0.0f;
    p.max_life = p_max_life * (1.0f + UtilityFunctions::randf() * 0.5f); // 随机寿命
    p.scale = p_scale;
    p.rotation = UtilityFunctions::randf() * Math_PI * 2.0;
    p.custom_float = p_is_ripple ? 1.0f : 0.0f;
    p.active = true;

    // 环形索引增加
    layer->next_idx = (layer->next_idx + 1) % layer->max_count;
}

void EffectManager::emit_particle_rot(String p_type, Vector3 p_pos, Vector3 p_vel, float p_scale, float p_max_life, float p_angle_rad) {
    if (!layers.has(p_type)) return;

    EffectLayer* layer = layers[p_type];
    ParticleInstance& p = layer->instances[layer->next_idx];

    // 缺省参数回退: scale/life 传 0 时使用层默认值; 速度为零向量时按朝向 * 默认前冲速度
    float final_scale = (p_scale > 0.0f) ? p_scale : layer->default_scale;
    float final_life = (p_max_life > 0.0f) ? p_max_life : layer->default_life;
    Vector3 final_vel = p_vel;
    if (final_vel.length_squared() < 0.0001f && layer->default_forward_speed > 0.0f) {
        final_vel = Vector3(Math::cos(p_angle_rad), 0.0f, Math::sin(p_angle_rad)) * layer->default_forward_speed;
    }

    p.position = p_pos;
    p.velocity = final_vel;
    p.life = 0.0f;
    p.max_life = final_life;
    p.scale = final_scale;
    p.rotation = 0.0f;                 // 不随机旋转 mesh, 方向完全交给 shader
    p.shape_angle = p_angle_rad;
    p.custom_float = 0.0f;
    p.active = true;

    // 环形索引增加
    layer->next_idx = (layer->next_idx + 1) % layer->max_count;
}

void EffectManager::set_procedural_params(String p_type, Color p_color, float p_core_radius, float p_spike_power, float p_spike_length, float p_side_strength, float p_scale, float p_life, float p_forward_speed) {
    if (!layers.has(p_type)) return;
    Ref<ShaderMaterial> mat = layers[p_type]->material;
    if (mat.is_null()) return;

    mat->set_shader_parameter("procedural", true);
    mat->set_shader_parameter("flash_color", p_color);
    mat->set_shader_parameter("core_radius", p_core_radius);
    mat->set_shader_parameter("cone_width", p_spike_power);
    mat->set_shader_parameter("cone_length", p_spike_length);
    mat->set_shader_parameter("side_strength", p_side_strength);

    // 可选的发射缺省值 (传 -1 表示不修改)
    EffectLayer* layer = layers[p_type];
    if (p_scale > 0.0f) layer->default_scale = p_scale;
    if (p_life > 0.0f) layer->default_life = p_life;
    if (p_forward_speed >= 0.0f) layer->default_forward_speed = p_forward_speed;
}

void EffectManager::set_flash_params(String p_type, Color p_color, float p_core_radius, float p_cone_length, float p_cone_width, float p_side_strength, float p_side_angle, float p_side_length, float p_spark_intensity, float p_glow_radius, float p_scale, float p_life, float p_forward_speed) {
    if (!layers.has(p_type)) return;
    Ref<ShaderMaterial> mat = layers[p_type]->material;
    if (mat.is_null()) return;

    mat->set_shader_parameter("procedural", true);
    mat->set_shader_parameter("flash_color", p_color);
    mat->set_shader_parameter("core_radius", p_core_radius);
    mat->set_shader_parameter("cone_length", p_cone_length);
    mat->set_shader_parameter("side_strength", p_side_strength);
    mat->set_shader_parameter("side_angle", p_side_angle);
    mat->set_shader_parameter("side_length", p_side_length);
    mat->set_shader_parameter("spark_intensity", p_spark_intensity);

    EffectLayer* layer = layers[p_type];
    if (p_scale > 0.0f) layer->default_scale = p_scale;
    if (p_life > 0.0f) layer->default_life = p_life;
    if (p_forward_speed >= 0.0f) layer->default_forward_speed = p_forward_speed;
}

void EffectManager::set_pixel_flash_params(String p_type, Color p_color, float p_pixel_size, int p_palette_type, float p_core_radius, float p_cone_length, float p_side_strength, float p_side_angle, float p_side_length, float p_spark_intensity, float p_scale, float p_life, float p_forward_speed) {
    if (!layers.has(p_type)) return;
    Ref<ShaderMaterial> mat = layers[p_type]->material;
    if (mat.is_null()) return;

    mat->set_shader_parameter("procedural", true);
    mat->set_shader_parameter("flash_color", p_color);
    mat->set_shader_parameter("pixel_size", p_pixel_size);
    mat->set_shader_parameter("palette_type", p_palette_type);
    mat->set_shader_parameter("core_radius", p_core_radius);
    mat->set_shader_parameter("cone_length", p_cone_length);
    mat->set_shader_parameter("side_strength", p_side_strength);
    mat->set_shader_parameter("side_angle", p_side_angle);
    mat->set_shader_parameter("side_length", p_side_length);
    mat->set_shader_parameter("spark_intensity", p_spark_intensity);

    EffectLayer* layer = layers[p_type];
    if (p_scale > 0.0f) layer->default_scale = p_scale;
    if (p_life > 0.0f) layer->default_life = p_life;
    if (p_forward_speed >= 0.0f) layer->default_forward_speed = p_forward_speed;
}

void EffectManager::set_shader_param(String p_type, String p_param_name, Variant p_value) {
    if (!layers.has(p_type)) return;
    Ref<ShaderMaterial> mat = layers[p_type]->material;
    if (mat.is_null()) return;
    mat->set_shader_parameter(p_param_name, p_value);
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
        xform.origin = Vector3(p.position.x, p.position.y + fake_depth_offset, p.position.z - p.position.y);
        // 这里的缩放可以随寿命衰减，或者在 Shader 里做
        xform.basis = Basis().scaled(Vector3(p.scale, p.scale, p.scale)).rotated(Vector3(1, 0, 0), Math_PI / 2.0);
        xform.basis = (xform.basis).rotated(Vector3(0, -1, 0), (p.rotation + Math_PI / 2.0f));

        mm->set_instance_transform(i, xform);

        // 传递寿命百分比到 Shader (X通道), is_ripple (Y通道), 朝向角 (Z通道)
        float life_ratio = p.life / p.max_life;
        mm->set_instance_custom_data(i, Color(life_ratio, p.custom_float, p.shape_angle, 0));
    }
}

void EffectManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "fog_node"), &EffectManager::setup);
    ClassDB::bind_method(D_METHOD("register_effect_type", "name", "texture", "max_count", "gravity", "drag", "modulate", "procedural", "additive"),
        &EffectManager::register_effect_type, DEFVAL(false), DEFVAL(false));
    ClassDB::bind_method(D_METHOD("emit_particle", "type", "pos", "vel", "scale", "max_life", "is_ripple"), &EffectManager::emit_particle, DEFVAL(false));
    ClassDB::bind_method(D_METHOD("emit_particle_rot", "type", "pos", "vel", "scale", "max_life", "angle_rad"), &EffectManager::emit_particle_rot);
    ClassDB::bind_method(D_METHOD("set_procedural_params", "type", "color", "core_radius", "spike_power", "spike_length", "side_strength", "scale", "life", "forward_speed"),
        &EffectManager::set_procedural_params, DEFVAL(-1.0f), DEFVAL(-1.0f), DEFVAL(-1.0f));
    ClassDB::bind_method(D_METHOD("set_flash_params", "type", "color", "core_radius", "cone_length", "cone_width", "side_strength", "side_angle", "side_length", "spark_intensity", "glow_radius", "scale", "life", "forward_speed"),
        &EffectManager::set_flash_params, DEFVAL(-1.0f), DEFVAL(-1.0f), DEFVAL(-1.0f));
    ClassDB::bind_method(D_METHOD("set_pixel_flash_params", "type", "color", "pixel_size", "palette_type", "core_radius", "cone_length", "side_strength", "side_angle", "side_length", "spark_intensity", "scale", "life", "forward_speed"),
        &EffectManager::set_pixel_flash_params, DEFVAL(-1.0f), DEFVAL(-1.0f), DEFVAL(-1.0f));
    ClassDB::bind_method(D_METHOD("set_shader_param", "type", "param_name", "value"), &EffectManager::set_shader_param);
}