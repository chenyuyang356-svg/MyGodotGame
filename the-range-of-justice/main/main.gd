extends Node

func _ready() -> void:
	var tile_map_layer: TileMapLayer = $TileMapLayer
	var map_manager: MapManager = $MapManager
	var unit_manager: UnitManager = $UnitManager
	var building_manager: BuildingManager = $BuildingManager
	var flow_field_manager: FlowFieldManager = $FlowFieldManager
	var selection_manager: SelectionManager = $SelectionManager
	var group_manager: GroupManager = $GroupManager
	var game_manager: GameManager = $GameManager
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	
	map_manager.load_from_tilemap(tile_map_layer)
	tile_map_layer.hide()
	
	game_manager.set_building_manager(building_manager)
	game_manager.set_unit_manager(unit_manager)
	game_manager.set_flow_field_manager(flow_field_manager)
	game_manager.set_selection_manager(selection_manager)
	game_manager.set_group_manager(group_manager)
	
	game_manager.setup_system(width, height, cell_size, grid_origin)
	
	unit_manager.register_unit_type("Tank", "res://config/unit/tank.txt")
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
	
	for x in range(10):
		for y in range(10):
			unit_manager.spawn_unit_by_type("Tank", -32 * Vector2(x, y), 1)
			unit_manager.spawn_unit_by_type("Fighter", -32 * Vector2(x, y), 1)
			unit_manager.spawn_unit_by_type("Fighter", -32 * Vector2(x, y) + Vector2(3000, 3000), 2)
	
	
