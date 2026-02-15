extends Control
@export var next_scene = "res://main/main.tscn"

func on_start_button_pressed():
	get_tree().change_scene_to_file(next_scene)

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
