extends BuildingManager

# --- 配置 ---
@export_group("References")
@export var camera: Camera3D                # 必须指定当前正交相机
@export var flow_field_manager: Node        # C++ FlowFieldManager

@export_group("Building Settings")
@export var current_building_size: Vector2i = Vector2i(3, 3) # 建筑占用的网格大小

# --- 内部状态 ---
var is_building_mode: bool = false
var ghost_grid_pos: Vector2i = Vector2i.ZERO
var can_place: bool = false

func _process(_delta):
	if is_building_mode:
		_update_logic()
		queue_redraw() # 每一帧触发 _draw()

func _unhandled_input(event: InputEvent):
	if event is InputEventKey and event.pressed and event.keycode == KEY_B:
		_toggle_building_mode()
		get_viewport().set_input_as_handled()

	if is_building_mode:
		if event is InputEventMouseButton:
			if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
				_try_place()
				get_viewport().set_input_as_handled()
			elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
				_toggle_building_mode()
				get_viewport().set_input_as_handled()

func _toggle_building_mode():
	is_building_mode = !is_building_mode
	Input.set_default_cursor_shape(Input.CURSOR_CROSS if is_building_mode else Input.CURSOR_ARROW)
	queue_redraw()

func _update_logic():
	if camera == null: return
	
	# 1. 射线投射：鼠标屏幕位置 -> 3D 世界坐标 (Y=0 平面)
	var m_pos = get_viewport().get_mouse_position()
	var ray_origin = camera.project_ray_origin(m_pos)
	var ray_dir = camera.project_ray_normal(m_pos)
	
	if abs(ray_dir.y) < 0.0001: return
	var t = -ray_origin.y / ray_dir.y
	var world_3d = ray_origin + ray_dir * t
	
	# 2. 3D 世界坐标 -> C++ 网格坐标
	var raw_grid_pos = flow_field_manager.world_to_grid(Vector2(world_3d.x, world_3d.z))
	var new_grid_pos = raw_grid_pos - (current_building_size / 2)
	
	if new_grid_pos != ghost_grid_pos:
		ghost_grid_pos = new_grid_pos
		# 调用 C++ 检测逻辑
		can_place = is_area_clear(ghost_grid_pos, current_building_size)

func _draw():
	if not is_building_mode or camera == null:
		return

	# 获取网格尺寸
	var cell_size = flow_field_manager.get_cell_size()
	
	# 1. 计算建筑在 3D 世界中的四个角 (Y 轴稍微抬高 0.1 防止由于精度问题被地面挡住)
	var x0 = ghost_grid_pos.x * cell_size.x
	var z0 = ghost_grid_pos.y * cell_size.y
	var x1 = (ghost_grid_pos.x + current_building_size.x) * cell_size.x
	var z1 = (ghost_grid_pos.y + current_building_size.y) * cell_size.y
	
	var corners_3d = [
		Vector3(x0, 0.1, z0),
		Vector3(x1, 0.1, z0),
		Vector3(x1, 0.1, z1),
		Vector3(x0, 0.1, z1)
	]
	
	# 2. 将 3D 角投影到 2D 屏幕
	var screen_points = PackedVector2Array()
	for p in corners_3d:
		# 如果在相机背面则不画（正交相机通常不会出现，但保持习惯）
		if camera.is_position_behind(p): return
		screen_points.append(camera.unproject_position(p))
	
	# 3. 绘制填充多边形 (建筑底座)
	var fill_color = Color(0, 1, 0, 0.3) if can_place else Color(1, 0, 0, 0.3)
	draw_colored_polygon(screen_points, fill_color)
	
	# 4. 绘制描边
	var line_color = Color.GREEN if can_place else Color.RED
	for i in range(4):
		draw_line(screen_points[i], screen_points[(i + 1) % 4], line_color, 2.0)

	# 5. 绘制辅助文字
	var font = ThemeDB.fallback_font
	var text_pos = screen_points[0] + Vector2(0, -10)
	var status_text = "[ OK ]" if can_place else "[ BLOCKED ]"
	draw_string(font, text_pos, status_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, line_color)

func _try_place():
	if can_place:
		var building_id = place_building(ghost_grid_pos, current_building_size, 1)
		if building_id != -1:
			# 放置成功逻辑
			_toggle_building_mode()
