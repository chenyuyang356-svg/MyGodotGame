extends Node2D

# 这两个变量会由 main.gd 在 _ready 中赋值
var unit_manager: Node2D = null
var unit_ids_to_draw: Array = []

var show_debug: bool = true

func _process(_delta: float) -> void:
	# 只要有数据并且开启调试，每帧请求重绘
	if show_debug and unit_manager != null and unit_ids_to_draw.size() > 0:
		queue_redraw()

func _draw() -> void:
	if not show_debug or unit_manager == null:
		return
		
	var font = ThemeDB.fallback_font
	
	# 因为没有读取真实的范围数据，这里填入你 Fighter txt配置里的大概数值用于可视化
	var approx_aggro_range = 250.0 
	var approx_attack_range = 100.0

	for uid in unit_ids_to_draw:
		# 使用你已经绑定的方法获取位置和状态
		var pos: Vector2 = unit_manager.get_unit_position(uid)
		var state_int: int = unit_manager.get_unit_state(uid)
		
		# 简单过滤死亡单位 (C++ 中查不到会返回 Vector2.ZERO)
		if pos == Vector2.ZERO:
			continue
			
		# 解析状态文本 (对应 UnitState 枚举)
		var state_str = "IDLE"
		match state_int:
			0: state_str = "IDLE"
			1: state_str = "MOVING"
			2: state_str = "CHASING"
			3: state_str = "ATTACKING"
			4: state_str = "PATROLLING"
			
		# 画单位中心点
		draw_circle(pos, 3.0, Color.GREEN)
		
		# 画警戒范围 (黄圈) 和 攻击范围 (红圈)
		draw_arc(pos, approx_aggro_range, 0.0, TAU, 32, Color(0.028, 0.345, 0.04, 0.2), 1.0)
		draw_arc(pos, approx_attack_range, 0.0, TAU, 32, Color(1, 0, 0, 0.4), 1.0)
		
		# 在单位头顶显示文字状态
		var text_pos = pos + Vector2(0, -20)
		var text_color = Color.WHITE
		
		# 如果是攻击或追击状态，文字变红
		if state_int == 2 or state_int == 3:
			text_color = Color.RED
			
		draw_string(font, text_pos, state_str + " (" + str(uid) + ")", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, text_color)
