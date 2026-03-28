extends SubViewportContainer

@export var main_camera: Camera3D  # 在检查器里指定主相机
@onready var minimap_camera: Camera3D = $SubViewport/MinimapCamera3D

var is_dragging: bool = false

# 地图边界（由 main.gd 初始化）
var _map_min: Vector2
var _map_max: Vector2
var _has_limit: bool = false

func _gui_input(event: InputEvent):
	if not _has_limit: return

	# 1. 处理点击和松开
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE:
			is_dragging = event.pressed
			# 消耗掉事件，防止传递给下层地图
			accept_event() 
		if event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				# 立即跳转主相机
				_teleport_main_camera(event.position)
			accept_event()

	# 2. 处理中键拖动
	if event is InputEventMouseMotion and is_dragging:
		# 平移小地图相机
		_move_minimap_camera(event.relative)
		accept_event()

# A. 移动小地图相机（内部视野平移）
func _move_minimap_camera(relative: Vector2):
	var viewport_size = get_rect().size
	# 计算像素到世界的比例 (基于小地图相机的 size)
	var world_to_pixel_ratio = minimap_camera.size / viewport_size.y
	
	minimap_camera.global_position.x -= relative.x * world_to_pixel_ratio
	minimap_camera.global_position.z -= relative.y * world_to_pixel_ratio
	
	# 限制小地图相机不超出边界
	_apply_minimap_bounds()

# B. 跳转主相机到点击位置
func _teleport_main_camera(ui_pos: Vector2):
	if not main_camera: return
	
	# 1. 将 UI 像素坐标转换为小地图视口内的百分比 (-0.5 到 0.5)
	var container_size = get_rect().size
	var pct_x = (ui_pos.x / container_size.x) - 0.5
	var pct_y = (ui_pos.y / container_size.y) - 0.5
	
	# 2. 获取小地图相机当前的视野范围
	var aspect = container_size.aspect()
	var view_height = minimap_camera.size
	var view_width = view_height * aspect
	
	# 3. 计算该 UI 点对应的世界坐标
	# 世界坐标 = 小地图相机中心点 + (偏移百分比 * 视野大小)
	var target_world_x = minimap_camera.global_position.x + (pct_x * view_width)
	var target_world_z = minimap_camera.global_position.z + (pct_y * view_height)
	
	# 4. 设置主相机位置 (主相机脚本里的 _apply_bounds_constraint 会自动处理边界防止它看出去)
	main_camera.global_position.x = target_world_x
	main_camera.global_position.z = target_world_z

# 初始化限制
func set_limits(map_min: Vector2, map_max: Vector2):
	_map_min = map_min
	_map_max = map_max
	_has_limit = true
	_apply_minimap_bounds()

# 小地图相机的边界限制（不让小地图看到黑边）
func _apply_minimap_bounds():
	var aspect = get_rect().size.aspect()
	var half_height = minimap_camera.size / 2.0
	var half_width = (minimap_camera.size * aspect) / 2.0
	
	var limit_min_x = _map_min.x + half_width
	var limit_max_x = _map_max.x - half_width
	var limit_min_z = _map_min.y + half_height
	var limit_max_z = _map_max.y - half_height
	
	if limit_min_x > limit_max_x:
		minimap_camera.global_position.x = (_map_min.x + _map_max.x) / 2.0
	else:
		minimap_camera.global_position.x = clamp(minimap_camera.global_position.x, limit_min_x, limit_max_x)
		
	if limit_min_z > limit_max_z:
		minimap_camera.global_position.z = (_map_min.y + _map_max.y) / 2.0
	else:
		minimap_camera.global_position.z = clamp(minimap_camera.global_position.z, limit_min_z, limit_max_z)
