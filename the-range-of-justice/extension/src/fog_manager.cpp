#include "fog_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

FogManager::FogManager() {}
FogManager::~FogManager() {}

void FogManager::setup_fog(Vector2 p_map_pos, Vector2 p_map_size, Ref<Texture2D> p_brush_texture) {
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
    vision_renderer->set_texture(p_brush_texture); // 这是一个带有羽化边缘的白色圆形贴图
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
    fog_overlay_mesh->set_position(Vector3(p_map_pos.x + p_map_size.x / 2.0, 1000.0, p_map_pos.y + p_map_size.y / 2.0));

    // 设置材质
    Ref<ShaderMaterial> mat; mat.instantiate();
    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shader/fog_shader.gdshader");
    mat->set_shader(shader);
    mat->set_shader_parameter("tex_live", vpc_live->get_texture());
    mat->set_shader_parameter("tex_history", vpc_history->get_texture());
    fog_overlay_mesh->set_material_override(mat);

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

void FogManager::_bind_methods() {}