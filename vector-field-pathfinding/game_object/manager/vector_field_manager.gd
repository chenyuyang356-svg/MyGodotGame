extends Node2D
#这个节点已被弃用
@export var Tile: TileMapLayer
@onready var TileRectangle: Rect2i = Tile.get_used_rect()
@onready var TileSize: Vector2i = Tile.tile_set.tile_size
#最近更新的CostField，IntegrationField，VectorField
#CostField和IntegrationField，65535表示不可到达
var CostField: Dictionary[Vector2i, int] = {}
var IntegrationField: Dictionary[Vector2i, int] = {}
var VectorField: Dictionary[Vector2i, Vector2] = {}

func _ready() -> void:
	GameEvent.choosing_target_position.connect(_on_choosing_target_position)


func _draw() -> void:
	pass


func _on_choosing_target_position(TargetPosition: Vector2):
	(GameState.IntegrationFields)[TargetPosition] = IntegrationField.duplicate()
	(GameState.VectorFields)[TargetPosition] = VectorField.duplicate()


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
