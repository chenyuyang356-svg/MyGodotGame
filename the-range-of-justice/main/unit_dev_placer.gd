extends Node
## 开发者单位放置工具：按 U 打开单位选择面板，选中后在鼠标位置放置单位

@export_group("References")
@export var camera: Camera3D
@export var unit_manager: Node
@export var selection_manager: Node
@export var flow_field_manager: Node
@export var unit_menu_container: Control

# --- 内部状态 ---
var is_unit_mode: bool = false
var current_type_name: String = ""
var ghost_world_pos: Vector2 = Vector2.ZERO
var can_place: bool = true

var overlay: Control

func _ready():
	_setup_overlay()
	if unit_menu_container:
		unit_menu_container.hide()

func _setup_overlay():
	var cl = CanvasLayer.new()
	add_child(cl)
	overlay = Control.new()
	overlay.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	overlay.draw.connect(_on_overlay_draw)
	cl.add_child(overlay)

func _process(_delta):
	if is_unit_mode:
		_update_logic()
		overlay.queue_redraw()

func _update_logic():
	if camera == null:
		return
	ghost_world_pos = camera.get_mouse_world_pos()
	can_place = _is_walkable(ghost_world_pos)

func _unhandled_input(event: InputEvent):
	# U 键打开/关闭开发者单位选择面板
	if event is InputEventKey and event.pressed and not event.echo:
		if event.keycode == KEY_U and DebugManager.is_dev_mode:
			_toggle_unit_menu()
			get_viewport().set_input_as_handled()
			return

	if is_unit_mode:
		if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_RIGHT:
			if not event.pressed:
				_exit_unit_mode()
				get_viewport().set_input_as_handled()
		elif event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
			if not event.pressed:
				_try_place()

func _toggle_unit_menu():
	if is_unit_mode:
		_exit_unit_mode()
	else:
		_open_dev_unit_menu()

func _open_dev_unit_menu():
	if unit_menu_container == null:
		return

	for child in unit_menu_container.get_children():
		child.queue_free()

	var all_types: PackedStringArray = unit_manager.get_registered_unit_types()
	all_types.sort()

	for type_name in all_types:
		var stats = unit_manager.get_unit_stats_by_type(type_name)
		if stats == null:
			continue
		var btn = Button.new()
		var label = "[DEV] %s" % type_name
		if stats.has_method("get_translated_name"):
			label = "[DEV] %s" % stats.get_translated_name()
		btn.text = label
		btn.custom_minimum_size = Vector2(280, 36)
		btn.pressed.connect(func():
			_on_dev_unit_button_pressed(type_name)
		)
		unit_menu_container.add_child(btn)

	unit_menu_container.show()

func _on_dev_unit_button_pressed(type_name: String):
	GlobalAudioManager.play_ui_sfx("InGameMenuClick")
	current_type_name = type_name
	is_unit_mode = true
	Input.set_default_cursor_shape(Input.CURSOR_CROSS)
	if unit_menu_container:
		unit_menu_container.hide()

func _try_place():
	if not can_place or current_type_name.is_empty():
		return
	# 开发模式直接放置单位（无 RPC 校验限制）
	GlobalGameManager.rpc_server_request_spawn_unit(current_type_name, ghost_world_pos, selection_manager.team_id)

func _exit_unit_mode():
	is_unit_mode = false
	current_type_name = ""
	Input.set_default_cursor_shape(Input.CURSOR_ARROW)
	overlay.queue_redraw()

func _is_walkable(world_pos: Vector2) -> bool:
	if flow_field_manager == null:
		return true
	var cell: Vector2i = flow_field_manager.world_to_grid(world_pos)
	var cost: float = flow_field_manager.get_cost(cell, 0) # 0 = NAV_LAND
	return cost < 255.0

func _on_overlay_draw():
	if not is_unit_mode or current_type_name.is_empty():
		return
	var cs: Vector2i = flow_field_manager.get_cell_size()

	var corners = [
		Vector3(ghost_world_pos.x - cs.x * 0.5, 0.1, ghost_world_pos.y - cs.y * 0.5),
		Vector3(ghost_world_pos.x + cs.x * 0.5, 0.1, ghost_world_pos.y - cs.y * 0.5),
		Vector3(ghost_world_pos.x + cs.x * 0.5, 0.1, ghost_world_pos.y + cs.y * 0.5),
		Vector3(ghost_world_pos.x - cs.x * 0.5, 0.1, ghost_world_pos.y + cs.y * 0.5),
	]
	var screen_pts = PackedVector2Array()
	for p in corners:
		screen_pts.append(camera.unproject_position(p))

	var main_color = Color.GREEN if can_place else Color.RED
	overlay.draw_colored_polygon(screen_pts, main_color * Color(1, 1, 1, 0.3))
	for i in range(4):
		overlay.draw_line(screen_pts[i], screen_pts[(i + 1) % 4], main_color, 2.0)
