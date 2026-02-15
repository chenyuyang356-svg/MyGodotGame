extends BuildingManager

# --- 配置 ---
@export_group("References")
@export var flow_field_manager: Node2D      # 对应 FlowFieldManager
@export var unit_manager: Node2D    # 对应 UnitManager

@export_group("Building Settings")
@export var current_building_size: Vector2i = Vector2i(3, 3) # 暂时默认3x3

# --- 内部状态 ---
var is_building_mode: bool = false
var ghost_grid_pos: Vector2i = Vector2i.ZERO
var can_place: bool = false

func _ready():
	# 初始化 C++ 内部引用
	set_flow_field_manager(flow_field_manager)
	set_unit_manager(unit_manager)

func _process(_delta):
	if is_building_mode:
		_update_ghost_position()

func _unhandled_input(event: InputEvent):
	# 按下 B 键切换建筑模式
	if event is InputEventKey and event.pressed and event.keycode == KEY_B:
		_toggle_building_mode()
		get_viewport().set_input_as_handled()

	if is_building_mode:
		if event is InputEventMouseButton:
			if event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
				_try_place()
				get_viewport().set_input_as_handled()
			elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
				_toggle_building_mode() # 取消建筑模式
				get_viewport().set_input_as_handled()

func _toggle_building_mode():
	is_building_mode = !is_building_mode
	if is_building_mode:
		Input.set_default_cursor_shape(Input.CURSOR_CROSS)
	else:
		Input.set_default_cursor_shape(Input.CURSOR_ARROW)
	queue_redraw()

func _update_ghost_position():
	var mouse_pos = get_global_mouse_position()
	# 转换到网格坐标 (考虑左上角对齐)
	var raw_grid_pos = flow_field_manager.world_to_grid(mouse_pos)
	# 让建筑中心跟随鼠标
	var new_grid_pos = raw_grid_pos - (current_building_size / 2)
	
	if new_grid_pos != ghost_grid_pos:
		ghost_grid_pos = new_grid_pos
		# 调用 C++ 的 is_area_clear (现在包含地形和单位检查)
		can_place = is_area_clear(ghost_grid_pos, current_building_size)
		queue_redraw()

func _try_place():
	if can_place:
		# 调用 C++ place_building
		var building_id = place_building(ghost_grid_pos, current_building_size, 1)
		if building_id != -1:
			print("建筑放置成功: ", building_id)
			# 这里可以添加实例化建筑视觉节点的代码
			_toggle_building_mode() # 放置后退出模式，或保持
	else:
		print("无法在此处建造：地形障碍或单位阻挡")

func _draw():
	if is_building_mode:
		# 1. 绘制网格参考 (可选，仅绘制鼠标周围区域)
		_draw_preview_grid()
		
		# 2. 绘制建筑预览 (Ghost)
		var cell_size = flow_field_manager.get_cell_size()
		var draw_pos = Vector2(ghost_grid_pos * cell_size)
		var draw_size = Vector2(current_building_size * cell_size)
		var color = Color(0, 1, 0, 0.5) if can_place else Color(1, 0, 0, 0.5)
		
		draw_rect(Rect2(draw_pos, draw_size), color, true) # 填充
		draw_rect(Rect2(draw_pos, draw_size), Color.WHITE, false, 1.0) # 描边

func _draw_preview_grid():
	# 绘制一个 20x20 的局部网格以辅助对齐
	var range_val = 10
	var color = Color(1, 1, 1, 0.1)
	var cell_size = flow_field_manager.get_cell_size()
	for x in range(-range_val, range_val + current_building_size.x):
		var start = Vector2((ghost_grid_pos.x + x) * cell_size.x, (ghost_grid_pos.y - range_val) * cell_size.y)
		var end = Vector2((ghost_grid_pos.x + x) * cell_size.x, (ghost_grid_pos.y + range_val + current_building_size.y) * cell_size.y)
		draw_line(start, end, color)
	for y in range(-range_val, range_val + current_building_size.y):
		var start = Vector2((ghost_grid_pos.x - range_val) * cell_size.x, (ghost_grid_pos.y + y) * cell_size.y)
		var end = Vector2((ghost_grid_pos.x + range_val + current_building_size.x) * cell_size.x, (ghost_grid_pos.y + y) * cell_size.y)
		draw_line(start, end, color)
