extends Node2D

var unit_manager = null
var camera: Camera3D = null
var flow_field_manager: FlowFieldManager = null

@export var show_debug: bool = false
@export var show_flow_field: bool = false  # 显示流场/地形可视化（按 F2 切换）

var unit_ids_to_draw: Array = []

# --- 流场目标点调试可视化 ---
var has_flow_target: bool = false
var flow_target_world: Vector2 = Vector2.ZERO
var flow_nav_type: int = 0  # 0=LAND, 1=SEA, 2=HOVER, 3=AIR

const STATE_NAMES = ["IDLE", "MOVING", "CHASING", "ATTACKING", "PATROLLING", "DYING", "DEPLOYING"]

func _process(_delta: float) -> void:
	# 按 F2 切换流场可视化
	if Input.is_key_pressed(KEY_F2) and not _f2_held:
		_f2_held = true
		show_flow_field = not show_flow_field
		if not show_flow_field:
			has_flow_target = false
	if not Input.is_key_pressed(KEY_F2):
		_f2_held = false

	# F3 切换流场导航类型（陆地/海面/悬浮/空中）
	if Input.is_key_pressed(KEY_F3) and not _f3_held:
		_f3_held = true
		flow_nav_type = (flow_nav_type + 1) % 4
	if not Input.is_key_pressed(KEY_F3):
		_f3_held = false

	if show_flow_field and flow_field_manager != null and camera != null:
		# 左键在地图上点选流场目标点
		if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT) and not _target_click_held:
			_target_click_held = true
			_set_flow_target(camera.get_mouse_world_pos())
		if not Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
			_target_click_held = false
		# 右键清除目标点
		if Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT) and not _clear_click_held:
			_clear_click_held = true
			has_flow_target = false
		if not Input.is_mouse_button_pressed(MOUSE_BUTTON_RIGHT):
			_clear_click_held = false
		queue_redraw()

	if show_debug and unit_manager != null and camera != null and unit_ids_to_draw.size() > 0:
		queue_redraw()

var _f2_held := false
var _f3_held := false
var _target_click_held := false
var _clear_click_held := false

func _set_flow_target(world_pos: Vector2) -> void:
	if flow_field_manager == null:
		return
	var origin: Vector2i = flow_field_manager.get_grid_origin()
	var cell: Vector2i = flow_field_manager.get_cell_size()
	var w: int = flow_field_manager.get_width()
	var h: int = flow_field_manager.get_height()
	var min_world := Vector2(origin * cell)
	var max_world := Vector2((origin + Vector2i(w, h)) * cell)
	flow_target_world = world_pos.clamp(min_world, max_world)
	has_flow_target = true

func _draw() -> void:
	if show_flow_field and flow_field_manager != null and camera != null:
		_draw_flow_field()
	if show_debug and unit_manager != null and camera != null:
		_draw_units()

func _draw_flow_field() -> void:
	var font = ThemeDB.fallback_font
	var origin: Vector2i = flow_field_manager.get_grid_origin()
	var cell: Vector2i = flow_field_manager.get_cell_size()
	var w: int = flow_field_manager.get_width()
	var h: int = flow_field_manager.get_height()

	for gy in range(h):
		for gx in range(w):
			var grid_pos: Vector2i = Vector2i(gx, gy) + origin
			var cost: float = flow_field_manager.get_cost(grid_pos, 0)  # NAV_LAND
			var mask: int = flow_field_manager.get_dir_mask(grid_pos)

			# 格子中心的世界坐标（2D）→ 3D → 屏幕
			var center_2d := Vector2(grid_pos) * Vector2(cell) + Vector2(cell) * 0.5
			var center_3d := Vector3(center_2d.x, 0.1, center_2d.y)
			if camera.is_position_behind(center_3d):
				continue
			var screen_center: Vector2 = camera.unproject_position(center_3d)

			# 格子屏幕尺寸
			var top_left_3d := Vector3(center_2d.x - cell.x * 0.5, 0.1, center_2d.y - cell.y * 0.5)
			var bottom_right_3d := Vector3(center_2d.x + cell.x * 0.5, 0.1, center_2d.y + cell.y * 0.5)
			var s_tl: Vector2 = camera.unproject_position(top_left_3d)
			var s_br: Vector2 = camera.unproject_position(bottom_right_3d)
			var s_rect := Rect2(s_tl, s_br - s_tl)

			# 地形底色
			if cost >= 255.0:
				draw_rect(s_rect, Color(1, 0, 0, 0.45))  # 墙/海：红
			elif mask != 15 and mask != 0:
				draw_rect(s_rect, Color(1, 0.8, 0, 0.45))  # 高地：黄
			else:
				draw_rect(s_rect, Color(0.3, 0.8, 0.3, 0.2))  # 平地：绿

			# 高地：画方向掩码（坡口方向箭头 + 崖壁红线）
			if mask != 15 and mask != 0:
				_draw_dir_mask(screen_center, s_rect, mask, font)

	if has_flow_target:
		_draw_flow_field_target()

