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
	var attack_maanger: AttackManager = $AttackManager
	var economy_manager: EconomyManager = $EconomyManager
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	var debug_draw: Node2D = $DebugCanvas/DebugDraw
	var main_camera: Camera3D = $Camera3D
	
	map_manager.load_from_tilemap(tile_map_layer)
	tile_map_layer.hide()
	
	game_manager.set_building_manager(building_manager)
	game_manager.set_unit_manager(unit_manager)
	game_manager.set_flow_field_manager(flow_field_manager)
	game_manager.set_selection_manager(selection_manager)
	game_manager.set_group_manager(group_manager)
	unit_manager.set_attack_manager(attack_maanger)
	game_manager.set_economy_manager(economy_manager)
	
	game_manager.setup_system(width, height, cell_size, grid_origin)
	
	unit_manager.register_unit_type("Tank", "res://config/unit/tank.txt")
	unit_manager.register_unit_type("Fighter", "res://config/unit/fighter.txt")
	unit_manager.register_unit_type("Battleship", "res://config/unit/battleship.txt")
	
	#NAV_LAND
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var coords: Vector2i = Vector2i(x, y)
			var data = tile_map_layer.get_cell_tile_data(coords)
			if data == null or data.get_custom_data("IsWall") or data.get_custom_data("IsSea"):
				flow_field_manager.init_cost(coords, 255, 0)
			else:
				flow_field_manager.init_cost(coords, 1, 0)
				for dx in range(-1, 2):
					for dy in range(-1, 2):
						if Vector2i(dx, dy) != Vector2i.ZERO:
							var neighbor_data = tile_map_layer.get_cell_tile_data(coords + Vector2i(dx, dy))
							if neighbor_data == null or neighbor_data.get_custom_data("IsWall") or data.get_custom_data("IsSea"):
								flow_field_manager.init_cost(coords, 30, 0)
								continue
	
	#NAV_SEA
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var coords: Vector2i = Vector2i(x, y)
			var data = tile_map_layer.get_cell_tile_data(coords)
			if data == null or (not data.get_custom_data("IsSea")):
				flow_field_manager.init_cost(coords, 255, 1)
			else:
				flow_field_manager.init_cost(coords, 1, 1)
				for dx in range(-1, 2):
					for dy in range(-1, 2):
						if Vector2i(dx, dy) != Vector2i.ZERO:
							var neighbor_data = tile_map_layer.get_cell_tile_data(coords + Vector2i(dx, dy))
							if neighbor_data == null or (not data.get_custom_data("IsSea")):
								flow_field_manager.init_cost(coords, 30, 1)
								continue
	
	var active_unit_ids: Array = []
	
	for x in range(2):
		for y in range(2):
			var id1 = unit_manager.spawn_unit_by_type("Fighter", -32 * Vector2(x, y), 1)
			var id2 = unit_manager.spawn_unit_by_type("Fighter", -32 * Vector2(x, y) + Vector2(3000, 3000), 2)
			var id3 = unit_manager.spawn_unit_by_type("Tank", -32 * Vector2(x, y), 1)
			unit_manager.spawn_unit_by_type("Tank", -32 * Vector2(x, y), 2)
			unit_manager.spawn_unit_by_type("Tank", -32 * Vector2(x, y), 3)
			unit_manager.spawn_unit_by_type("Tank", -32 * Vector2(x, y), 4)
			unit_manager.spawn_unit_by_type("Battleship", -32 * Vector2(x, y) + 256 * Vector2(-36, -7), 1)
			unit_manager.spawn_unit_by_type("Battleship", -32 * Vector2(x, y) + 256 * Vector2(-36, -7), 2)
			unit_manager.spawn_unit_by_type("Battleship", -32 * Vector2(x, y) + 256 * Vector2(-36, -7), 3)
			unit_manager.spawn_unit_by_type("Battleship", -32 * Vector2(x, y) + 256 * Vector2(-36, -7), 4)
			
			
			active_unit_ids.append(id1)
			active_unit_ids.append(id2)
		
	economy_manager.set_balance(1, 5000)
			
	if debug_draw != null:
		debug_draw.unit_manager = unit_manager
		debug_draw.camera = main_camera
		debug_draw.unit_ids_to_draw = active_unit_ids
	
