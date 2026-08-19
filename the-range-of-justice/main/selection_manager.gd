extends SelectionManager

var godot_icon: Texture2D = preload("res://icon.svg")
@export var selection_item_scene: PackedScene = preload("res://ui/selection_item/selection_item.tscn")
# --- 节点引用 (在编辑器中指定) ---
@export_group("Dependencies")
@export var unit_manager: Node3D
@export var building_manager: Node3D
@export var camera_3d: Node3D # 需具备 get_mouse_world_pos() 和 get_visible_world_rect() 方法
@export var information_panel: GridContainer
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
const TAG_BUILDER = 4 

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

	# 如果处于开发者单位放置模式，拦截选择行为
	var dev_placer = get_node_or_null("../UnitDevPlacer")
	if dev_placer and dev_placer.is_unit_mode:
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
		if multiplayer.is_server() and DebugManager.is_dev_mode:
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
	_update_information_panel()
	
	production_panel.hide()
	var selected_bids = get_selected_building_ids()
	
	if selected_bids.size() > 0:
		var first_bid = selected_bids[0]
		var first_stats = building_manager.get_building_stats(first_bid)
		
		if first_stats and first_stats.building_type == BuildingStats.BUILDING_BARRACKS:
			var target_type_name = first_stats.building_name
			var is_homogeneous = true
			
			for i in range(1, selected_bids.size()):
				var current_stats = building_manager.get_building_stats(selected_bids[i])
				if not current_stats or current_stats.building_name != target_type_name:
					is_homogeneous = false
					break
			
			if is_homogeneous:
				_show_production_buttons(selected_bids, first_stats)
	
	# --- 建造者逻辑 ---
	var selected_uids = get_selected_unit_ids()
	var builder_stats_list = []
	for u_id in selected_uids:
		var u_stats = unit_manager.get_unit_stats(u_id)
		if u_stats and (u_stats.unit_tags & TAG_BUILDER):
			builder_stats_list.append(u_stats)
	
	if builder_stats_list.size() > 0:
		var buildable_list = builder_stats_list[0].producible_buildings
		# 注意：building_manager.show_builder_menu 内部也应使用 tr()
		building_manager.show_builder_menu(buildable_list)
	else:
		building_manager.hide_builder_menu()

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


# --- 选中单位或建筑 ---

func _update_information_panel() -> void:
	if not information_panel:
		return

	for child in information_panel.get_children():
		child.queue_free()

	# 统计数据。Key 存储原始的 KEY_字符串，不在此处翻译以防排序错乱
	var registry = {}

	for u_id in get_selected_unit_ids():
		var stats = unit_manager.get_unit_stats(u_id)
		if stats:
			_add_to_registry(registry, stats.unit_name_key, godot_icon)

	for b_id in get_selected_building_ids():
		var stats = building_manager.get_building_stats(b_id)
		if stats:
			_add_to_registry(registry, stats.building_name_key, godot_icon)

	var keys = registry.keys()
	keys.sort()

	for key_name in keys:
		var data = registry[key_name]
		var item = selection_item_scene.instantiate()
		information_panel.add_child(item)
		
		# 在最终显示给 UI 时进行翻译
		var translated_name = tr(key_name)
		item.setup(translated_name, data.count, data.icon)

# 辅助函数：简化计数逻辑
func _add_to_registry(reg: Dictionary, type_name: String, icon: Texture2D):
	if reg.has(type_name):
		reg[type_name].count += 1
	else:
		reg[type_name] = {"count": 1, "icon": icon}


func _show_production_buttons(building_ids: Array, stats: BuildingStats):
	production_panel.show()
	for child in production_panel.get_children():
		child.queue_free()
		
	var units = stats.producible_units # 这里存的是单位的类型名 KEY
	for unit_type in units:
		var unit_stats = unit_manager.get_unit_stats_by_type(unit_type)
		if (!unit_stats): continue
		var unit_key = unit_stats.unit_name_key
		
		var btn = Button.new()
		
		# 使用格式化字符串： "Produce %s" -> "生产 %s"
		var base_text = tr("KEY_UI_PRODUCE_COMMAND") % tr(unit_key)
		
		# 如果操作多个建筑，增加数量显示，例如 "生产 坦克 (x3)"
		if building_ids.size() > 1:
			var count_text = "(x%s)" % building_ids.size()
			btn.text = base_text + count_text
		else:
			btn.text = base_text
			
		btn.custom_minimum_size = Vector2(100, 40)
		btn.pressed.connect(func(): 
			GlobalAudioManager.play_ui_sfx("InGameMenuClick")
			for b_id in building_ids:
				request_unit_production(b_id, unit_type)
		)
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


func get_current_unit_selection() -> Array:
	return get_selected_unit_ids()
