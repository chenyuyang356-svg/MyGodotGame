extends Node
#需要添加一个向量场列表

#查找时Dictionary可能有效率问题，最好换成PackedInt(Vector2)Array
signal IntegrationFieldUpdated(IntField: Dictionary)
signal VectorFieldUpdated(VecField: Dictionary)
signal TileMapLayerLoaded(Size: Vector2i, Rectangle: Rect2i)

var IntegrationField: Dictionary[Vector2i, int]
var VectorField: Dictionary[Vector2i, Vector2]
var UnitGrid: Dictionary[Vector2i, Array]

var TileSize: Vector2i
var UsedRectangle: Rect2i
var SeparationForceLimit: float = 1000


func _ready() -> void:
	IntegrationFieldUpdated.connect(_on_integration_field_updated)
	VectorFieldUpdated.connect(_on_vector_field_updated)
	TileMapLayerLoaded.connect(_on_tile_map_layer_loaded)


func _physics_process(delta: float) -> void:
	UpdateUnitGrid.call_deferred()


func _on_tile_map_layer_loaded(Size: Vector2i, Rectangle: Rect2i):
	TileSize = Size
	UsedRectangle = Rectangle


func _on_integration_field_updated(IntField: Dictionary):
	IntegrationField = IntField


func _on_vector_field_updated(VecField: Dictionary):
	VectorField = VecField


func UpdateUnitGrid():
	UnitGrid.clear()
	var Units: Array = get_tree().get_nodes_in_group("Unit")
	for Unit: Node2D in Units:
		var GridPosition: Vector2i = Vec2toVec2i(Unit.global_position)
		if not UnitGrid.has(GridPosition):
			UnitGrid[GridPosition] = []
		UnitGrid[GridPosition].append(Unit)


func GetFlowForce(Coords: Vector2):
	return VectorField[Vec2toVec2i(Coords)]


func GetSeparationForce(Coords: Vector2):
	var SeparationForce: Vector2 = Vector2.ZERO
	var GridCoords: Vector2i = Vec2toVec2i(Coords)
	for Unit: Node2D in GetNearbyUnit(GridCoords):
		var RadiusVector: Vector2 = Unit.global_position - Coords
		if RadiusVector.length_squared() == 0:
				continue
		SeparationForce += -RadiusVector / RadiusVector.length_squared()
	SeparationForce = SeparationForce.limit_length(SeparationForceLimit)
	return SeparationForce


func GetDampingForce(Coords: Vector2, Radius: float):
	var DampingForce: Vector2 = Vector2.ZERO
	var GridCoords: Vector2i = Vec2toVec2i(Coords)
	for Unit: Node2D in GetNearbyUnit(GridCoords):
		var RadiusVector: Vector2 = Unit.global_position - Coords
		if RadiusVector.length_squared() == 0:
			continue
		var BalanceRadius: float = Radius + Unit.Radius
		var BalanceVector: Vector2 = RadiusVector.normalized() * BalanceRadius
		DampingForce += BalanceVector - RadiusVector
	return DampingForce


func GetFritionForce(Velocity: Vector2):
	return (-Velocity) 


func Vec2toVec2i(CoordsVec2: Vector2):
	var IntX: int = floor(CoordsVec2.x / TileSize.x)
	var IntY: int = floor(CoordsVec2.y / TileSize.y)
	return Vector2i(IntX, IntY)


#可能需要增加一个Radius项
func GetNearbyUnit(GridCoords: Vector2i):
	var Units: Array = []
	for X in range(-1, 2):
		for Y in range(-1, 2):
			var CheckPosition: Vector2i = GridCoords + Vector2i(X, Y)
			if UnitGrid.has(CheckPosition):
				for Unit: Node2D in UnitGrid[CheckPosition]:
					Units.append(Unit)
	return Units
