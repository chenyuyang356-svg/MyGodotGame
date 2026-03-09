#pragma once

#include "fog_manager.h"

using namespace godot;

FogManager::FogManager() {}
FogManager::~FogManager() {}

void FogManager::setup(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin_pos) {
    map_width = p_width;
    map_height = p_height;
    cell_size = p_cell_size;
    origin_pos = p_origin_pos;

    // 1. 创建 Image 和 Texture (迷雾的数据源)
    fog_image = Image::create(map_width, map_height, false, Image::FORMAT_L8); // L8 表示单通道灰度
    grid_data.resize(map_width * map_height);
    grid_data.fill(0); // 初始全部为黑

    fog_texture = ImageTexture::create_from_image(fog_image);

    // 2. 动态创建 MeshInstance3D
    fog_mesh_instance = memnew(MeshInstance3D);
    add_child(fog_mesh_instance);

    // 3. 设置 Mesh (一个巨大的平面)
    Ref<QuadMesh> mesh;
    mesh.instantiate();
    Vector2 world_size = Vector2(p_width * p_cell_size.x, p_height * p_cell_size.y);
    Vector2 center_world_pos = Vector2(p_cell_size * p_origin_pos) + world_size / 2;
    mesh->set_size(world_size);
    mesh->set_orientation(PlaneMesh::FACE_Y); // 水平铺在地面上
    fog_mesh_instance->set_mesh(mesh);

    // 4. 设置位置 (居中对齐到地图)
    fog_mesh_instance->set_position(Vector3(center_world_pos.x, 1000.0, center_world_pos.y)); // 高度设为1.0避免闪烁

    // 5. 设置 Shader 材质
    fog_material.instantiate();
    Ref<Shader> shader = ResourceLoader::get_singleton()->load("res://shader/fog_shader.gdshader");

    fog_material->set_shader(shader);
    fog_material->set_shader_parameter("fog_data_tex", fog_texture);
    fog_mesh_instance->set_material_override(fog_material);
}

void FogManager::update_fog() {
    fog_image->set_data(map_width, map_height, false, Image::FORMAT_L8, grid_data);
    fog_texture->update(fog_image);
}

void FogManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup", "width", "height", "cell_size", "grid_origin"), &FogManager::setup);
}