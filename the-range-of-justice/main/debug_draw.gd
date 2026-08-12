extends Node2D

var unit_manager = null
var camera: Camera3D = null # 建议保留类型提示，方便享有代码补全，但不严格限制初始值
@export var show_debug: bool = true

var unit_ids_to_draw: Array = []

# 使用数组映射状态，比 match 语句更高效、整洁
const STATE_NAMES = ["IDLE", "MOVING", "CHASING", "ATTACKING", "PATROLLING", "DYING", "DEPLOYING"]

func _process(_delta: float) -> void:
	if not show_debug:
		return
	# 1. 诊断信息：每 60 帧打印一次（去掉了错误的 return）
	if Engine.get_process_frames() % 60 == 0:
		print("=== DebugDraw 诊断信息 ===")
		print("1. unit_manager 状态: ", "没收到 (null)" if unit_manager == null else "正常")
		print("2. camera 状态: ", "没收到 (null)" if camera == null else "正常")
		print("3. 要画的单位数量: ", unit_ids_to_draw.size())
		
		if unit_manager != null and camera != null and unit_ids_to_draw.size() > 0:
			var test_id = unit_ids_to_draw[0]
			var logic_pos = unit_manager.get_unit_position(test_id)
			var world_pos_3d = Vector3(logic_pos.x, 0.0, logic_pos.y) 
			var screen_pos = camera.unproject_position(world_pos_3d)
			var is_behind = camera.is_position_behind(world_pos_3d)
			
			print("  -> 单位[0]逻辑坐标: ", logic_pos)
			print("  -> 单位[0]屏幕坐标: ", screen_pos)
			print("  -> 单位是否在相机背后: ", is_behind)

	# 2. 触发重绘：必须每一帧都检测并重绘，否则相机移动时 UI 圈会脱节卡顿
	if show_debug and unit_manager != null and camera != null and unit_ids_to_draw.size() > 0:
		queue_redraw()
	
func _draw() -> void:
	if not show_debug or unit_manager == null or camera == null:
		return
		
	var font = ThemeDB.fallback_font

	for uid in unit_ids_to_draw:
		var logic_pos: Vector2 = unit_manager.get_unit_position(uid)
		
		# 【修改】如果 (0,0) 是合法坐标，请不要用 logic_pos == Vector2.ZERO 判断。
		# 建议通过 manager 判断单位是否存活/合法，例如：
		# if not unit_manager.is_valid_unit(uid): continue 
			
		var state_int: int = unit_manager.get_unit_state(uid)
		var real_aggro_range: float = unit_manager.get_unit_aggro_range(uid)
		var real_attack_range: float = unit_manager.get_unit_attack_range(uid)	
		
		# 1. 坐标转换
		var world_pos_3d = Vector3(logic_pos.x, 0.0, logic_pos.y)
		
		# 2. 相机剔除 (非常关键，防止坐标反转)
		if camera.is_position_behind(world_pos_3d):
			continue
			
		# 3. 屏幕投影
		var screen_pos: Vector2 = camera.unproject_position(world_pos_3d)
		
		# 4. 透视半径计算 (取水平边缘点)
		var edge_pos_3d = world_pos_3d + Vector3(real_aggro_range, 0.0, 0.0)
		var screen_aggro_radius = screen_pos.distance_to(camera.unproject_position(edge_pos_3d))
		
		var atk_edge_pos_3d = world_pos_3d + Vector3(real_attack_range, 0.0, 0.0)
		var screen_attack_radius = screen_pos.distance_to(camera.unproject_position(atk_edge_pos_3d))

		# 获取状态文本
		var state_str = STATE_NAMES[state_int] if state_int >= 0 and state_int < STATE_NAMES.size() else "UNKNOWN"
			
		# 绘制屏幕上的中心点
		draw_circle(screen_pos, 3.0, Color.GREEN)
		
		# 绘制半径圈
		draw_arc(screen_pos, screen_aggro_radius, 0.0, TAU, 32, Color(1, 1, 0, 0.2), 2.0)
		draw_arc(screen_pos, screen_attack_radius, 0.0, TAU, 32, Color(1, 0, 0, 0.4), 2.0)
		
		# 绘制单位头顶的文字状态
		var text_pos = screen_pos + Vector2(0, -20)
		# CHASING (2) 或 ATTACKING (3) 标红
		var text_color = Color.RED if state_int in [2, 3] else Color.WHITE
			
		draw_string(font, text_pos, state_str + " (" + str(uid) + ")", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, text_color)
