extends SelectionManager

# --- 配置参数 ---
@export_group("Settings")
@export var double_click_interval_ms: int = 300   # 双击判定间隔（毫秒）
@export var drag_threshold_dist: float = 10.0      # 移动超过多少像素判定为框选（而非单击）
@export var long_press_threshold_ms: int = 500    # 虽然 RTS 通常按距离判定框选，但这里预留时间判定
@export var building_manager: Node2D

# 输入状态跟踪
var is_left_down: bool = false
var press_start_pos: Vector2 = Vector2.ZERO
var press_start_time: int = 0
var last_left_click_time: int = 0
var is_actual_drag: bool = false # 是否达到了框选的触发门槛

func _unhandled_input(event: InputEvent):
	if building_manager and building_manager.is_building_mode:
		# 重置选择状态，防止框选框残留
		if is_left_down:
			is_left_down = false
			is_actual_drag = false
			queue_redraw()
		return
	
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				_on_left_pressed()
			else:
				_on_left_released()
		
		elif event.button_index == MOUSE_BUTTON_RIGHT:
			if not event.pressed: # 右键也遵循松开触发
				_on_right_released()

	elif event is InputEventMouseMotion:
		if is_left_down:
			_on_left_drag_motion()
		else:
			set_mouse_position(get_global_mouse_position())

# --- 鼠标逻辑处理 ---

func _on_left_pressed():
	is_left_down = true
	press_start_pos = get_global_mouse_position()
	press_start_time = Time.get_ticks_msec()
	is_actual_drag = false

func _on_left_drag_motion():
	# 检查是否满足移动距离门槛，满足则开启框选视觉
	if not is_actual_drag:
		if get_global_mouse_position().distance_to(press_start_pos) > drag_threshold_dist:
			is_actual_drag = true
	
	if is_actual_drag:
		set_mouse_position(get_global_mouse_position())
		box_selecting()
		queue_redraw() # 更新框选框绘制

func _on_left_released():
	var release_pos = get_global_mouse_position()
	var current_time = Time.get_ticks_msec()
	var duration = current_time - press_start_time
	
	# 1. 判定是否为框选 (根据距离或时长判定)
	if is_actual_drag or duration > long_press_threshold_ms:
		_handle_box_selection(press_start_pos, release_pos)
	
	# 2. 判定是否为双击
	elif current_time - last_left_click_time < double_click_interval_ms:
		_handle_double_click(release_pos)
		last_left_click_time = 0 # 重置，防止三连击触发两次双击
	
	# 3. 判定为普通单击
	else:
		_handle_single_click(release_pos)
		last_left_click_time = current_time
	
	# 重置状态
	is_left_down = false
	is_actual_drag = false
	queue_redraw()

func _on_right_released():
	set_mouse_position(get_global_mouse_position())
	selecting_target_position()

# --- 具体执行动作 ---

# 单击：选择单个单位
func _handle_single_click(pos: Vector2):
	set_mouse_position(pos)
	single_selecting()

# 双击：通常逻辑是选择屏幕内同类型的单位
func _handle_double_click(pos: Vector2):
	set_mouse_position(pos)
	type_selecting()

# 框选
func _handle_box_selection(start: Vector2, end: Vector2):
	set_mouse_position(end)
	end_box_selecting()

# --- 辅助功能 ---

func _get_rect(p1: Vector2, p2: Vector2) -> Rect2:
	return Rect2(
		p1.min(p2),
		(p1 - p2).abs()
	)

func _draw():
	if is_left_down and is_actual_drag:
		var current_mouse_pos = get_global_mouse_position()
		var rect = _get_rect(press_start_pos, current_mouse_pos)
		
		# 绘制空心矩形和半透明填充
		draw_rect(rect, Color(0, 1, 0, 0.1), true)
		draw_rect(rect, Color(0, 1, 0, 0.5), false, 1.0)
