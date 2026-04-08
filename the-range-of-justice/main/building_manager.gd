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

var is_dev_placement: bool = false # 是否是开发者模式下的放置

func _ready():
	# 动态创建画布层，防止手动在编辑器里摆放麻烦
	_setup_overlay()
	
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
	# 监听 B 键打开全建筑面板
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_B and DebugManager.is_dev_mode:
			_open_dev_build_menu()
	
	if is_building_mode:
		if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT:
			if not event.pressed: 
				_exit_building_mode()
				get_viewport().set_input_as_handled()
		elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			if not event.pressed: _try_place()


func _on_build_button_pressed(type_name: String):
	GlobalAudioManager.play_ui_sfx("InGameMenuClick")
	current_type_name = type_name
	current_stats = get_building_stats_by_type(type_name)
	if current_stats:
		is_building_mode = true
		Input.set_default_cursor_shape(Input.CURSOR_CROSS)


# 打开包含所有已注册建筑的菜单
func _open_dev_build_menu():
	if !build_menu_container: return
	
	for child in build_menu_container.get_children():
		child.queue_free()
	
	var all_types = get_registered_building_types()
	
	for type_name in all_types:
		var stats = get_building_stats_by_type(type_name)
		var btn = Button.new()
		
		# 使用 tr() 翻译前缀，并翻译 stats.building_name (假设它存储的是 KEY)
		# 格式示例: "[DEV] 兵营"
		var dev_label = tr("KEY_UI_DEV_PREFIX") # 翻译 "[DEV]"
		var b_name = tr(stats.building_name_key)    # 翻译 "KEY_BUILDING_BARRACKS"
		btn.text = dev_label + " " + b_name
		
		btn.custom_minimum_size = Vector2(150, 40)
		btn.pressed.connect(func():
			_on_dev_build_button_pressed(type_name)
		)
		build_menu_container.add_child(btn)
	
	build_menu_container.show()

# 开发者建造按钮按下
func _on_dev_build_button_pressed(type_name: String):
	GlobalAudioManager.play_ui_sfx("InGameMenuClick")
	is_dev_placement = true # 标记为开发者模式
	_on_build_button_pressed(type_name) # 复用原有的按钮按下逻辑


func _try_place():
	if can_place and current_stats != null:
		# 核心修改：如果是开发者模式点击的，最后一个参数设为 true (即 is_pre_placed)
		emit_signal("placement_requested", 
			current_type_name, 
			ghost_grid_pos, 
			selection_manager.team_id, 
			-1, 
			is_dev_placement # 这里的参数对应 C++ 的 is_pre_placed
		)
		if is_dev_placement:
			# 如果是开发者模式，我们不调用 _exit_building_mode()
			# 这样 is_building_mode 依然为 true，ghost 会继续跟随鼠标
			# 我们只需要触发一次重绘即可
			overlay.queue_redraw()
		else:
			# 普通建造模式（如农民建造），放置一个后立即退出
			_exit_building_mode()

func _exit_building_mode():
	is_building_mode = false
	is_dev_placement = false # 重置开发者标记
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
		
		# 直接翻译建筑名称 Key
		btn.text = tr(stats.building_name_key)
		
		btn.custom_minimum_size = Vector2(100, 40)
		btn.pressed.connect(_on_build_button_pressed.bind(type_name))
		build_menu_container.add_child(btn)


func show_builder_menu(building_names: Array):
	if !build_menu_container: return
	
	for child in build_menu_container.get_children():
		child.queue_free()
	
	for type_name in building_names:
		var stats = get_building_stats_by_type(type_name)
		if !stats: continue
		
		var btn = Button.new()
		
		# 使用带参数的格式化字符串，例如 "建造 %s"
		# KEY_UI_BUILD_COMMAND 在 CSV 中定义为 "Build %s" 或 "建造 %s"
		var format_str = tr("KEY_UI_BUILD_COMMAND")
		btn.text = format_str % tr(stats.building_name_key)
		
		btn.custom_minimum_size = Vector2(120, 40)
		btn.pressed.connect(_on_build_button_pressed.bind(type_name))
		build_menu_container.add_child(btn)
	
	build_menu_container.show()

func hide_builder_menu():
	if build_menu_container:
		build_menu_container.hide()
		_exit_building_mode()
