extends Camera3D

@export_group("Movement")
@export var base_move_speed: float = 20.0   # 基础移动速度
@export var shift_multiplier: float = 2.5   # 按住 Shift 时的加速
@export var adjust_speed_with_zoom: bool = true # 勾选后，缩放越远移速越快

@export_group("Zoom")
@export var zoom_speed: float = 100.0        # 滚轮单次缩放的量
@export var min_zoom: float = 50.0           # 最小视野（最近）
@export var max_zoom: float = 10000.0         # 最大视野（最远）
@export var lerp_speed: float = 10.0        # 平滑速度

var _target_zoom: float = 400.0

func _ready():
	# 确保初始状态正确
	projection = ProjectionType.PROJECTION_ORTHOGONAL
	size = _target_zoom
	_target_zoom = size

func _process(delta: float):
	handle_movement(delta)
	handle_zoom(delta)

func handle_movement(delta: float):
	# 使用你的输入映射获取方向向量
	# get_vector 会自动处理斜向移动的速度规范化
	var input_vec = Input.get_vector("camera_left", "camera_right", "camera_up", "camera_down")
	var move_dir = Vector3(input_vec.x, 0, input_vec.y)
	
	if move_dir.length_squared() > 0:
		var current_speed = base_move_speed
		
		# 如果开启了速度随缩放调整
		if adjust_speed_with_zoom:
			# 逻辑：视野越大（size越大），移速线性增加
			current_speed *= (size / 20.0) 
			
		if Input.is_key_pressed(KEY_SHIFT):
			current_speed *= shift_multiplier
			
		# 更新坐标 (XZ 平面)
		global_position += move_dir * current_speed * delta

func handle_zoom(delta: float):
	# 平滑缩放
	size = lerp(size, _target_zoom, lerp_speed * delta)

func _unhandled_input(event: InputEvent):
	# 滚轮缩放逻辑
	if event is InputEventMouseButton and event.is_pressed():
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_target_zoom -= zoom_speed
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_target_zoom += zoom_speed
		
		# 限制范围
		_target_zoom = clamp(_target_zoom, min_zoom, max_zoom)

func get_mouse_world_pos() -> Vector2:
	var mouse_pos = get_viewport().get_mouse_position()
	var ray_origin = project_ray_origin(mouse_pos)
	var ray_dir = project_ray_normal(mouse_pos)
	
	# 因为相机是正交的且地面在 Y=0
	# 计算射线与 Y=0 平面的交点：t = -origin.y / dir.y
	var t = -ray_origin.y / ray_dir.y
	var world_pos = ray_origin + ray_dir * t
	return Vector2(world_pos.x, world_pos.z)
