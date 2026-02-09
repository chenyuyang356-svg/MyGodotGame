extends TileMapLayer

func _ready() -> void:
	var TileSize: Vector2i = tile_set.tile_size
	var UsedRectangle: Rect2i = get_used_rect()
	GameState.TileSize = TileSize
	GameState.UsedRectangle = UsedRectangle
	GlobalFlowFieldManager.setup_grid(UsedRectangle.size.x, UsedRectangle.size.y, UsedRectangle.position, TileSize)
	
	##将地图信息传递给流场管理器
	for x in range(UsedRectangle.position.x, UsedRectangle.end.x):
		for y in range(UsedRectangle.position.y, UsedRectangle.end.y):
			var Coords: Vector2i = Vector2i(x, y)
			var Data = get_cell_tile_data(Coords)
			if Data == null or Data.get_custom_data("IsWall"):
				GlobalFlowFieldManager.set_cost(Coords, 255)
