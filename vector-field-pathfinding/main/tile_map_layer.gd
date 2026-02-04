extends TileMapLayer

func _ready() -> void:
	var Size: Vector2i = tile_set.tile_size
	var Rectangle: Rect2i = get_used_rect()
	GameState.emit_signal("TileMapLayerLoaded", Size, Rectangle)
