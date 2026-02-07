extends Node

@onready var Parent: CharacterBody2D = $"../"
@onready var SelectionArea: Area2D = $"../SelectionArea"

var IsMouseOn: bool = false
var IsSelected: bool = false


func _ready() -> void:
	SelectionArea.mouse_entered.connect(_on_mouse_entered)
	SelectionArea.mouse_exited.connect(_on_mouse_exited)
	
	GameEvent.single_selecting.connect(_on_single_selecting, 1)
	GameEvent.double_click_selecting.connect(_on_double_click_selecting, 1)
	GameEvent.choosing_target_position.connect(_on_choosing_target_position, 1)
	GameEvent.is_box_selecting.connect(_on_is_box_selecting, 1)
	GameEvent.end_box_selecting.connect(_on_end_box_selecting, 1)


func _on_mouse_entered():
	print("Mouse is on!")
	IsMouseOn = true
	Highlight()


func _on_mouse_exited():
	print("Mouse isn't on!")
	IsMouseOn = false
	RestoreModulate()


func _on_single_selecting(Index: int):
	if Parent.get_index() == Index:
		ChangeIsSelectedState()
	elif IsSelected:
		ChangeIsSelectedState()


func _on_double_click_selecting(GroupName: StringName):
	if GroupName in Parent.get_groups():
		if !IsSelected:
			ChangeIsSelectedState()
	elif IsSelected:
		ChangeIsSelectedState() 


func _on_is_box_selecting(Indexes: Array):
	if Parent.get_index() in Indexes:
		Highlight()
	else:
		RestoreModulate()


func _on_end_box_selecting(Indexes: Array):
	if Parent.get_index() in Indexes:
		if !IsSelected:
			ChangeIsSelectedState()
		else:
			RestoreModulate()
	elif IsSelected:
		ChangeIsSelectedState()


func _on_choosing_target_position(MousePosition: Vector2):
	if IsSelected:
		Parent.TargetPosition = MousePosition
		Parent.ChangeStateTo(GameState.UnitState.MOVING)


func ChangeIsSelectedState():
	IsSelected = !IsSelected
	RestoreModulate()


func Highlight():
	Parent.modulate = Color(1.5, 1.5, 1.5)


func RestoreModulate():
	if IsSelected:
		Parent.modulate = Color(1.2, 1.2, 1.2)
	else:
		Parent.modulate = Color(1.0, 1.0, 1.0)
