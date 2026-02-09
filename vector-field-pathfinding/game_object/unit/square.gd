extends CharacterBody2D
@onready var health_component: HealthComponent = $HealthComponent
@onready var VelocityComponent: Node = $VelocityComponent

var CurrentState: int = GameState.UnitState.IDLE

var TargetPosition: Vector2 = global_position
var OldTargetPositon: Vector2 = global_position

const Radius: float = 5


func _ready() -> void:
	health_component.died.connect(_on_unit_died)


func _input(event: InputEvent) -> void:
	if event is InputEventKey and event.pressed:
		if event.keycode == KEY_T:
			health_component.take_damage(10)

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


func _on_unit_died() -> void:
	queue_free()#处理单位死亡
