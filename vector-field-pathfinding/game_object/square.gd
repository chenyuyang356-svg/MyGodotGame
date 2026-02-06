extends CharacterBody2D

@onready var VelocityComponent: Node = $VelocityComponent
@onready var SelectionArea: Area2D = $SelectionArea

var CurrentState: int = GameState.UnitState.IDLE

var IsMouseOn: bool = false
var IsSelected: bool = false

var TargetPosition: Vector2 = global_position
var OldTargetPositon: Vector2 = global_position

const Radius: float = 5


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
	VelocityComponent.UpdateVelocity(self, delta)
	move_and_slide()


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


func RestoreModulate():
	if IsSelected:
		modulate = Color(1.2, 1.2, 1.2)
	else:
		modulate = Color(1.0, 1.0, 1.0)
