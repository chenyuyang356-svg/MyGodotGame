extends BuildingManager

# --- 配置 ---
@export_group("References")
@export var camera: Camera3D
@export var flow_field_manager: Node
@export var selection_manager: Node
@export var build_menu_container: Control # 拖入你的 HBoxContainer

# --- 内部状态 ---
var is_building_mode: bool = false
var ghost_grid_pos: Vector2i = Vector2i.ZERO
var can_place: bool = false
var current_stats: BuildingStats = null
var current_type_name: String = ""

func _ready():
	# 1. 注册建筑 (实际项目中可能在全局初始化处执行)
	register_building_type("HumanBarrack", "res://config/building/human_barrack.txt")
	
	# 2. 延迟一点时间生成按钮，确保 C++ 注册完成
	call_deferred("_create_build_buttons")
	
	# 初始隐藏菜单，按 B 才显示
	if build_menu_container:
		build_menu_container.hide()

# 动态生成建筑按钮
func _create_build_buttons():
	if not build_menu_container: return
	
	# 清空现有按钮
	for child in build_menu_container.get_children():
		child.queue_free()
	
	# 从 C++ 获取所有注册的名称
	var types = get_registered_building_types()
	
	for type_name in types:
		var btn = Button.new()
		var stats = get_building_stats_by_type(type_name)
		
		# 设置按钮外观
		btn.text = stats.building_name
		btn.custom_minimum_size = Vector2(100, 40)
		
		# 绑定点击事件，传递建筑类型名称
		btn.pressed.connect(_on_build_button_pressed.bind(type_name))
		
		build_menu_container.add_child(btn)

func _unhandled_input(event: InputEvent):
	# 按 B 开关 UI 菜单
	if event is InputEventKey and event.pressed and event.keycode == KEY_B:
		if build_menu_container:
			build_menu_container.visible = !build_menu_container.visible
			# 如果关闭了菜单，同时退出建筑预览模式
			if not build_menu_container.visible:
				_exit_building_mode()

	if is_building_mode:
		# 右键取消当前预览
		if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT:
			# 检测松开 (Release)
			if not event.pressed: 
				_exit_building_mode()
				# 告诉系统这个“松开事件”已经被消耗了
				get_viewport().set_input_as_handled()
			 
		# 左键尝试放置
		elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			# 检测松开 (Release)
			if not event.pressed: 
				_try_place()

# 当点击 UI 上的建筑按钮时触发
func _on_build_button_pressed(type_name: String):
	current_type_name = type_name
	current_stats = get_building_stats_by_type(type_name)
	
	if current_stats:
		is_building_mode = true
		Input.set_default_cursor_shape(Input.CURSOR_CROSS)
		# 隐藏菜单以防挡住视线 (可选)
		# build_menu_container.hide()
	else:
		print("Error: Could not find stats for ", type_name)

func _process(_delta):
	if is_building_mode:
		_update_logic()
		queue_redraw()

func _update_logic():
	if camera == null or current_stats == null: return
	
	var world_2d = camera.get_mouse_world_pos()
	var footprint = current_stats.get_footprint()
	
	var raw_grid_pos = flow_field_manager.world_to_grid(world_2d)
	ghost_grid_pos = raw_grid_pos - (footprint / 2)
	
	can_place = is_area_clear(ghost_grid_pos, current_stats)

func _try_place():
	if can_place and current_stats != null:
		# 从 selection_manager 获取 team_id (C++ 注册的属性)
		var my_team_id = selection_manager.team_id
		
		var b_id = place_building_by_type(current_type_name, ghost_grid_pos, my_team_id)
		
		if b_id != -1:
			print("Placed ", current_type_name, " ID: ", b_id)
			# 这里如果不调用 _exit_building_mode() 就可以连续建造
		else:
			print("Placement blocked by internal logic.")

func _exit_building_mode():
	current_stats = null
	current_type_name = ""
	Input.set_default_cursor_shape(Input.CURSOR_ARROW)
	queue_redraw()
	is_building_mode = false


func _draw():
	if is_building_mode and current_stats:
		_draw_preview_rects()

# 保持之前的双矩形绘制逻辑...
func _draw_preview_rects():
	var cell_size = flow_field_manager.get_cell_size()
	
	# Clearance (外框)
	var clearance = current_stats.get_clearance_size()
	var footprint = current_stats.get_footprint()
	var offset = (clearance - footprint) / 2
	_draw_grid_rect(ghost_grid_pos - offset, clearance, Color(1, 1, 0, 0.1), Color(1, 1, 0, 0.2))

	# Footprint (内框)
	var main_color = Color.GREEN if can_place else Color.RED
	_draw_grid_rect(ghost_grid_pos, footprint, main_color * Color(1, 1, 1, 0.3), main_color)

func _draw_grid_rect(grid_pos: Vector2i, size: Vector2i, fill: Color, line: Color):
	var cs = flow_field_manager.get_cell_size()
	var corners = [
		Vector3(grid_pos.x * cs.x, 0.1, grid_pos.y * cs.y),
		Vector3((grid_pos.x + size.x) * cs.x, 0.1, grid_pos.y * cs.y),
		Vector3((grid_pos.x + size.x) * cs.x, 0.1, (grid_pos.y + size.y) * cs.y),
		Vector3(grid_pos.x * cs.x, 0.1, (grid_pos.y + size.y) * cs.y)
	]
	var screen_pts = PackedVector2Array()
	for p in corners: screen_pts.append(camera.unproject_position(p))
	draw_colored_polygon(screen_pts, fill)
	for i in range(4): draw_line(screen_pts[i], screen_pts[(i + 1) % 4], line, 2.0)
