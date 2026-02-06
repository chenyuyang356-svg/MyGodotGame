extends Node

@onready var AssemblingTimer: Timer = $AssemblingTimer

const DesiredIntegration: int = 2
const FlowForceFactor: float = 2000
const AssemblingFlowForceFactor: float = 500
const SeparationForceFactor: float = 10000
#以后吸引力因子会是变量，用于调节不同半径单位间的平衡
const AttractionForceFactor: float = 20
const FrictionForceFactor: float = 100
const VarianceThreshold: float = .1

var MaxSpeed: float = 100
var RecentPositions: Array[Vector2] = []
var FinishAssembling: bool = false


func _ready() -> void:
	AssemblingTimer.timeout.connect(_on_assembling_timer_timeout, 1)


func _on_assembling_timer_timeout():
	var Variance: float = GetVariance(RecentPositions)
	if Variance < VarianceThreshold:
		FinishAssembling = true
		AssemblingTimer.stop()
	RecentPositions.clear()


func UpdateVelocity(Unit: CharacterBody2D, delta: float):
	match Unit.CurrentState:
		GameState.UnitState.IDLE:
			HandleIdle(Unit, delta)
		GameState.UnitState.MOVING:
			HandleMoving(Unit, delta)
		GameState.UnitState.ASSEMBLING:
			HandleAssembling(Unit, delta)


func HandleIdle(Unit: CharacterBody2D, delta: float):
	Unit.velocity = Vector2.ZERO


func HandleMoving(Unit: CharacterBody2D, delta: float):
	if GameState.GetIntegration(Unit) < DesiredIntegration:
		Unit.velocity = Vector2.ZERO
		Unit.ChangeStateTo(GameState.UnitState.ASSEMBLING)
	var Force: Vector2 = GetMovingForce(Unit)
	var Acceleration: Vector2 = Force
	Unit.velocity = Unit.velocity + Acceleration * delta
	Unit.velocity = Unit.velocity.limit_length(MaxSpeed)


func HandleAssembling(Unit: CharacterBody2D, delta: float):
	if FinishAssembling:
		if (GameState.AssemblingStates).has(Unit.TargetPosition):
			if not (GameState.AssemblingStates)[Unit.TargetPosition]:
				Unit.ChangeStateTo(GameState.UnitState.IDLE)
		Unit.velocity = Vector2.ZERO
		return
	
	if not (GameState.AssemblingGroups).has(Unit.TargetPosition):
		(GameState.AssemblingGroups).append(Unit.TargetPosition)
	
	RecentPositions.append(Unit.global_position)
	var Force: Vector2 = GetAssemblingForce(Unit)
	var Acceleration: Vector2 = Force
	Unit.velocity = Unit.velocity + Acceleration * delta
	Unit.velocity = Unit.velocity.limit_length(MaxSpeed)


func GetMovingForce(Unit: CharacterBody2D):
	var Force: Vector2 = Vector2.ZERO
	var FlowForce = GameState.GetFlowForce(Unit)
	var SeparationForce = GameState.GetSeparationForce(Unit)
	Force = FlowForce * FlowForceFactor +\
	 SeparationForce * SeparationForceFactor
	return Force


func GetAssemblingForce(Unit: CharacterBody2D):
	var Force: Vector2 = Vector2.ZERO
	var SeparationForce: Vector2 = GameState.GetSeparationForce(Unit)
	var FlowForce: Vector2 = GameState.GetFlowForce(Unit)
	var AttractionForce: Vector2 = GameState.GetAttractionForce(Unit)
	var FrictionForce: Vector2 = GameState.GetFritionForce(Unit)
	Force = AttractionForce * AttractionForceFactor +\
	 FrictionForce * FrictionForceFactor +\
	 SeparationForce * SeparationForceFactor +\
	 FlowForce * AssemblingFlowForceFactor
	return Force


func GetVariance(Positions: Array[Vector2]):
	var AveragePosition: Vector2 = GetAveragePosition(Positions)
	var Variance: float = 0
	for Position: Vector2 in Positions:
		Variance += Position.distance_squared_to(AveragePosition)
	Variance = Variance / Positions.size()
	return Variance


func GetAveragePosition(Positions: Array[Vector2]):
	var AveragePosition: Vector2 = Vector2.ZERO
	for Position: Vector2 in Positions:
		AveragePosition += Position
	AveragePosition = AveragePosition / Positions.size()
	return AveragePosition
