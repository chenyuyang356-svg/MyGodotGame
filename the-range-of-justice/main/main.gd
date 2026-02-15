extends Node

func _ready() -> void:
	var tile_map_layer: TileMapLayer = $TileMapLayer
	var multi_mesh_instance_2d: MultiMeshInstance2D = $MultiMeshInstance2D
	var unit_manager: UnitManager = $UnitManager
	var flow_field_manager: FlowFieldManager = $FlowFieldManager
	var selection_manager: SelectionManager = $SelectionManager
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	
	unit_manager.set_multimesh_instance(multi_mesh_instance_2d)
	
	unit_manager.set_flow_field_manager(flow_field_manager)	
	unit_manager.set_selection_manager(selection_manager)
	
	unit_manager.setup_system(width, height, cell_size, grid_origin)
	
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var coords: Vector2i = Vector2i(x, y)
			var data = tile_map_layer.get_cell_tile_data(coords)
			if data == null or data.get_custom_data("IsWall"):
				flow_field_manager.set_cost(coords, 10)
	
	for x in range(40):
		for y in range(40):
			unit_manager.spawn_unit(Vector2(-16 * x, -16 * y), unit_manager.SQUARE)
	
	
