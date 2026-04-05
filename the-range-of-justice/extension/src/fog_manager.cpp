#include "fog_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

FogManager::FogManager() {}
FogManager::~FogManager() {}

void FogManager::setup_fog(Vector2 p_map_pos, Vector2 p_map_size) {
    map_size = p_map_size;
    map_pos = p_map_pos;

    // 1. 设置实时视野 Viewport
    vpc_live = memnew(SubViewport);
    vpc_live->set_size(Vector2i(fog_resolution, fog_resolution));
    vpc_live->set_transparent_background(true);
    vpc_live->set_clear_mode(SubViewport::CLEAR_MODE_ALWAYS);
    vpc_live->set_update_mode(SubViewport::UPDATE_ALWAYS);
    vpc_live->set_use_hdr_2d(false); // 只需要灰度图，关掉HDR省性能
    add_child(vpc_live);

    // 【新增】配置全局光亮层，为了使其垫底，最早被加入
    global_light_rect = memnew(ColorRect);
    global_light_rect->set_size(Vector2(fog_resolution, fog_resolution));
    global_light_rect->set_color(Color(1.0f, 1.0f, 1.0f, 1.0f)); // 纯白，代表全局视野
    global_light_rect->hide();
    vpc_live->add_child(global_light_rect);
    set_fog_mode(fog_mode);

    // 2. 在 vpc_live 中添加 2D 相机，对齐 0-1 空间
    Camera2D* cam = memnew(Camera2D);
    cam->set_anchor_mode(Camera2D::ANCHOR_MODE_FIXED_TOP_LEFT);
    vpc_live->add_child(cam);

    // 3. 配置 MultiMeshInstance2D (用于高效画几千个圆)
    vision_renderer = memnew(MultiMeshInstance2D);
    vision_multimesh.instantiate();
    vision_multimesh->set_mesh(memnew(QuadMesh)); // 使用简单的平面
    vision_multimesh->set_transform_format(MultiMesh::TRANSFORM_2D);
    vision_renderer->set_multimesh(vision_multimesh);
    vision_renderer->set_texture(brush_texture); // 这是一个带有羽化边缘的白色圆形贴图
    vpc_live->add_child(vision_renderer);

    // 4. 设置探索历史 Viewport
    vpc_history = memnew(SubViewport);
    vpc_history->set_size(Vector2i(fog_resolution, fog_resolution));

    // 重要：关闭透明，让它有一个默认的黑色背景
    vpc_history->set_transparent_background(true);

    // 重要：绝对不要在 history 里放 ColorRect 或者任何背景节点！
    // 因为 CLEAR_MODE_NEVER 会导致每一帧都在上一帧的基础上画。
    // 如果有背景节点，它每一帧都会把上一帧覆盖掉。
    vpc_history->set_clear_mode(SubViewport::CLEAR_MODE_NEVER);
    vpc_history->set_update_mode(SubViewport::UPDATE_ALWAYS);
    add_child(vpc_history);

    // 给历史层也加个相机，确保坐标对齐
    Camera2D* cam_hist = memnew(Camera2D);
    cam_hist->set_anchor_mode(Camera2D::ANCHOR_MODE_FIXED_TOP_LEFT);
    vpc_history->add_child(cam_hist);

    // 历史累加器：它是 history 里的唯一渲染节点
    Sprite2D* history_painter = memnew(Sprite2D);
    history_painter->set_texture(vpc_live->get_texture());
    history_painter->set_centered(false);

    // 设置相加材质：只把 live 里的白色圆圈“加”到 history 缓冲区
    Ref<CanvasItemMaterial> mat_add;
    mat_add.instantiate();
    mat_add->set_blend_mode(CanvasItemMaterial::BLEND_MODE_ADD);
    history_painter->set_material(mat_add);

    vpc_history->add_child(history_painter);

    // --- 初始清理 ---
    // 为了防止第一帧有乱码，我们让 history 强行清理一次成黑色
    vpc_history->set_clear_mode(SubViewport::CLEAR_MODE_ONCE);

    // 5. 创建 3D 地图覆盖层 (让地图变暗)
    fog_overlay_mesh = memnew(MeshInstance3D);
    Ref<QuadMesh> qm; qm.instantiate();
    qm->set_size(p_map_size);
    fog_overlay_mesh->set_mesh(qm);

    // 旋转并放置到地图中心
    fog_overlay_mesh->set_rotation(Vector3(-Math_PI / 2.0, 0, 0));
    fog_overlay_mesh->set_position(Vector3(p_map_pos.x + p_map_size.x / 2.0, 100.0, p_map_pos.y + p_map_size.y / 2.0));

    // 设置材质
    Ref<ShaderMaterial> mat; mat.instantiate();
    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shader/fog_shader.gdshader");
    mat->set_shader(shader);
    mat->set_shader_parameter("tex_live", vpc_live->get_texture());
    mat->set_shader_parameter("tex_history", vpc_history->get_texture());
    fog_overlay_mesh->set_material_override(mat);

    fog_overlay_mesh->set_layer_mask(3);

    add_child(fog_overlay_mesh);
}

