#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace godot {

    class FogManager : public Node3D {
        GDCLASS(FogManager, Node3D)

    public:
        enum FogMode { MODE_NONE = 0, MODE_LIGHT = 1, MODE_HEAVY = 2 };

    private:
        // 渲染组件
        MeshInstance3D* fog_mesh_instance = nullptr;
        Ref<ImageTexture> fog_texture;
        Ref<Image> fog_image;
        Ref<ShaderMaterial> fog_material;

        // 迷雾数据
        int map_width = 0;
        int map_height = 0;
        Vector2i cell_size;
        Vector2i origin_pos;
        FogMode current_mode = MODE_HEAVY;
        PackedByteArray grid_data; // 存储每个格子的状态: 0=未探索, 127=已探索, 255=当前可见

    protected:
        static void _bind_methods();

    public:
        FogManager();
        ~FogManager();

        // 在游戏初始化时调用
        void setup(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin_pos);

        // 每帧更新迷雾贴图
        void update_fog();

        void set_fog_mode(int p_mode);
    };

}