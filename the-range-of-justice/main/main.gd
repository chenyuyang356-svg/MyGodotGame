extends Node

func _ready() -> void:
	var tile_map_layer: TileMapLayer = $TileMapLayer
	var multi_mesh_instance_2d: MultiMeshInstance2D = $MultiMeshInstance2D
	var unit_manager: UnitManager = $UnitManager
	var building_manager: BuildingManager = $BuildingManager
	var flow_field_manager: FlowFieldManager = $FlowFieldManager
	var selection_manager: SelectionManager = $SelectionManager
	var game_manager: GameManager = $GameManager
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	
	game_manager.set_building_manager(building_manager)
	game_manager.set_unit_manager(unit_manager)
	game_manager.set_flow_field_manager(flow_field_manager)
	game_manager.set_selection_manager(selection_manager)
	game_manager.set_multimesh_instance(multi_mesh_instance_2d)
	
	game_manager.setup_system(width, height, cell_size, grid_origin)
	
	unit_manager.register_unit_type("Fighter", "res://config/unit/fighter.txt")
	
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var coords: Vector2i = Vector2i(x, y)
			var data = tile_map_layer.get_cell_tile_data(coords)
			if data == null or data.get_custom_data("IsWall"):
				flow_field_manager.set_cost(coords, 255)
			else:
				for dx in range(-1, 2):
					for dy in range(-1, 2):
						if Vector2i(dx, dy) != Vector2i.ZERO:
							var neighbor_data = tile_map_layer.get_cell_tile_data(coords + Vector2i(dx, dy))
							if neighbor_data == null or neighbor_data.get_custom_data("IsWall"):
								flow_field_manager.set_cost(coords, 30)
								continue
				flow_field_manager.set_cost(coords, 1)
	
	for x in range(30):
		for y in range(30):
			unit_manager.spawn_unit_by_type("Fighter", -32 * Vector2(x, y))
	
	