void FogManager::update_vision(const std::vector<Vector2>& p_positions, const std::vector<float>& p_radii) {
    int count = (int)p_positions.size();

    // 只有当数量变化时才重新分配内存，避免每帧分配产生的开销
    if (vision_multimesh->get_instance_count() != count) {
        vision_multimesh->set_instance_count(count);
    }

    if (count == 0) return;

    for (int i = 0; i < count; ++i) {
        // 1. 计算在 Viewport (0~1024) 中的坐标
        // 假设 p_positions 是世界坐标 (例如 0~200)
        Vector2 uv = (p_positions[i] - map_pos) / map_size;
        Vector2 pos = uv * (float)fog_resolution;

        // 2. 计算缩放
        // 注意：QuadMesh 默认大小是 1x1，所以缩放值就是直径（像素单位）
        float diameter_x = (p_radii[i] * 2.0f / map_size.x) * (float)fog_resolution;
        float diameter_y = (p_radii[i] * 2.0f / map_size.y) * (float)fog_resolution;

        // 3. 构建 Transform2D
        Transform2D xform;
        xform = xform.scaled(Vector2(diameter_x, diameter_y));
        xform.set_origin(pos);

        // 4. 应用变换 (注意下划线)
        vision_multimesh->set_instance_transform_2d(i, xform);
    }
}

Ref<Texture2D> FogManager::get_live_texture() const { return vpc_live->get_texture(); }
Ref<Texture2D> FogManager::get_history_texture() const { return vpc_history->get_texture(); }

void FogManager::set_fog_mode(FogMode p_mode) {
    fog_mode = p_mode;

    // 如果还没有 setup 过（例如 Editor 阶段改属性时），只保留状态即可
    if (!vpc_live || !global_light_rect) return;

    switch (fog_mode) {
    case FOG_NONE:
        global_light_rect->show(); // 铺上一层白底，强制全图亮起
        vpc_live->set_clear_mode(SubViewport::CLEAR_MODE_ALWAYS);
        break;
    case FOG_LIGHT:
        global_light_rect->hide();
        vpc_live->set_clear_mode(SubViewport::CLEAR_MODE_NEVER); // 从不清理，玩家走过的地方白色圆圈会一直保留
        break;
    case FOG_HEAVY:
        global_light_rect->hide();
        vpc_live->set_clear_mode(SubViewport::CLEAR_MODE_ALWAYS); // 每帧清理，仅留下当前回合画的白圈
        break;
    }
}

FogManager::FogMode FogManager::get_fog_mode() const {
    return fog_mode;
}

void FogManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_fog_mode", "mode"), &FogManager::set_fog_mode);
    ClassDB::bind_method(D_METHOD("get_fog_mode"), &FogManager::get_fog_mode);

    // 绑定至 Godot Editor: 将会出现一个下拉选单 [None, Light, Heavy]
    ADD_PROPERTY(PropertyInfo(Variant::INT, "fog_mode", PROPERTY_HINT_ENUM, "None,Light,Heavy"), "set_fog_mode", "get_fog_mode");

    ClassDB::bind_method(D_METHOD("set_brush_texture", "texture"), &FogManager::set_brush_texture);
    ClassDB::bind_method(D_METHOD("get_brush_texture"), &FogManager::get_brush_texture);

    // 这样在 Godot 编辑器的 Inspector 面板就能看到 brush_texture 槽位了
    ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "brush_texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"), "set_brush_texture", "get_brush_texture");

    BIND_ENUM_CONSTANT(FOG_NONE);
    BIND_ENUM_CONSTANT(FOG_LIGHT);
    BIND_ENUM_CONSTANT(FOG_HEAVY);
}