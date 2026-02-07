extends Node
#需要添加一个向量场列表

#查找时Dictionary可能有效率问题，最好换成PackedInt(Vector2)Array
#IntegartionFields的keys就是使用中的TargetPositions，目前通过TargetPosition管理集群
var IntegrationFields: Dictionary[Vector2, Dictionary]
var VectorFields: Dictionary[Vector2, Dictionary]
var UnitGrid: Dictionary[Vector2i, Array]
#AssemblingGroups只作为一个集合，它的键值不会被使用
var AssemblingGroups: Dictionary[Vector2, bool]
var AssemblingStates: Dictionary[Vector2, bool]

var TileSize: Vector2i
var UsedRectangle: Rect2i
var SeparationForceLimit: float = 1000

enum UnitState {IDLE, MOVING, ASSEMBLING}


func _ready() -> void:
	pass


func _physics_process(delta: float) -> void:
	UpdateUnitGrid.call_deferred()
	RemoveFields.call_deferred()
	UpdateAssemblingGroups.call_deferred()


func UpdateUnitGrid():
	UnitGrid.clear()
	var Units: Array = get_tree().get_nodes_in_group("Unit")
	for Unit: Node2D in Units:
		var GridPosition: Vector2i = Vec2toVec2i(Unit.global_position)
		if not UnitGrid.has(GridPosition):
			UnitGrid[GridPosition] = []
		UnitGrid[GridPosition].append(Unit)


func RemoveFields():
	var Units: Array = get_tree().get_nodes_in_group("Unit")
	var OldTargetPositions: Array = IntegrationFields.keys()
	var NewTargetPositions: Array = []
	for Unit: Node2D in Units:
		if not Unit.TargetPosition in NewTargetPositions:
			NewTargetPositions.append(Unit.TargetPosition)
	for TargetPosition: Vector2 in OldTargetPositions:
		if not TargetPosition in NewTargetPositions:
			IntegrationFields.erase(TargetPosition)
			VectorFields.erase(TargetPosition) 


func UpdateAssemblingGroups():
	var OldAssemblingGroups: Array = AssemblingStates.keys()
	for TargetPosition: Vector2 in OldAssemblingGroups:
		if TargetPosition in IntegrationFields.keys():
			AssemblingStates[TargetPosition] = false
		else:
			AssemblingStates.erase(TargetPosition)
	for TargetPostion: Vector2 in AssemblingGroups:
		AssemblingStates[TargetPostion] = true
	AssemblingGroups.clear()


func GetIntegration(Unit: Node2D):
	var IntegrationField: Dictionary = IntegrationFields[Unit.TargetPosition]
	return IntegrationField[Vec2toVec2i(Unit.global_position)]


func GetFlowForce(Unit: Node2D):
	var VectorField: Dictionary = VectorFields[Unit.TargetPosition]
	var FlowForce: Vector2 = VectorField[Vec2toVec2i(Unit.global_position)]
	return FlowForce


func GetSeparationForce(Unit: Node2D):
	var IsIdle: bool = (Unit.CurrentState == UnitState.IDLE)
	var SeparationForce: Vector2 = Vector2.ZERO
	var GridCoords: Vector2i = Vec2toVec2i(Unit.global_position)
	for NearbyUnit: Node2D in GetNearbyUnit(GridCoords, 1):
		var RadiusVector: Vector2 = NearbyUnit.global_position - Unit.global_position
		if RadiusVector.length_squared() == 0:
				continue
		if IsIdle:
			if NearbyUnit.CurrentState == UnitState.IDLE:
				SeparationForce += -RadiusVector / (RadiusVector.length_squared() ** 2)
			else:
				SeparationForce += -2 * RadiusVector / RadiusVector.length_squared()
		else:
			if NearbyUnit.CurrentState == UnitState.IDLE:
				SeparationForce += -.5 * RadiusVector / RadiusVector.length_squared()
			else:
				SeparationForce += -RadiusVector / RadiusVector.length_squared()
	SeparationForce = SeparationForce.limit_length(SeparationForceLimit)
	return SeparationForce


func GetAttractionForce(Unit: Node2D):
	var AttractionForce: Vector2 = Vector2.ZERO
	var GridCoords: Vector2i = Vec2toVec2i(Unit.global_position)
	for NearbyUnit: Node2D in GetNearbyUnit(GridCoords, 1):
		if NearbyUnit.CurrentState != UnitState.ASSEMBLING or NearbyUnit == Unit:
			continue
		var RadiusVector: Vector2 = NearbyUnit.global_position - Unit.global_position
		AttractionForce += RadiusVector
	return AttractionForce


func GetFritionForce(Unit: Node2D):
	return (-Unit.velocity)  


func Vec2toVec2i(CoordsVec2: Vector2):
	var IntX: int = floor(CoordsVec2.x / TileSize.x)
	var IntY: int = floor(CoordsVec2.y / TileSize.y)
	return Vector2i(IntX, IntY)


func GetNearbyUnit(GridCoords: Vector2i, Radius: int):
	var Units: Array = []
	for X in range(-Radius, Radius + 1):
		for Y in range(-Radius, Radius + 1):
			var CheckPosition: Vector2i = GridCoords + Vector2i(X, Y)
			if UnitGrid.has(CheckPosition):
				for Unit: Node2D in UnitGrid[CheckPosition]:
					Units.append(Unit)
	return Units
