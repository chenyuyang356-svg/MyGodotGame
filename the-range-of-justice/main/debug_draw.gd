extends Node2D

# 1. 去掉严格的 : Node2D 和 : Camera3D 限制，直接赋初始值 null
var unit_manager = null
var camera = null
@export var show_debug: bool = true

var unit_ids_to_draw: Array = []

func _process(_delta: float) -> void:
	if Engine.get_process_frames() % 60 == 0:
		return
		print("=== DebugDraw 诊断信息 ===")
		print("1. unit_manager 收到没: ", "没收到(null)" if unit_manager == null else "正常")
		print("2. camera 收到没: ", "没收到(null)" if camera == null else "正常")
		print("3. 要画的单位数量: ", unit_ids_to_draw.size())
		
		# 如果前三个都正常，抽查第一个单位的位置算得对不对
		if unit_manager != null and camera != null and unit_ids_to_draw.size() > 0:
			var test_id = unit_ids_to_draw[0]
			var logic_pos = unit_manager.get_unit_position(test_id)
			
			# 【注意】这里假设你的 3D 地面在 XZ 平面 (Y=0)
			var world_pos_3d = Vector3(logic_pos.x, 0.0, logic_pos.y) 
			var screen_pos = camera.unproject_position(world_pos_3d)
			var is_behind = camera.is_position_behind(world_pos_3d)
			
			print("4. 单位0逻辑坐标: ", logic_pos)
			print("5. 单位0屏幕坐标: ", screen_pos)
			print("6. 单位是否在相机背后: ", is_behind)

	# 原有的触发重绘逻辑
	if show_debug and unit_manager != null and camera != null and unit_ids_to_draw.size() > 0:
		queue_redraw()
	
func _draw() -> void:
	if not show_debug or unit_manager == null or camera == null:
		return
		
	var font = ThemeDB.fallback_font


	for uid in unit_ids_to_draw:
		var logic_pos: Vector2 = unit_manager.get_unit_position(uid)
		var state_int: int = unit_manager.get_unit_state(uid)
		
		if logic_pos == Vector2.ZERO:
			continue
			
		var real_aggro_range: float = unit_manager.get_unit_aggro_range(uid)
		var real_attack_range: float = unit_manager.get_unit_attack_range(uid)	
		# 1. 【坐标转换】将逻辑 2D 坐标转换为 3D 世界坐标
		# 假设你的 3D 地面在 Y=0 的平面，逻辑坐标对应 X 和 Z 轴
		var world_pos_3d = Vector3(logic_pos.x, 0.0, logic_pos.y)
		
		# 2. 【相机剔除】如果单位在相机背面，跳过绘制防止坐标反转错乱
		if camera.is_position_behind(world_pos_3d):
			continue
			
		# 3. 【屏幕投影】将 3D 世界坐标转换为玩家当前的 2D 屏幕像素坐标
		var screen_pos: Vector2 = camera.unproject_position(world_pos_3d)
		
		# 4. 【透视半径计算】为了让圈在屏幕上有“近大远小”的透视感
		# 取世界空间中边缘的一个点，计算它在屏幕上的投影距离
		var edge_pos_3d = world_pos_3d + Vector3(real_aggro_range, 0, 0)
		var edge_screen_pos = camera.unproject_position(edge_pos_3d)
		var screen_aggro_radius = screen_pos.distance_to(edge_screen_pos)
		
		var atk_edge_pos_3d = world_pos_3d + Vector3(real_attack_range, 0, 0)
		var atk_edge_screen_pos = camera.unproject_position(atk_edge_pos_3d)
		var screen_attack_radius = screen_pos.distance_to(atk_edge_screen_pos)

		# 解析状态文本
		var state_str = "IDLE"
		match state_int:
			0: state_str = "IDLE"
			1: state_str = "MOVING"
			2: state_str = "CHASING"
			3: state_str = "ATTACKING"
			4: state_str = "PATROLLING"
			
		# 绘制屏幕上的中心点
		draw_circle(screen_pos, 3.0, Color.GREEN)
		
		# 使用换算后的屏幕半径绘制圈
		draw_arc(screen_pos, screen_aggro_radius, 0.0, TAU, 32, Color(1, 1, 0, 0.2), 2.0)
		draw_arc(screen_pos, screen_attack_radius, 0.0, TAU, 32, Color(1, 0, 0, 0.4), 2.0)
		
		# 在单位头顶显示文字状态
		var text_pos = screen_pos + Vector2(0, -20)
		var text_color = Color.WHITE
		if state_int == 2 or state_int == 3:
			text_color = Color.RED
			
		draw_string(font, text_pos, state_str + " (" + str(uid) + ")", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, text_color)
