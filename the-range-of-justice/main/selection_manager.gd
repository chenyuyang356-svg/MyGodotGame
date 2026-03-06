extends SelectionManager

# --- 节点引用 (在编辑器中指定) ---
@export_group("Dependencies")
@export var unit_manager: Node3D
@export var building_manager: Node3D
@export var camera_3d: Node3D # 需具备 get_mouse_world_pos() 和 get_visible_world_rect() 方法
@export var production_panel: VBoxContainer

# --- 配置参数 ---
@export_group("Settings")
@export var double_click_interval_ms: int = 300
@export var drag_threshold_pixels: float = 10.0 # 屏幕像素距离，判定为开始拖拽

# --- 内部变量 ---
var is_left_down: bool = false
var press_start_screen_pos: Vector2 = Vector2.ZERO # 屏幕坐标：用于绘制 UI 和判定拖拽
var press_start_world_pos: Vector2 = Vector2.ZERO  # 世界坐标：用于传给 C++ 逻辑
var is_actual_drag: bool = false
var last_left_click_time: int = 0

func _ready() -> void:
	connect("selection_changed", _on_selection_changed)
	
	if production_panel:
		production_panel.hide()

func _process(_delta):
	# 每帧更新悬停检测 (C++ 实现)
	# 这会让单位/建筑在鼠标划过时产生即时的高亮渲染反馈
	var mouse_world = camera_3d.get_mouse_world_pos()
	update_hover(mouse_world, unit_manager, building_manager)

func _unhandled_input(event: InputEvent):
	# 如果处于建筑放置模式（由建筑管理器控制），拦截并清理选择行为
	if building_manager and building_manager.is_building_mode:
		_reset_ui_state()
		return

	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				_on_left_pressed()
			else:
				_on_left_released()
		
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			_on_right_pressed()

	elif event is InputEventMouseMotion:
		if is_left_down:
			_on_left_drag_motion()
	
	elif event is InputEventKey and event.pressed and not event.echo:
		var keycode = event.keycode
		match keycode:
			KEY_1:
				team_id = 1
			KEY_2:
				team_id = 2
			KEY_3:
				team_id = 3
			KEY_4:
				team_id = 4
			KEY_5:
				team_id = 5
	

func _on_selection_changed():
	# 清空旧按钮
	for child in production_panel.get_children():
		child.queue_free()
	production_panel.hide()
	
	# 获取选中的建筑 ID
	var selected_bids = get_selected_building_ids()
	
	# 逻辑：只有选中【一个】建筑，且它是【兵营】时显示按钮
	if selected_bids.size() == 1:
		var b_id = selected_bids[0]
		var stats = building_manager.get_building_stats(b_id)
		
		if stats and stats.building_type == BuildingStats.BUILDING_BARRACKS:
			_show_production_buttons(b_id, stats)

# --- 鼠标左键逻辑 ---

func _on_left_pressed():
	is_left_down = true
	press_start_screen_pos = get_global_mouse_position()
	press_start_world_pos = camera_3d.get_mouse_world_pos()
	is_actual_drag = false

func _on_left_drag_motion():
	if not is_actual_drag:
		# 判定是否超过像素阈值，开启框选视觉
		if get_global_mouse_position().distance_to(press_start_screen_pos) > drag_threshold_pixels:
			is_actual_drag = true
	
	if is_actual_drag:
		queue_redraw() # 触发 _draw()

func _on_left_released():
	var release_world_pos = camera_3d.get_mouse_world_pos()
	var current_time = Time.get_ticks_msec()
	
	if is_actual_drag:
		# 1. 执行框选 (C++ 接口：使用空间网格筛选单位 ID)
		var box = _get_world_rect(press_start_world_pos, release_world_pos)
		do_box_select(box, unit_manager, building_manager)
	
	elif current_time - last_left_click_time < double_click_interval_ms:
		# 2. 执行双击 (C++ 接口：选取屏幕内所有同类单位)
		var view_rect = camera_3d.get_visible_world_rect()
		do_type_select(release_world_pos, view_rect, unit_manager, building_manager)
		last_left_click_time = 0
	
	else:
		# 3. 执行单选 (C++ 接口：优先检测单位，后检测建筑)
		do_single_select(release_world_pos, unit_manager, building_manager)
		last_left_click_time = current_time
	
	_reset_ui_state()

# --- 鼠标右键逻辑 ---

func _on_right_pressed():
	var mouse_world_pos = camera_3d.get_mouse_world_pos()
	# 调用 C++ 接口，内部会执行：
	# 1. 判定点击了地面、单位还是建筑
	# 2. 自动 emit_signal("command_issued", ...) 信号给 GameManager
	handle_right_click(mouse_world_pos, unit_manager, building_manager)

# --- 选中建筑 ---


func _show_production_buttons(building_id: int, stats: BuildingStats):
	production_panel.show()
	var units = stats.producible_units
	for unit_type in units:
		var btn = Button.new()
		btn.text = "Produce " + unit_type
		btn.custom_minimum_size = Vector2(100, 40)
		# 绑定点击事件，发送 RPC 请求给服务器
		btn.pressed.connect(func(): request_unit_production(building_id, unit_type))
		production_panel.add_child(btn)


# --- 视觉反馈与辅助 ---

func _reset_ui_state():
	is_left_down = false
	is_actual_drag = false
	queue_redraw()

func _get_world_rect(p1: Vector2, p2: Vector2) -> Rect2:
	return Rect2(p1.min(p2), (p1 - p2).abs())

func _draw():
	# 仅在实际拖拽时绘制绿框
	if is_left_down and is_actual_drag:
		var current_screen_pos = get_global_mouse_position()
		
		# 将屏幕坐标转换为本地 Canvas 坐标（处理 UI 缩放适配）
		var local_start = make_canvas_position_local(press_start_screen_pos)
		var local_end = make_canvas_position_local(current_screen_pos)
		
		var rect = Rect2(local_start.min(local_end), (local_start - local_end).abs())
		
		# 绘制半透明填充和描边
		draw_rect(rect, Color(0, 1, 0, 0.15), true)
		draw_rect(rect, Color(0, 1, 0, 0.5), false, 1.0)
