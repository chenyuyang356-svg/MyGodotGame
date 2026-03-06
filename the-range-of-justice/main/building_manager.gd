extends BuildingManager # 此时 C++ 类已是 Node3D

# --- 配置 ---
@export_group("References")
@export var camera: Camera3D
@export var flow_field_manager: Node
@export var selection_manager: Node
@export var build_menu_container: Control 

# --- 内部状态 (供 Overlay 读取) ---
var is_building_mode: bool = false
var ghost_grid_pos: Vector2i = Vector2i.ZERO
var can_place: bool = false
var current_stats: BuildingStats = null
var current_type_name: String = ""

# 2D 绘图层引用
var overlay: Control

func _ready():
	# 动态创建画布层，防止手动在编辑器里摆放麻烦
	_setup_overlay()
	
	call_deferred("_create_build_buttons")
	
	if build_menu_container:
		build_menu_container.hide()

func _setup_overlay():
	var cl = CanvasLayer.new()
	add_child(cl)
	overlay = Control.new()
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT) # 撑满全屏
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE # 别挡住鼠标点击
	overlay.draw.connect(_on_overlay_draw) # 绑定绘图回调
	cl.add_child(overlay)

# --- 逻辑更新 ---
func _process(_delta):
	if is_building_mode:
		_update_logic()
		overlay.queue_redraw() # 关键：让 2D 覆盖层重绘

func _update_logic():
	if camera == null or current_stats == null: return
	
	# 注意：确保你的 camera 有这个自定义方法，或者改用标准的 raycast 得到 3D 坐标
	var world_3d_pos = camera.get_mouse_world_pos() # 假设返回的是 Vector3
	var footprint = current_stats.get_footprint()
	
	# 这里假设 flow_field_manager 的 world_to_grid 接收 Vector3
	var raw_grid_pos = flow_field_manager.world_to_grid(world_3d_pos)
	ghost_grid_pos = raw_grid_pos - (footprint / 2)
	
	can_place = is_area_clear(ghost_grid_pos, current_stats)

# --- UI 与 输入处理 (保持原样) ---
func _unhandled_input(event: InputEvent):
	if event is InputEventKey and event.pressed and event.keycode == KEY_B:
		if build_menu_container:
			build_menu_container.visible = !build_menu_container.visible
			if not build_menu_container.visible: _exit_building_mode()

	if is_building_mode:
		if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT:
			if not event.pressed: 
				_exit_building_mode()
				get_viewport().set_input_as_handled()
		elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			if not event.pressed: _try_place()

func _on_build_button_pressed(type_name: String):
	current_type_name = type_name
	current_stats = get_building_stats_by_type(type_name)
	if current_stats:
		is_building_mode = true
		Input.set_default_cursor_shape(Input.CURSOR_CROSS)

func _try_place():
	if can_place and current_stats != null:
		emit_signal("placement_requested", current_type_name, ghost_grid_pos, selection_manager.team_id)

func _exit_building_mode():
	is_building_mode = false
	current_stats = null
	Input.set_default_cursor_shape(Input.CURSOR_ARROW)
	overlay.queue_redraw()

# --- 绘图逻辑 (由 Overlay 调用) ---
func _on_overlay_draw():
	if is_building_mode and current_stats:
		var cell_size = flow_field_manager.get_cell_size()
		
		# 1. Clearance (外框)
		var clearance = current_stats.get_clearance_size()
		var footprint = current_stats.get_footprint()
		var offset = (clearance - footprint) / 2
		_draw_grid_rect(ghost_grid_pos - offset, clearance, Color(1, 1, 0, 0.1), Color(1, 1, 0, 0.2))

		# 2. Footprint (内框)
		var main_color = Color.GREEN if can_place else Color.RED
		_draw_grid_rect(ghost_grid_pos, footprint, main_color * Color(1, 1, 1, 0.3), main_color)

func _draw_grid_rect(grid_pos: Vector2i, size: Vector2i, fill: Color, line: Color):
	var cs = flow_field_manager.get_cell_size()
	
	# 这里定义网格的四个角 (3D 坐标)
	# 假设网格平面是 XZ 平面，高度为 0.1 防止 Z-Fighting
	var corners = [
		Vector3(grid_pos.x * cs.x, 0.1, grid_pos.y * cs.y),
		Vector3((grid_pos.x + size.x) * cs.x, 0.1, grid_pos.y * cs.y),
		Vector3((grid_pos.x + size.x) * cs.x, 0.1, (grid_pos.y + size.y) * cs.y),
		Vector3(grid_pos.x * cs.x, 0.1, (grid_pos.y + size.y) * cs.y)
	]
	
	var screen_pts = PackedVector2Array()
	for p in corners:
		# 使用 camera 将 3D 坐标转为屏幕 2D 坐标
		screen_pts.append(camera.unproject_position(p))
	
	# 在 Overlay (Control 节点) 上绘制 2D 多边形
	overlay.draw_colored_polygon(screen_pts, fill)
	for i in range(4):
		overlay.draw_line(screen_pts[i], screen_pts[(i + 1) % 4], line, 2.0)

# 动态生成按钮逻辑保持不变...
func _create_build_buttons():
	if not build_menu_container: return
	for child in build_menu_container.get_children(): child.queue_free()
	var types = get_registered_building_types()
	for type_name in types:
		var btn = Button.new()
		var stats = get_building_stats_by_type(type_name)
		btn.text = stats.building_name
		btn.custom_minimum_size = Vector2(100, 40)
		btn.pressed.connect(_on_build_button_pressed.bind(type_name))
		build_menu_container.add_child(btn)