func _draw_flow_field_target() -> void:
	var origin: Vector2i = flow_field_manager.get_grid_origin()
	var cell: Vector2i = flow_field_manager.get_cell_size()
	var w: int = flow_field_manager.get_width()
	var h: int = flow_field_manager.get_height()
	var nav := flow_nav_type

	# 第一遍：收集每个可走格子的中心、integration 值、流场方向（避免重复查询）
	var centers: Array[Vector2] = []
	var vals: Array[float] = []
	var dirs: Array[Vector2] = []
	var walkable: Array[bool] = []
	var min_v := INF
	var max_v := -INF

	for gy in range(h):
		for gx in range(w):
			var grid_pos: Vector2i = Vector2i(gx, gy) + origin
			var walk: bool = flow_field_manager.get_cost(grid_pos, nav) < 255.0
			walkable.append(walk)
			var center := Vector2(grid_pos) * Vector2(cell) + Vector2(cell) * 0.5
			centers.append(center)
			if walk:
				var v: float = flow_field_manager.get_integration(center, flow_target_world, nav)
				vals.append(v)
				if v < 65000.0:
					min_v = min(min_v, v)
					max_v = max(max_v, v)
			else:
				vals.append(65000.0)
			dirs.append(flow_field_manager.get_flow_direction(center, flow_target_world, nav))

	var range_v := max_v - min_v

	# 第二遍：绘制集成场热力图 + 流场方向箭头
	for i in range(centers.size()):
		if not walkable[i]:
			continue
		var center := centers[i]
		var center_3d := Vector3(center.x, 0.1, center.y)
		if camera.is_position_behind(center_3d):
			continue
		var screen_center: Vector2 = camera.unproject_position(center_3d)
		var s_tl: Vector2 = camera.unproject_position(Vector3(center.x - cell.x * 0.5, 0.1, center.y - cell.y * 0.5))
		var s_br: Vector2 = camera.unproject_position(Vector3(center.x + cell.x * 0.5, 0.1, center.y + cell.y * 0.5))
		var s_rect := Rect2(s_tl, s_br - s_tl)

		# 集成场热力图：绿(近) → 黄 → 红(远)，深灰=不可达
		var v: float = vals[i]
		var col: Color
		if v >= 65000.0 or range_v <= 0.0:
			col = Color(0.15, 0.15, 0.15, 0.55)
		else:
			col = _heat_color(clamp((v - min_v) / range_v, 0.0, 1.0), 0.5)
		draw_rect(s_rect, col)

		# 流场方向箭头（格子在屏幕上足够大时才画，避免缩放太远时一片乱箭）
		if s_rect.size.length() >= 4.0:
			var dir := dirs[i]
			if dir.length_squared() > 0.001:
				_draw_arrow(screen_center, screen_center + dir * (s_rect.size.length() * 0.3), Color(1, 1, 1, 0.95))

	# 目标点标记
	var target_3d := Vector3(flow_target_world.x, 0.1, flow_target_world.y)
	if not camera.is_position_behind(target_3d):
		var s_target: Vector2 = camera.unproject_position(target_3d)
		draw_circle(s_target, 6.0, Color(1, 0.9, 0, 0.95))
		draw_line(s_target + Vector2(-12, 0), s_target + Vector2(12, 0), Color(1, 0.9, 0, 0.95), 2.0)
		draw_line(s_target + Vector2(0, -12), s_target + Vector2(0, 12), Color(1, 0.9, 0, 0.95), 2.0)

	# 图例
	var nav_names := ["LAND", "SEA", "HOVER", "AIR"]
	var font = ThemeDB.fallback_font
	draw_string(font, Vector2(10, 20),
		"F2:流场  F3:导航类型[%s]  左键:设目标  右键:清除 | 热力图:绿近红远/深灰不可达  白箭头:流场方向" % nav_names[nav],
		HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color(1, 1, 1, 0.95))

