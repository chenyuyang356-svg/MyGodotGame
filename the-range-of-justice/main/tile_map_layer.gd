extends TileMapLayer

func _ready() -> void:
	var CellSize: Vector2i = tile_set.tile_size
	var UsedRect: Rect2i = get_used_rect()
	var Width: int = UsedRect.size.x
	var Height: int = UsedRect.size.y
	var GridOrigin: Vector2i = UsedRect.position
	GlobalUnitManager.setup_system(Width, Height, CellSize, GridOrigin)
