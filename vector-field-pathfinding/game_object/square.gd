extends CharacterBody2D

@onready var SelectionArea: Area2D = $SelectionArea

enum UnitState {IDLE, MOVING, ASSEMBLING}
var CurrentState: int = UnitState.IDLE

var IsMouseOn: bool = false
var IsSelected: bool = false

var TargetPosition: Vector2 = global_position
var OldTargetPositon: Vector2 = global_position
var DeisredDistance: float = 10

var Radius: float = 5
var Speed: float = 100
var FlowForceFactor: float = 2000
var SeperationForecFactor: float = 10000
var DampingForceFactor: float
var FrictionForceFactor: float


func _ready() -> void:
	SelectionArea.mouse_entered.connect(_on_mouse_entered)
	SelectionArea.mouse_exited.connect(_on_mouse_exited)
	GameEvent.single_selecting.connect(_on_single_selecting, 1)
	GameEvent.double_click_selecting.connect(_on_double_click_selecting, 1)
	GameEvent.choosing_target_position.connect(_on_choosing_target_position, 1)
	GameEvent.is_box_selecting.connect(_on_is_box_selecting, 1)
	GameEvent.end_box_selecting.connect(_on_end_box_selecting, 1)


func _process(delta: float) -> void:
	pass


func _physics_process(delta: float) -> void:
	match CurrentState:
		UnitState.IDLE:
			HandleIdle()
		UnitState.MOVING:
			HandleMoving(delta)
		UnitState.ASSEMBLING:
			HandleAssembling(delta)


func _on_mouse_entered():
	print("Mouse is on!")
	IsMouseOn = true
	modulate = Color(1.5, 1.5, 1.5)


func _on_mouse_exited():
	print("Mouse isn't on!")
	IsMouseOn = false
	RestoreModulate()


func _on_single_selecting(Index: int):
	if get_index() == Index:
		ChangeIsSelectedState()
	elif IsSelected:
		ChangeIsSelectedState()


func _on_double_click_selecting(GroupName: StringName):
	if GroupName in get_groups():
		if !IsSelected:
			ChangeIsSelectedState()
	elif IsSelected:
		ChangeIsSelectedState() 


func _on_is_box_selecting(Indexes: Array):
	if get_index() in Indexes:
		modulate = Color(1.5, 1.5, 1.5)
	else:
		modulate = Color(1.0, 1.0, 1.0)


func _on_end_box_selecting(Indexes: Array):
	if get_index() in Indexes:
		if !IsSelected:
			ChangeIsSelectedState()
		else:
			RestoreModulate()
	elif IsSelected:
		ChangeIsSelectedState()


func _on_choosing_target_position(MousePosition: Vector2):
	if IsSelected:
		TargetPosition = MousePosition
		ChangeStateTo(UnitState.MOVING)


func ChangeIsSelectedState():
	IsSelected = !IsSelected
	RestoreModulate()


func ChangeStateTo(NewState: int):
	CurrentState = NewState


func HandleIdle():
	velocity = Vector2.ZERO


func HandleMoving(delta: float):
	var Force: Vector2 = GetForce()
	UpdateVelocity(delta, Force)
	move_and_slide()


func HandleAssembling(delta: float):
	pass


func GetForce():
	var Force: Vector2
	var FlowForce = GameState.GetFlowForce(global_position)
	var SeparationForce = GameState.GetSeparationForce(global_position)
	print(get_index(), FlowForce * FlowForceFactor, SeparationForce * SeperationForecFactor)
	Force = FlowForce * FlowForceFactor + SeparationForce * SeperationForecFactor
	return Force


func UpdateVelocity(delta: float, Force: Vector2):
	if global_position.distance_squared_to(TargetPosition) < DeisredDistance ** 2:
		velocity = Vector2.ZERO
		ChangeStateTo(UnitState.IDLE)
	else:
		var Acceleration: Vector2 = Force
		velocity = velocity + Acceleration * delta
		velocity = velocity.limit_length(Speed)


func RestoreModulate():
	if IsSelected:
		modulate = Color(1.2, 1.2, 1.2)
	else:
		modulate = Color(1.0, 1.0, 1.0)
