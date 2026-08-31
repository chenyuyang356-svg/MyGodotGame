extends PanelContainer

# 编队栏 UI：显示 1~9,0 编队的成员数量，左键选中(双击跳转)、右键解散
# 样式与 MinimapBorder 保持一致：半透明黑底 + 2px 边框

@export var selection_manager: Node
@export var unit_manager: Node

const MAX_SLOTS: int = 10
const GRID_COLUMNS: int = 5
const SLOT_SIZE := Vector2(52, 46)

var _slots: Array = []
var _refresh_accumulator: float = 0.0

func _ready() -> void:
	custom_minimum_size = Vector2(0, 112)

	var style := StyleBoxFlat.new()
	style.bg_color = Color(0, 0, 0, 0.39215687)
	style.set_border_width_all(2)
	add_theme_stylebox_override("panel", style)

	var grid := GridContainer.new()
	grid.columns = GRID_COLUMNS
	grid.add_theme_constant_override("h_separation", 4)
	grid.add_theme_constant_override("v_separation", 4)
	grid.size_flags_horizontal = Control.SIZE_SHRINK_CENTER
	add_child(grid)

	for i in MAX_SLOTS:
		var slot := _build_slot(i)
		grid.add_child(slot)
		_slots.append(slot)

	if selection_manager:
		selection_manager.selection_changed.connect(refresh)
		selection_manager.control_groups_changed.connect(refresh)

	refresh()

func _build_slot(idx: int) -> Button:
	var btn := Button.new()
	btn.custom_minimum_size = SLOT_SIZE
	btn.focus_mode = Control.FOCUS_NONE
	btn.toggle_mode = true
	btn.text = _slot_label(idx)
	btn.add_theme_font_size_override("font_size", 14)
	btn.pressed.connect(func(): selection_manager._on_control_group_requested(idx))
	btn.gui_input.connect(_on_slot_gui_input.bind(idx))
	return btn

func _slot_label(idx: int) -> String:
	return str((idx + 1) % 10)

func _on_slot_gui_input(event: InputEvent, idx: int) -> void:
	# 右键解散编队
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
		selection_manager.disband_control_group(idx)

func _process(delta: float) -> void:
	# 定期刷新，捕捉成员阵亡等无信号变化
	_refresh_accumulator += delta
	if _refresh_accumulator >= 0.3:
		_refresh_accumulator = 0.0
		refresh()

func refresh() -> void:
	if selection_manager == null:
		return

	var current_sel: Array = selection_manager.get_selected_unit_ids()
	for i in MAX_SLOTS:
		var btn: Button = _slots[i]
		var ids: Array = selection_manager.get_control_group_ids(i)

		if ids.is_empty():
			btn.text = _slot_label(i)
			btn.tooltip_text = ""
			btn.set_pressed_no_signal(false)
			btn.modulate = Color(1, 1, 1, 0.35)
		else:
			btn.text = "%s\n%d" % [_slot_label(i), ids.size()]
			btn.set_pressed_no_signal(_same_members(ids, current_sel))
			btn.modulate = Color(1, 1, 1, 1)
			btn.tooltip_text = _build_tooltip(ids)

func _same_members(a: Array, b: Array) -> bool:
	if a.size() != b.size():
		return false
	var lookup := {}
	for v in b:
		lookup[v] = true
	for v in a:
		if not lookup.has(v):
			return false
	return true

func _build_tooltip(ids: Array) -> String:
	var counts := {}
	for uid in ids:
		var stats = unit_manager.get_unit_stats(uid)
		if stats == null:
			continue
		counts[stats.unit_name_key] = counts.get(stats.unit_name_key, 0) + 1

	var lines: Array = []
	var keys = counts.keys()
	keys.sort()
	for key in keys:
		lines.append("%s ×%d" % [tr(key), counts[key]])
	return "\n".join(lines)