func _heat_color(t: float, alpha: float) -> Color:
	if t < 0.25:
		var u := t / 0.25
		return Color(0.0, u, 1.0, alpha)
	elif t < 0.5:
		var u := (t - 0.25) / 0.25
		return Color(0.0, 1.0, 1.0 - u, alpha)
	elif t < 0.75:
		var u := (t - 0.5) / 0.25
		return Color(u, 1.0, 0.0, alpha)
	else:
		var u := (t - 0.75) / 0.25
		return Color(1.0, 1.0 - u, 0.0, alpha)

func _draw_dir_mask(screen_center: Vector2, s_rect: Rect2, mask: int, font) -> void:
	var half := s_rect.size * 0.5
	var arrow_len := half.length() * 0.7

	# 坡口方向：画绿色箭头
	if mask & 1:  # UP
		_draw_arrow(screen_center, screen_center + Vector2(0, -arrow_len), Color(0, 1, 0, 0.9))
	if mask & 2:  # DOWN
		_draw_arrow(screen_center, screen_center + Vector2(0, arrow_len), Color(0, 1, 0, 0.9))
	if mask & 4:  # LEFT
		_draw_arrow(screen_center, screen_center + Vector2(-arrow_len, 0), Color(0, 1, 0, 0.9))
	if mask & 8:  # RIGHT
		_draw_arrow(screen_center, screen_center + Vector2(arrow_len, 0), Color(0, 1, 0, 0.9))

	# 崖壁方向：画红色短线（禁止）
	if not (mask & 1):
		draw_line(screen_center + Vector2(-half.x * 0.6, -half.y), screen_center + Vector2(half.x * 0.6, -half.y), Color(1, 0, 0, 0.9), 2.0)
	if not (mask & 2):
		draw_line(screen_center + Vector2(-half.x * 0.6, half.y), screen_center + Vector2(half.x * 0.6, half.y), Color(1, 0, 0, 0.9), 2.0)
	if not (mask & 4):
		draw_line(screen_center + Vector2(-half.x, -half.y * 0.6), screen_center + Vector2(-half.x, half.y * 0.6), Color(1, 0, 0, 0.9), 2.0)
	if not (mask & 8):
		draw_line(screen_center + Vector2(half.x, -half.y * 0.6), screen_center + Vector2(half.x, half.y * 0.6), Color(1, 0, 0, 0.9), 2.0)

	# 显示掩码数值
	draw_string(font, screen_center + Vector2(0, -half.y * 0.3), str(mask), HORIZONTAL_ALIGNMENT_CENTER, -1, 10, Color(1, 1, 1, 0.9))

func _draw_arrow(from: Vector2, to: Vector2, color: Color) -> void:
	draw_line(from, to, color, 2.0)
	var dir := (to - from).normalized()
	var perp := Vector2(-dir.y, dir.x)
	var tip := to
	draw_line(tip, tip - dir * 6 + perp * 4, color, 2.0)
	draw_line(tip, tip - dir * 6 - perp * 4, color, 2.0)

func _draw_units() -> void:
	var font = ThemeDB.fallback_font
	for uid in unit_ids_to_draw:
		var logic_pos: Vector2 = unit_manager.get_unit_position(uid)
		var state_int: int = unit_manager.get_unit_state(uid)
		var real_aggro_range: float = unit_manager.get_unit_aggro_range(uid)
		var real_attack_range: float = unit_manager.get_unit_attack_range(uid)

		var world_pos_3d = Vector3(logic_pos.x, 0.0, logic_pos.y)
		if camera.is_position_behind(world_pos_3d):
			continue
		var screen_pos: Vector2 = camera.unproject_position(world_pos_3d)

		var edge_pos_3d = world_pos_3d + Vector3(real_aggro_range, 0.0, 0.0)
		var screen_aggro_radius = screen_pos.distance_to(camera.unproject_position(edge_pos_3d))
		var atk_edge_pos_3d = world_pos_3d + Vector3(real_attack_range, 0.0, 0.0)
		var screen_attack_radius = screen_pos.distance_to(camera.unproject_position(atk_edge_pos_3d))

		var state_str = STATE_NAMES[state_int] if state_int >= 0 and state_int < STATE_NAMES.size() else "UNKNOWN"

		draw_circle(screen_pos, 3.0, Color.GREEN)
		draw_arc(screen_pos, screen_aggro_radius, 0.0, TAU, 32, Color(1, 1, 0, 0.2), 2.0)
		draw_arc(screen_pos, screen_attack_radius, 0.0, TAU, 32, Color(1, 0, 0, 0.4), 2.0)

		var text_pos = screen_pos + Vector2(0, -20)
		var text_color = Color.RED if state_int in [2, 3] else Color.WHITE
		draw_string(font, text_pos, state_str + " (" + str(uid) + ")", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, text_color)
