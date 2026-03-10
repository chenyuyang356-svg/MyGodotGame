#pragma once

#include <vector>

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/sub_viewport.hpp>
#include <godot_cpp/classes/multi_mesh_instance2d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/camera2d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/placeholder_texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/viewport_texture.hpp>
#include <godot_cpp/classes/color_rect.hpp>
#include <godot_cpp/classes/canvas_item_material.hpp>

namespace godot {

    class FogManager : public Node3D {
        GDCLASS(FogManager, Node3D)

    private:
        // Viewports
        SubViewport* vpc_live = nullptr;
        SubViewport* vpc_history = nullptr;

        // 2D 渲染组件 (用于在 Viewport 里画圆)
        MultiMeshInstance2D* vision_renderer = nullptr;
        Ref<MultiMesh> vision_multimesh;

        // 3D 视觉覆盖层
        MeshInstance3D* fog_overlay_mesh = nullptr;

        Vector2 map_size;
        Vector2 map_pos;
        int fog_resolution = 512; // 贴图分辨率

    protected:
        static void _bind_methods();

    public:
        FogManager();
        ~FogManager();

        void setup_fog(Vector2 p_map_pos, Vector2 p_map_size, Ref<Texture2D> p_brush_texture);

        // 每帧调用：传入所有拥有视野的单位坐标和半径
        void update_vision(const std::vector<Vector2>& p_positions, const std::vector<float>& p_radii);

        Ref<Texture2D> get_live_texture() const;
        Ref<Texture2D> get_history_texture() const;
        Vector2 get_map_size() const { return map_size; }
    };

}