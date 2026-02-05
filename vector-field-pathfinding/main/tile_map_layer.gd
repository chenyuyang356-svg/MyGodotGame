extends TileMapLayer

func _ready() -> void:
	GameState.TileSize = tile_set.tile_size
	GameState.UsedRectangle = get_used_rect()
