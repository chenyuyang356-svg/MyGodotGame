extends Camera2D

var ZoomRate: float = 2.0
var MaxZoomRate: float = 4.0
var MinZoomRate: float = 1.0
var Speed: float = 100

func _input(event):
	if event is InputEventMouseButton:
		match event.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				ZoomRate = clamp(ZoomRate + .1, MinZoomRate, MaxZoomRate)
			MOUSE_BUTTON_WHEEL_DOWN:
				ZoomRate = clamp(ZoomRate - .1, MinZoomRate, MaxZoomRate)
		zoom = Vector2(1, 1) * ZoomRate


func _process(delta: float) -> void:
	var Direction: Vector2
	Direction.y = Input.get_action_strength("camera_down") - Input.get_action_raw_strength("camera_up")
	Direction.x = Input.get_action_strength("camera_right") - Input.get_action_strength("camera_left")
	Direction = Direction.normalized()
	var Movement: Vector2 = Direction * delta * Speed
	global_position += Movement
