#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include "game_definitions.h"
#include <godot_cpp/classes/tile_map_layer.hpp>
#include <godot_cpp/classes/tile_set.hpp>
#include <godot_cpp/classes/tile_set_atlas_source.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>

namespace godot {

    class MapManager : public Node3D {
        GDCLASS(MapManager, Node3D)

    private:
        MultiMeshInstance3D* map_renderer = nullptr;
        Ref<Shader> map_shader;

    protected:
        static void _bind_methods();

    public:
        MapManager();
        ~MapManager();

        // 核心函数：将 2D 地图层转化为 3D MultiMesh
        void load_from_tilemap(Node* p_tilemap_layer_node);
    };

}