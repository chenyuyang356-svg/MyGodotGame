extends CharacterBody2D

@onready var VelocityComponent: Node = $VelocityComponent

var CurrentState: int = GameState.UnitState.IDLE

var TargetPosition: Vector2 = global_position
var OldTargetPositon: Vector2 = global_position

const Radius: float = 5


func _ready() -> void:
	pass


func _process(delta: float) -> void:
	pass


func _physics_process(delta: float) -> void:
	VelocityComponent.UpdateVelocity(self, delta)
	move_and_slide()


func ChangeStateTo(NewState: int):
	ExistState(CurrentState)
	EnterState(NewState)


func ExistState(OldState: int):
	match OldState:
		GameState.UnitState.IDLE:
			return
		GameState.UnitState.MOVING:
			return
		GameState.UnitState.ASSEMBLING:
			return


func EnterState(NewState: int):
	CurrentState = NewState
	match CurrentState:
		GameState.UnitState.IDLE:
			return 
		GameState.UnitState.MOVING:
			return
		GameState.UnitState.ASSEMBLING:
			VelocityComponent.FinishAssembling = false
			VelocityComponent.AssemblingTimer.start()
