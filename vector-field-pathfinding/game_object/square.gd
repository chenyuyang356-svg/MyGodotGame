extends CharacterBody2D

@onready var SelectionArea: Area2D = $SelectionArea

var CurrentState: int = GameState.UnitState.IDLE

var IsMouseOn: bool = false
var IsSelected: bool = false

var TargetPosition: Vector2 = global_position
var OldTargetPositon: Vector2 = global_position
var DesiredIntegration: int = 2

var Radius: float = 5
var Speed: float = 100
var FlowForceFactor: float = 2000
var AssemblingFlowForceFactor: float = 500
var SeparationForceFactor: float = 10000
var AttractionForceFactor: float = 20
var FrictionForceFactor: float = 100


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
		GameState.UnitState.IDLE:
			HandleIdle()
		GameState.UnitState.MOVING:
			HandleMoving(delta)
		GameState.UnitState.ASSEMBLING:
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
		RestoreModulate()


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
		ChangeStateTo(GameState.UnitState.MOVING)


func ChangeIsSelectedState():
	IsSelected = !IsSelected
	RestoreModulate()


func ChangeStateTo(NewState: int):
	CurrentState = NewState


func HandleIdle():
	velocity = Vector2.ZERO


func HandleMoving(delta: float):
	if GameState.GetIntegration(self) < DesiredIntegration:
		velocity = Vector2.ZERO
		ChangeStateTo(GameState.UnitState.ASSEMBLING)
	var Force: Vector2 = GetMovingForce()
	UpdateVelocity(delta, Force)
	move_and_slide()


func HandleAssembling(delta: float):
	var Force: Vector2 = GetAssemblingForce()
	UpdateVelocity(delta, Force)
	move_and_slide()


func GetMovingForce():
	var Force: Vector2 = Vector2.ZERO
	var FlowForce = GameState.GetFlowForce(self)
	var SeparationForce = GameState.GetSeparationForce(self)
	Force = FlowForce * FlowForceFactor +\
	 SeparationForce * SeparationForceFactor
	return Force


func GetAssemblingForce():
	var Force: Vector2 = Vector2.ZERO
	var SeparationForce: Vector2 = GameState.GetSeparationForce(self)
	var FlowForce: Vector2 = GameState.GetFlowForce(self)
	var AttractionForce: Vector2 = GameState.GetAttractionForce(self)
	var FrictionForce: Vector2 = GameState.GetFritionForce(self)
	Force = AttractionForce * AttractionForceFactor +\
	 FrictionForce * FrictionForceFactor +\
	 SeparationForce * SeparationForceFactor +\
	 FlowForce * AssemblingFlowForceFactor
	return Force


func UpdateVelocity(delta: float, Force: Vector2):
	var Acceleration: Vector2 = Force
	velocity = velocity + Acceleration * delta
	velocity = velocity.limit_length(Speed)


func RestoreModulate():
	if IsSelected:
		modulate = Color(1.2, 1.2, 1.2)
	else:
		modulate = Color(1.0, 1.0, 1.0)
