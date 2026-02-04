extends Node2D

@export var Tile: TileMapLayer
@onready var TileRectangle: Rect2i = Tile.get_used_rect()
@onready var TileSize: Vector2i = Tile.tile_set.tile_size
var CostField: Dictionary[Vector2i, int]
var IntegrationField: Dictionary[Vector2i, int]
var VectorField: Dictionary[Vector2i, Vector2]

func _ready() -> void:
	GenerateCostField(Tile)
	GenerateIntegartionField(Vector2i.ZERO)
	GenerateVectorField()
	queue_redraw()
	GameEvent.choosing_target_position.connect(_on_choosing_target_position)


func _draw() -> void:
	DrawIntField(IntegrationField)
	DrawVecField(VectorField)


func _on_choosing_target_position(TargetPosition: Vector2):
	GenerateIntegartionField(Vec2toVec2i(TargetPosition))
	GenerateVectorField()
	GameState.emit_signal("IntegrationFieldUpdated", IntegrationField)
	GameState.emit_signal("VectorFieldUpdated", VectorField)
	queue_redraw()


func GenerateCostField(UsedTile: TileMapLayer):
	var UsedRectangle: Rect2i = UsedTile.get_used_rect()
	for x in range(UsedRectangle.position.x, UsedRectangle.end.x):
		for y in range(UsedRectangle.position.y, UsedRectangle.end.y):
			var Coords: Vector2i = Vector2i(x, y)
			var Data = UsedTile.get_cell_tile_data(Coords)
			if Data == null or Data.get_custom_data("IsWall"):
				CostField[Coords] = 255
			else:
				CostField[Coords] = 1


func GenerateIntegartionField(TargetPosition: Vector2i):
	for Coords in CostField:
		IntegrationField[Coords] = 65535
	if !TileRectangle.has_point(TargetPosition):
		return
	IntegrationField[TargetPosition] = 0
	var Queue: Array = [TargetPosition]
	while Queue.size() > 0:
		var CurrentPosition = Queue.pop_front()
		var CurrentDistance = IntegrationField[CurrentPosition]
		for Neighbor in GetNeighbors(CurrentPosition, false):
			if CostField[Neighbor] >= 255:
				continue
			var NewDistance: int = CurrentDistance + CostField[Neighbor]
			if NewDistance < IntegrationField[Neighbor]:
				IntegrationField[Neighbor] = NewDistance
				Queue.append(Neighbor)


func GenerateVectorField():
	for Coords: Vector2i in IntegrationField:
		var Integration: int = IntegrationField[Coords]
		if Integration <= 65535 and Integration > 0:
			var BestNeighbor: Vector2i = GetBestNeighbor(Coords)
			var Vector: Vector2 = Vector2(BestNeighbor - Coords).normalized()
			VectorField[Coords] = Vector	
		else:
			VectorField[Coords] = Vector2.ZERO


func GetNeighbors(Coords: Vector2i, IncludeDiagonal: bool):
	var Result: Array
	if Coords.x + 1 < TileRectangle.end.x:
		Result.append(Coords + Vector2i.RIGHT)
	if Coords.x - 1 >= TileRectangle.position.x:
		Result.append(Coords + Vector2i.LEFT)
	if Coords.y + 1 < TileRectangle.end.y:
		Result.append(Coords + Vector2i.DOWN)
	if Coords.y - 1 >= TileRectangle.position.y:
		Result.append(Coords + Vector2i.UP)
	if IncludeDiagonal:
		for X in [-1, 1]:
			for Y in [-1, 1]:
				var DiagonalNeighbor: Vector2i = Coords + Vector2i(X, Y)
				if TileRectangle.has_point(DiagonalNeighbor):
					Result.append(DiagonalNeighbor)
	
	return Result


func DrawIntField(IntField: Dictionary):
	for Coords in IntField:
		var Number: int = IntField[Coords]
		if Number >= 65535:
			Number = -1
		var Text: String = str(Number)
		var TextPosition: Vector2
		TextPosition.x = (Coords.x + .5) * TileSize.x
		TextPosition.y = (Coords.y + .5) * TileSize.y
		
		draw_string(ThemeDB.fallback_font, TextPosition, Text, HORIZONTAL_ALIGNMENT_CENTER, -1, 8)


func DrawVecField(VecField: Dictionary):
	for Coords in VecField:
		var Vec: Vector2 = VecField[Coords]
		if Vec.length_squared() == 0:
			continue
		var VecPosition: Vector2
		VecPosition.x = (Coords.x + .5) * TileSize.x
		VecPosition.y = (Coords.y + .5) * TileSize.y
		var VecEnd: Vector2 = VecPosition + Vec * 4
		draw_line(VecPosition, VecEnd, Color.RED)
		draw_line(VecEnd, VecEnd - 2 * Vec.rotated(deg_to_rad(20)), Color.RED)
		draw_line(VecEnd, VecEnd - 2 * Vec.rotated(-deg_to_rad(20)), Color.RED)


func GetBestNeighbor(Coords: Vector2i):
	var BestNeighbor: Vector2i = Coords
	var IsHandled: Dictionary
	for Neighbor in GetNeighbors(Coords, true):
		IsHandled[Neighbor] = false
	for Neighbor in GetNeighbors(Coords, false):
		if IntegrationField[Neighbor] >= 65535:
			if (Neighbor - Coords).x == 0:
				IsHandled[Neighbor + Vector2i.LEFT] = true
				IsHandled[Neighbor + Vector2i.RIGHT] = true 
			if (Neighbor - Coords).y == 0:
				IsHandled[Neighbor + Vector2i.UP] = true
				IsHandled[Neighbor + Vector2i.DOWN] = true
		elif IntegrationField[Neighbor] < IntegrationField[BestNeighbor]:
			BestNeighbor = Neighbor
		IsHandled[Neighbor] = true
	for Neighbor in IsHandled:
		if !IsHandled[Neighbor] and IntegrationField[Neighbor] < IntegrationField[BestNeighbor]:
			BestNeighbor = Neighbor
	
	return BestNeighbor


func Vec2toVec2i(CoordsVec2: Vector2):
	var IntX: int = floor(CoordsVec2.x / TileSize.x)
	var IntY: int = floor(CoordsVec2.y / TileSize.y)
	return Vector2i(IntX, IntY)
