#include "map_manager.h"
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/tile_set.hpp>

using namespace godot;

MapManager::MapManager() {
    map_renderer = memnew(MultiMeshInstance3D);
    add_child(map_renderer);
    map_shader = ResourceLoader::get_singleton()->load("res://shader/map_tile.gdshader");
}

MapManager::~MapManager() {}

void MapManager::load_from_tilemap(Node* p_node) {
    TileMapLayer* layer = Object::cast_to<TileMapLayer>(p_node);
    if (!layer) return;

    Ref<TileSet> ts = layer->get_tile_set();
    if (ts.is_null() || ts->get_source_count() == 0) return;

    // 1. 获取 Source (先拿到基类 Ref)
    int source_id = ts->get_source_id(0);
    Ref<TileSetSource> base_source = ts->get_source(source_id);

    // 2. 这里的转换是关键：
    // 使用 .ptr() 拿到原始指针，然后 cast_to，最后用 Ref 包装
    TileSetAtlasSource* atlas_ptr = Object::cast_to<TileSetAtlasSource>(base_source.ptr());
    if (!atlas_ptr) {
        // 如果该 Source 不是 Atlas 类型（可能是场景瓦片），则跳过
        return;
    }
    Ref<TileSetAtlasSource> source(atlas_ptr);

    // --- 后续逻辑保持不变 ---
    TypedArray<Vector2i> cells = layer->get_used_cells();
    int cell_count = cells.size();

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_custom_data(true);
    mm->set_instance_count(cell_count);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();
    Vector2i tile_size = ts->get_tile_size();
    qmesh->set_size(Vector2(tile_size.x, tile_size.y));
    mm->set_mesh(qmesh);

    for (int i = 0; i < cell_count; ++i) {
        Vector2i coords = cells[i];
        Vector2i atlas_coords = layer->get_cell_atlas_coords(coords);

        Transform3D xform;
        // 注意：这里为了 RTS 视角，将 Y 坐标设为 0，Z 坐标设为 2D 的 Y
        xform.origin = Vector3((coords.x + 0.5f) * tile_size.x, -1.0f, (coords.y + 0.5f) * tile_size.y);
        xform.basis = Basis().rotated(Vector3(1, 0, 0), -Math_PI / 2.0); // 躺平

        mm->set_instance_transform(i, xform);
        mm->set_instance_custom_data(i, Color(atlas_coords.x, atlas_coords.y, 0, 0));
    }

    map_renderer->set_multimesh(mm);

    // 设置材质参数
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    mat->set_shader(map_shader);

    Ref<Texture2D> tex = source->get_texture();
    if (tex.is_valid()) {
        mat->set_shader_parameter("albedo_texture", tex);
        Vector2 tex_size = tex->get_size();
        Vector2 atlas_grid_size = tex_size / Vector2(tile_size.x, tile_size.y);
        mat->set_shader_parameter("atlas_size", atlas_grid_size);
    }
    map_renderer->set_material_override(mat);

    map_renderer->set_layer_mask(3);
}

void MapManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_from_tilemap", "tilemap_layer_node"), &MapManager::load_from_tilemap);
}