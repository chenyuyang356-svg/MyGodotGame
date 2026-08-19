extends Control
## 炮塔与武器定位编辑器 (Turret & Weapon Positioner)
## 支持：挂载点拖拽微调、旋转中心(Pivot)标定、枪口(Muzzle)标定、实时旋转与开火预览、多炮塔管理与镜像

const UNIT_DIR := "res://config/unit"
const BUILDING_DIR := "res://config/building"
const WEAPON_DIR := "res://config/weapon"

enum EditMode {
	MOUNT = 0,    ## 调整挂载点位置 (Unit cfg: weapon_mount)
	PIVOT = 1,    ## 调整武器旋转中心 (Weapon cfg: rotation_center)
	MUZZLE = 2    ## 调整枪口发射位置 (Weapon cfg: muzzle_offset)
}

# --- 数据结构 ---
var unit_entries: Array = []              # [{name, path, is_building, display_name}]
var weapon_cfgs: Dictionary = {}          # weapon_name -> {path, data}
var weapon_cache: Dictionary = {}         # weapon_name -> {tex, h_frames, v_frames, idle_row, attacking_row, pivot, muzzle, ...}

var current_target_path: String = ""
var current_target_data: Dictionary = {}
var is_current_building: bool = false
var mounts: Array = []                    # [{name, offset: Vector2, orig_offset: Vector2, line: int}]
var current_mount_idx: int = -1

# --- 画布与交互状态 ---
var edit_mode: EditMode = EditMode.MOUNT
var preview_angle_deg: float = 0.0
var is_auto_rotating: bool = false
var is_mouse_aiming: bool = false
var is_attacking_preview: bool = false
var show_grid: bool = true
var show_collision_radius: bool = true
var show_turrets: bool = true

# 视口变换
var canvas_zoom: float = 4.0
const ZOOM_MIN := 0.25
const ZOOM_MAX := 16.0
var canvas_pan: Vector2 = Vector2.ZERO
var is_panning: bool = false
var pan_start_mouse: Vector2 = Vector2.ZERO
var pan_start_pos: Vector2 = Vector2.ZERO

# 拖拽句柄
var is_dragging_handle: bool = false
var active_drag_type: String = ""         # "mount", "pivot", "muzzle"
var drag_handle_mount_idx: int = -1
var drag_start_val: Vector2 = Vector2.ZERO
var drag_mouse_start: Vector2 = Vector2.ZERO
var hovered_handle: Dictionary = {}       # {type, index}

# --- 节点引用 ---
@onready var stage_container: Control = $StageContainer
@onready var stage: Node2D = $StageContainer/Stage
@onready var grid_draw: Node2D = $StageContainer/Stage/GridDraw
@onready var body_sprite: Sprite2D = $StageContainer/Stage/BodySprite
@onready var turret_container: Node2D = $StageContainer/Stage/TurretContainer
@onready var handles_draw: Node2D = $StageContainer/Stage/HandlesDraw

# UI 引用
@onready var unit_selector: OptionButton = %UnitSelector
@onready var prev_unit_btn: Button = %PrevUnitBtn
@onready var next_unit_btn: Button = %NextUnitBtn
@onready var mode_mount_btn: Button = %ModeMountBtn
@onready var mode_pivot_btn: Button = %ModePivotBtn
@onready var mode_muzzle_btn: Button = %ModeMuzzleBtn

@onready var mount_item_list: ItemList = %MountItemList
@onready var add_mount_btn: Button = %AddMountBtn
@onready var del_mount_btn: Button = %DelMountBtn
@onready var mirror_mount_btn: Button = %MirrorMountBtn
@onready var mirror_dup_btn: Button = %MirrorDupBtn
@onready var move_up_btn: Button = %MoveUpBtn
@onready var move_down_btn: Button = %MoveDownBtn

@onready var spin_mount_x: SpinBox = %SpinMountX
@onready var spin_mount_y: SpinBox = %SpinMountY
@onready var spin_pivot_x: SpinBox = %SpinPivotX
@onready var spin_pivot_y: SpinBox = %SpinPivotY
@onready var spin_muzzle_x: SpinBox = %SpinMuzzleX
@onready var spin_muzzle_y: SpinBox = %SpinMuzzleY
@onready var spin_muzzle_z: SpinBox = %SpinMuzzleZ

@onready var weapon_info_label: Label = %WeaponInfoLabel
@onready var angle_slider: HSlider = %AngleSlider
@onready var angle_value_label: Label = %AngleValueLabel
@onready var auto_rot_check: CheckButton = %AutoRotCheck
@onready var mouse_aim_check: CheckButton = %MouseAimCheck
@onready var fire_anim_check: CheckButton = %FireAnimCheck

@onready var zoom_label: Label = %ZoomLabel
@onready var toast_label: Label = %ToastLabel
@onready var toast_timer: Timer = $ToastTimer
@onready var add_weapon_dialog: ConfirmationDialog = $AddWeaponDialog
@onready var weapon_pick_list: ItemList = $AddWeaponDialog/VBox/WeaponPickList


func _ready() -> void:
	_scan_all_weapons()
	_scan_all_targets()
	_setup_ui_signals()
	_setup_canvas()
	
	if unit_selector.item_count > 0:
		unit_selector.select(0)
		_on_target_selected(0)
	
	_set_mode(EditMode.MOUNT)


func _process(delta: float) -> void:
	if is_auto_rotating:
		preview_angle_deg = fmod(preview_angle_deg + delta * 90.0, 360.0)
		angle_slider.set_value_no_signal(preview_angle_deg)
		angle_value_label.text = "%.1f°" % preview_angle_deg
		_update_turret_transforms()
		handles_draw.queue_redraw()
	elif is_mouse_aiming and current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var mount_pos: Vector2 = mounts[current_mount_idx].offset
		var mouse_local := stage.to_local(get_global_mouse_position())
		var dir := mouse_local - mount_pos
		if dir.length_squared() > 1.0:
			var target_angle_deg := rad_to_deg(dir.angle())
			preview_angle_deg = fmod(target_angle_deg + 360.0, 360.0)
			angle_slider.set_value_no_signal(preview_angle_deg)
			angle_value_label.text = "%.1f°" % preview_angle_deg
			_update_turret_transforms()
			handles_draw.queue_redraw()


# ==============================================================================
# 数据加载与扫描
# ==============================================================================

func _scan_all_weapons() -> void:
	weapon_cfgs.clear()
	weapon_cache.clear()
	for path in _get_dir_files(WEAPON_DIR, ".cfg"):
		var d := _parse_cfg_file(path)
		var w_name: String = str(d.get("weapon_name", path.get_file().get_basename()))
		weapon_cfgs[w_name] = {
			"path": path,
			"data": d
		}
		_load_weapon_cache(w_name)


func _load_weapon_cache(w_name: String) -> Dictionary:
	if weapon_cache.has(w_name):
		return weapon_cache[w_name]
	if not weapon_cfgs.has(w_name):
		return {}
	
	var d: Dictionary = weapon_cfgs[w_name].data
	var tex_path: String = str(d.get("texture_path", ""))
	var tex: Texture2D = null
	if ResourceLoader.exists(tex_path):
		tex = load(tex_path)
	
	var h_f := int(d.get("h_frames", "1"))
	var v_f := int(d.get("v_frames", "1"))
	var idle_r := int(d.get("idle_row", "0"))
	var atk_r := int(d.get("attacking_row", "1" if v_f > 1 else "0"))
	
	var pivot_str: String = str(d.get("rotation_center", "0,0"))
	var pivot_parts := pivot_str.split(",", false)
	var pivot := Vector2(
		float(pivot_parts[0]) if pivot_parts.size() >= 1 else 0.0,
		float(pivot_parts[1]) if pivot_parts.size() >= 2 else 0.0
	)
	
	var muz_str: String = str(d.get("muzzle_offset", "0,0,0"))
	var muz_parts := muz_str.split(",", false)
	var muzzle := Vector3(
		float(muz_parts[0]) if muz_parts.size() >= 1 else 0.0,
		float(muz_parts[1]) if muz_parts.size() >= 2 else 0.0,
		float(muz_parts[2]) if muz_parts.size() >= 3 else 0.0
	)
	
	var entry := {
		"name": w_name,
		"tex": tex,
		"tex_path": tex_path,
		"h_frames": max(1, h_f),
		"v_frames": max(1, v_f),
		"idle_row": idle_r,
		"attacking_row": atk_r,
		"pivot": pivot,
		"muzzle": muzzle
	}
	weapon_cache[w_name] = entry
	return entry


func _scan_all_targets() -> void:
	unit_entries.clear()
	unit_selector.clear()
	
	# 1. 扫描兵种单位
	for path in _get_dir_files(UNIT_DIR, ".cfg"):
		var d := _parse_cfg_file(path)
		var u_name: String = str(d.get("unit_name", path.get_file().get_basename()))
		unit_entries.append({
			"name": u_name,
			"path": path,
			"is_building": false,
			"display_name": "[单位] " + u_name
		})
	
	# 2. 扫描建筑单位 (如 Fortress 等)
	for path in _get_dir_files(BUILDING_DIR, ".cfg"):
		var d := _parse_cfg_file(path)
		var b_name: String = str(d.get("building_name", path.get_file().get_basename()))
		unit_entries.append({
			"name": b_name,
			"path": path,
			"is_building": true,
			"display_name": "[建筑] " + b_name
		})
	
	for entry in unit_entries:
		unit_selector.add_item(entry.display_name)


func _load_target(path: String, is_building: bool) -> void:
	current_target_path = path
	is_current_building = is_building
	current_target_data = _parse_cfg_file(path)
	
	# 1. 加载身体/底盘贴图
	var tex_path: String = str(current_target_data.get("texture_path", ""))
	var tex: Texture2D = null
	if ResourceLoader.exists(tex_path):
		tex = load(tex_path)
	
	body_sprite.texture = tex
	if tex:
		var h_f := int(current_target_data.get("h_frames", "1"))
		var v_f := int(current_target_data.get("v_frames", "1"))
		h_f = max(1, h_f)
		v_f = max(1, v_f)
		body_sprite.region_enabled = true
		var fw := tex.get_width() / float(h_f)
		var fh := tex.get_height() / float(v_f)
		body_sprite.region_rect = Rect2(0, 0, fw, fh)
		body_sprite.position = Vector2.ZERO
	else:
		body_sprite.region_enabled = false
	
	# 2. 解析 weapon_mount 列表
	mounts = _parse_all_mounts(path)
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	
	if mounts.size() > 0:
		_select_mount(0)
	else:
		_select_mount(-1)
	
	grid_draw.queue_redraw()
	handles_draw.queue_redraw()


func _parse_all_mounts(path: String) -> Array:
	var out: Array = []
	if not FileAccess.file_exists(path):
		return out
	var lines := FileAccess.get_file_as_string(path).split("\n")
	for i in lines.size():
		var line := lines[i].strip_edges()
		if not line.begins_with("weapon_mount"):
			continue
		var eq := line.find("=")
		if eq <= 0:
			continue
		var val := line.substr(eq + 1).strip_edges()
		var comment := ""
		var semi := val.find(";")
		if semi >= 0:
			comment = val.substr(semi)
			val = val.substr(0, semi).strip_edges()
		var parts: PackedStringArray = val.split(",", false)
		if parts.size() >= 1:
			var w_name: String = parts[0].strip_edges()
			var ox: float = float(parts[1]) if parts.size() >= 2 else 0.0
			var oy: float = float(parts[2]) if parts.size() >= 3 else 0.0
			out.append({
				"name": w_name,
				"offset": Vector2(ox, oy),
				"orig_offset": Vector2(ox, oy),
				"comment": comment,
				"line": i
			})
	return out


func _rebuild_turret_sprites() -> void:
	for child in turret_container.get_children():
		child.queue_free()
	
	for i in mounts.size():
		var m = mounts[i]
		var vis := _load_weapon_cache(m.name)
		var spr := Sprite2D.new()
		spr.name = "TurretSprite_%d" % i
		spr.texture = vis.get("tex", null)
		if spr.texture:
			spr.region_enabled = true
			var h_f: int = vis.get("h_frames", 1)
			var v_f: int = vis.get("v_frames", 1)
			var fw := spr.texture.get_width() / float(h_f)
			var fh := spr.texture.get_height() / float(v_f)
			var row: int = vis.get("attacking_row", 0) if is_attacking_preview else vis.get("idle_row", 0)
			spr.region_rect = Rect2(0, row * fh, fw, fh)
		turret_container.add_child(spr)
	
	_update_turret_transforms()


func _update_turret_transforms() -> void:
	var children := turret_container.get_children()
	var rad := deg_to_rad(preview_angle_deg)
	
	for i in children.size():
		if i >= mounts.size():
			continue
		var spr: Sprite2D = children[i] as Sprite2D
		var m = mounts[i]
		var vis := _load_weapon_cache(m.name)
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		
		# 核心几何：mount.offset 是舰体/底盘上的固定旋转轴心
		# 炮塔精灵需要相对于该轴心按 pivot 反向偏移并整体旋转
		spr.rotation = rad
		spr.position = m.offset - pivot.rotated(rad)
		spr.visible = show_turrets
		
		# 高亮选中项
		if i == current_mount_idx:
			spr.modulate = Color(1.2, 1.2, 1.2, 1.0)
		else:
			spr.modulate = Color(1.0, 1.0, 1.0, 0.6)


# ==============================================================================
# UI 逻辑与交互
# ==============================================================================

func _setup_ui_signals() -> void:
	unit_selector.item_selected.connect(_on_target_selected)
	prev_unit_btn.pressed.connect(func():
		var idx := (unit_selector.selected - 1 + unit_selector.item_count) % unit_selector.item_count
		unit_selector.select(idx)
		_on_target_selected(idx)
	)
	next_unit_btn.pressed.connect(func():
		var idx := (unit_selector.selected + 1) % unit_selector.item_count
		unit_selector.select(idx)
		_on_target_selected(idx)
	)
	
	mode_mount_btn.pressed.connect(func(): _set_mode(EditMode.MOUNT))
	mode_pivot_btn.pressed.connect(func(): _set_mode(EditMode.PIVOT))
	mode_muzzle_btn.pressed.connect(func(): _set_mode(EditMode.MUZZLE))
	
	mount_item_list.item_selected.connect(_on_mount_list_selected)
	add_mount_btn.pressed.connect(_on_add_mount_pressed)
	del_mount_btn.pressed.connect(_on_del_mount_pressed)
	mirror_mount_btn.pressed.connect(_on_mirror_mount_pressed)
	mirror_dup_btn.pressed.connect(_on_mirror_dup_pressed)
	move_up_btn.pressed.connect(_on_move_up_pressed)
	move_down_btn.pressed.connect(_on_move_down_pressed)
	
	spin_mount_x.value_changed.connect(_on_spin_mount_x_changed)
	spin_mount_y.value_changed.connect(_on_spin_mount_y_changed)
	spin_pivot_x.value_changed.connect(_on_spin_pivot_x_changed)
	spin_pivot_y.value_changed.connect(_on_spin_pivot_y_changed)
	spin_muzzle_x.value_changed.connect(_on_spin_muzzle_x_changed)
	spin_muzzle_y.value_changed.connect(_on_spin_muzzle_y_changed)
	spin_muzzle_z.value_changed.connect(_on_spin_muzzle_z_changed)
	
	angle_slider.value_changed.connect(_on_angle_slider_changed)
	auto_rot_check.toggled.connect(func(v): is_auto_rotating = v)
	mouse_aim_check.toggled.connect(func(v): is_mouse_aiming = v)
	fire_anim_check.toggled.connect(_on_fire_anim_toggled)
	
	%BtnAngle0.pressed.connect(func(): _set_angle(0.0))
	%BtnAngle90.pressed.connect(func(): _set_angle(90.0))
	%BtnAngle180.pressed.connect(func(): _set_angle(180.0))
	%BtnAngle270.pressed.connect(func(): _set_angle(270.0))
	
	%SaveUnitBtn.pressed.connect(_save_current_unit)
	%SaveWeaponBtn.pressed.connect(_save_current_weapon)
	%SaveAllBtn.pressed.connect(_save_all)
	%ResetBtn.pressed.connect(_reset_current_unit)
	
	%ZoomInBtn.pressed.connect(func(): _set_zoom(canvas_zoom * 1.3))
	%ZoomOutBtn.pressed.connect(func(): _set_zoom(canvas_zoom / 1.3))
	%ZoomResetBtn.pressed.connect(func(): _set_zoom(4.0))
	%ZoomFitBtn.pressed.connect(_fit_view)
	
	%GridCheck.toggled.connect(func(v): show_grid = v; grid_draw.queue_redraw())
	%RadiusCheck.toggled.connect(func(v): show_collision_radius = v; grid_draw.queue_redraw())
	%TurretsCheck.toggled.connect(func(v): show_turrets = v; _update_turret_transforms())
	
	add_weapon_dialog.confirmed.connect(_on_add_weapon_dialog_confirmed)


func _setup_canvas() -> void:
	if stage_container.size != Vector2.ZERO:
		canvas_pan = stage_container.size * 0.5
	else:
		canvas_pan = get_viewport_rect().size * 0.5
	_apply_canvas_transform()


func _set_mode(mode: EditMode) -> void:
	edit_mode = mode
	mode_mount_btn.button_pressed = (mode == EditMode.MOUNT)
	mode_pivot_btn.button_pressed = (mode == EditMode.PIVOT)
	mode_muzzle_btn.button_pressed = (mode == EditMode.MUZZLE)
	handles_draw.queue_redraw()


func _set_angle(deg: float) -> void:
	preview_angle_deg = deg
	angle_slider.set_value_no_signal(deg)
	angle_value_label.text = "%.1f°" % deg
	_update_turret_transforms()
	handles_draw.queue_redraw()


func _on_angle_slider_changed(val: float) -> void:
	preview_angle_deg = val
	angle_value_label.text = "%.1f°" % val
	_update_turret_transforms()
	handles_draw.queue_redraw()


func _on_fire_anim_toggled(pressed: bool) -> void:
	is_attacking_preview = pressed
	_rebuild_turret_sprites()


func _on_target_selected(idx: int) -> void:
	if idx < 0 or idx >= unit_entries.size():
		return
	var entry = unit_entries[idx]
	_load_target(entry.path, entry.is_building)


func _update_mount_list_ui() -> void:
	mount_item_list.clear()
	for i in mounts.size():
		var m = mounts[i]
		var label := "[%d] %s (%.1f, %.1f)" % [i, m.name, m.offset.x, m.offset.y]
		mount_item_list.add_item(label)
	
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		mount_item_list.select(current_mount_idx)


func _select_mount(idx: int) -> void:
	current_mount_idx = idx
	_update_turret_transforms()
	
	if idx >= 0 and idx < mounts.size():
		var m = mounts[idx]
		spin_mount_x.set_value_no_signal(m.offset.x)
		spin_mount_y.set_value_no_signal(m.offset.y)
		
		var vis := _load_weapon_cache(m.name)
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		var muz: Vector3 = vis.get("muzzle", Vector3.ZERO)
		
		spin_pivot_x.set_value_no_signal(pivot.x)
		spin_pivot_y.set_value_no_signal(pivot.y)
		spin_muzzle_x.set_value_no_signal(muz.x)
		spin_muzzle_y.set_value_no_signal(muz.y)
		spin_muzzle_z.set_value_no_signal(muz.z)
		
		weapon_info_label.text = "武器: %s (图集 %dx%d)" % [m.name, vis.get("h_frames", 1), vis.get("v_frames", 1)]
	else:
		spin_mount_x.set_value_no_signal(0.0)
		spin_mount_y.set_value_no_signal(0.0)
		spin_pivot_x.set_value_no_signal(0.0)
		spin_pivot_y.set_value_no_signal(0.0)
		spin_muzzle_x.set_value_no_signal(0.0)
		spin_muzzle_y.set_value_no_signal(0.0)
		spin_muzzle_z.set_value_no_signal(0.0)
		weapon_info_label.text = "未选中武器"
	
	handles_draw.queue_redraw()


func _on_mount_list_selected(idx: int) -> void:
	_select_mount(idx)


func _on_spin_mount_x_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		mounts[current_mount_idx].offset.x = val
		_update_mount_list_ui()
		_update_turret_transforms()
		handles_draw.queue_redraw()


func _on_spin_mount_y_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		mounts[current_mount_idx].offset.y = val
		_update_mount_list_ui()
		_update_turret_transforms()
		handles_draw.queue_redraw()


func _on_spin_pivot_x_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name].pivot.x = val
			_update_turret_transforms()
			handles_draw.queue_redraw()


func _on_spin_pivot_y_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name].pivot.y = val
			_update_turret_transforms()
			handles_draw.queue_redraw()


func _on_spin_muzzle_x_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name].muzzle.x = val
			handles_draw.queue_redraw()


func _on_spin_muzzle_y_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name].muzzle.y = val
			handles_draw.queue_redraw()


func _on_spin_muzzle_z_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name].muzzle.z = val
			handles_draw.queue_redraw()


func _on_add_mount_pressed() -> void:
	weapon_pick_list.clear()
	for w_name in weapon_cfgs.keys():
		weapon_pick_list.add_item(w_name)
	if weapon_pick_list.item_count > 0:
		weapon_pick_list.select(0)
	add_weapon_dialog.popup_centered(Vector2i(360, 420))


func _on_add_weapon_dialog_confirmed() -> void:
	var selected := weapon_pick_list.get_selected_items()
	if selected.is_empty():
		return
	var w_name: String = weapon_pick_list.get_item_text(selected[0])
	var new_mount := {
		"name": w_name,
		"offset": Vector2.ZERO,
		"orig_offset": Vector2.ZERO,
		"comment": "",
		"line": -1
	}
	mounts.append(new_mount)
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(mounts.size() - 1)
	_show_toast("已添加武器挂载: %s" % w_name)


func _on_del_mount_pressed() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		return
	var removed_name = mounts[current_mount_idx].name
	mounts.remove_at(current_mount_idx)
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	if mounts.size() > 0:
		_select_mount(clamp(current_mount_idx, 0, mounts.size() - 1))
	else:
		_select_mount(-1)
	_show_toast("已删除挂载: %s" % removed_name)


func _on_mirror_mount_pressed() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		return
	mounts[current_mount_idx].offset.x = -mounts[current_mount_idx].offset.x
	spin_mount_x.set_value_no_signal(mounts[current_mount_idx].offset.x)
	_update_mount_list_ui()
	_update_turret_transforms()
	handles_draw.queue_redraw()
	_show_toast("已水平镜像X坐标")


func _on_mirror_dup_pressed() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		return
	var src = mounts[current_mount_idx]
	var dup := {
		"name": src.name,
		"offset": Vector2(-src.offset.x, src.offset.y),
		"orig_offset": Vector2(-src.offset.x, src.offset.y),
		"comment": src.comment,
		"line": -1
	}
	mounts.append(dup)
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(mounts.size() - 1)
	_show_toast("已镜像克隆挂载点")


func _on_move_up_pressed() -> void:
	if current_mount_idx <= 0 or current_mount_idx >= mounts.size():
		return
	var tmp = mounts[current_mount_idx]
	mounts[current_mount_idx] = mounts[current_mount_idx - 1]
	mounts[current_mount_idx - 1] = tmp
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(current_mount_idx - 1)


func _on_move_down_pressed() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size() - 1:
		return
	var tmp = mounts[current_mount_idx]
	mounts[current_mount_idx] = mounts[current_mount_idx + 1]
	mounts[current_mount_idx + 1] = tmp
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(current_mount_idx + 1)


# ==============================================================================
# 画布视口缩放与平移
# ==============================================================================

func _set_zoom(z: float, focus_center: Vector2 = Vector2.ZERO) -> void:
	var old_zoom := canvas_zoom
	canvas_zoom = clampf(z, ZOOM_MIN, ZOOM_MAX)
	zoom_label.text = "x%.1f" % canvas_zoom
	
	if focus_center != Vector2.ZERO:
		canvas_pan = focus_center + (canvas_pan - focus_center) * (canvas_zoom / old_zoom)
	
	_apply_canvas_transform()


func _fit_view() -> void:
	canvas_pan = stage_container.size * 0.5
	_set_zoom(4.0)


func _apply_canvas_transform() -> void:
	stage.position = canvas_pan
	stage.scale = Vector2.ONE * canvas_zoom
	grid_draw.queue_redraw()
	handles_draw.queue_redraw()


# ==============================================================================
# 鼠标输入处理与拖拽句柄
# ==============================================================================

func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_MIDDLE or (event.button_index == MOUSE_BUTTON_RIGHT and not is_dragging_handle):
			if event.pressed:
				is_panning = true
				pan_start_mouse = event.position
				pan_start_pos = canvas_pan
			else:
				is_panning = false
		elif event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			_set_zoom(canvas_zoom * 1.2, event.position)
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			_set_zoom(canvas_zoom / 1.2, event.position)
		elif event.button_index == MOUSE_BUTTON_LEFT:
			if event.pressed:
				_handle_left_click_pressed(event.position)
			else:
				_handle_left_click_released()
				
	elif event is InputEventMouseMotion:
		if is_panning:
			canvas_pan = pan_start_pos + (event.position - pan_start_mouse)
			_apply_canvas_transform()
		elif is_dragging_handle:
			_handle_drag_motion(event.position)
		else:
			_update_hovered_handle(event.position)


func _unhandled_key_input(event: InputEvent) -> void:
	if event.is_pressed() and not event.is_echo():
		if event.is_action_pressed("ui_cancel"):
			pass
		if event.keycode == KEY_S and event.is_command_or_control_pressed():
			_save_all()
			get_viewport().set_input_as_handled()
		elif event.keycode == KEY_SPACE:
			is_auto_rotating = not is_auto_rotating
			auto_rot_check.button_pressed = is_auto_rotating
			get_viewport().set_input_as_handled()
		elif event.keycode == KEY_R:
			_set_angle(0.0)
			get_viewport().set_input_as_handled()
		elif event.keycode == KEY_1:
			_set_mode(EditMode.MOUNT)
		elif event.keycode == KEY_2:
			_set_mode(EditMode.PIVOT)
		elif event.keycode == KEY_3:
			_set_mode(EditMode.MUZZLE)


func _update_hovered_handle(mouse_pos: Vector2) -> void:
	var prev_hover = hovered_handle.duplicate()
	hovered_handle.clear()
	
	var stage_mouse := stage.to_local(mouse_pos)
	var hit_radius_world := 14.0 / canvas_zoom
	var rad := deg_to_rad(preview_angle_deg)
	
	# 逆序检测 (后绘制的在上层)
	for i in range(mounts.size() - 1, -1, -1):
		var m = mounts[i]
		var vis := _load_weapon_cache(m.name)
		var mount_pos: Vector2 = m.offset
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		var muz3: Vector3 = vis.get("muzzle", Vector3.ZERO)
		var muz_pos: Vector2 = mount_pos + Vector2(muz3.x, muz3.y).rotated(rad)
		
		# 1. 枪口句柄检测
		if edit_mode == EditMode.MUZZLE and stage_mouse.distance_to(muz_pos) <= hit_radius_world:
			hovered_handle = {"type": "muzzle", "index": i}
			break
		
		# 2. 旋转中心(Pivot)句柄检测
		if edit_mode == EditMode.PIVOT and stage_mouse.distance_to(mount_pos) <= hit_radius_world:
			hovered_handle = {"type": "pivot", "index": i}
			break
		
		# 3. 挂载点(Mount)句柄检测
		if stage_mouse.distance_to(mount_pos) <= hit_radius_world:
			hovered_handle = {"type": "mount", "index": i}
			break
		
		# 4. 炮塔精灵包围盒检测
		var spr_pos: Vector2 = mount_pos - pivot.rotated(rad)
		if stage_mouse.distance_to(spr_pos) <= hit_radius_world * 2.0:
			hovered_handle = {"type": "sprite", "index": i}
			break
	
	if prev_hover != hovered_handle:
		handles_draw.queue_redraw()


func _handle_left_click_pressed(mouse_pos: Vector2) -> void:
	var stage_mouse := stage.to_local(mouse_pos)
	_update_hovered_handle(mouse_pos)
	
	if not hovered_handle.is_empty():
		var idx: int = hovered_handle.index
		_select_mount(idx)
		
		is_dragging_handle = true
		drag_handle_mount_idx = idx
		drag_mouse_start = stage_mouse
		
		var m = mounts[idx]
		var vis := _load_weapon_cache(m.name)
		
		if edit_mode == EditMode.MOUNT or hovered_handle.type == "mount" or hovered_handle.type == "sprite":
			active_drag_type = "mount"
			drag_start_val = m.offset
		elif edit_mode == EditMode.PIVOT or hovered_handle.type == "pivot":
			active_drag_type = "pivot"
			drag_start_val = vis.get("pivot", Vector2.ZERO)
		elif edit_mode == EditMode.MUZZLE or hovered_handle.type == "muzzle":
			active_drag_type = "muzzle"
			var muz3: Vector3 = vis.get("muzzle", Vector3.ZERO)
			drag_start_val = Vector2(muz3.x, muz3.y)


func _handle_left_click_released() -> void:
	is_dragging_handle = false
	active_drag_type = ""
	handles_draw.queue_redraw()


func _handle_drag_motion(mouse_pos: Vector2) -> void:
	if drag_handle_mount_idx < 0 or drag_handle_mount_idx >= mounts.size():
		return
	
	var stage_mouse := stage.to_local(mouse_pos)
	var delta_mouse := stage_mouse - drag_mouse_start
	var rad := deg_to_rad(preview_angle_deg)
	
	if active_drag_type == "mount":
		var new_pos := (drag_start_val + delta_mouse).snapped(Vector2(0.5, 0.5))
		mounts[drag_handle_mount_idx].offset = new_pos
		spin_mount_x.set_value_no_signal(new_pos.x)
		spin_mount_y.set_value_no_signal(new_pos.y)
		_update_mount_list_ui()
		_update_turret_transforms()
		
	elif active_drag_type == "pivot":
		var w_name: String = mounts[drag_handle_mount_idx].name
		if weapon_cache.has(w_name):
			# 拖动 pivot 时，鼠标位移需要反向旋转回局部空间
			var new_pivot := (drag_start_val - delta_mouse.rotated(-rad)).snapped(Vector2(0.5, 0.5))
			weapon_cache[w_name].pivot = new_pivot
			spin_pivot_x.set_value_no_signal(new_pivot.x)
			spin_pivot_y.set_value_no_signal(new_pivot.y)
			_update_turret_transforms()
			
	elif active_drag_type == "muzzle":
		var w_name: String = mounts[drag_handle_mount_idx].name
		if weapon_cache.has(w_name):
			var new_muz := (drag_start_val + delta_mouse.rotated(-rad)).snapped(Vector2(0.5, 0.5))
			weapon_cache[w_name].muzzle.x = new_muz.x
			weapon_cache[w_name].muzzle.y = new_muz.y
			spin_muzzle_x.set_value_no_signal(new_muz.x)
			spin_muzzle_y.set_value_no_signal(new_muz.y)
	
	handles_draw.queue_redraw()


# ==============================================================================
# 自定义渲染层 (Grid & Handles)
# ==============================================================================

func _on_grid_draw() -> void:
	if not show_grid:
		return
	
	var r := 600.0
	var step := 16.0
	
	# 绘制细网格
	for x in range(int(-r), int(r) + 1, int(step)):
		var col := Color(0.2, 0.23, 0.28, 0.35 if x % 64 == 0 else 0.15)
		grid_draw.draw_line(Vector2(x, -r), Vector2(x, r), col, 1.0)
	for y in range(int(-r), int(r) + 1, int(step)):
		var col := Color(0.2, 0.23, 0.28, 0.35 if y % 64 == 0 else 0.15)
		grid_draw.draw_line(Vector2(-r, y), Vector2(r, y), col, 1.0)
	
	# 绘制坐标轴 (红X, 绿Y)
	grid_draw.draw_line(Vector2(-r, 0), Vector2(r, 0), Color(0.9, 0.3, 0.3, 0.6), 1.5)
	grid_draw.draw_line(Vector2(0, -r), Vector2(0, r), Color(0.3, 0.8, 0.4, 0.6), 1.5)
	grid_draw.draw_circle(Vector2.ZERO, 3.0, Color(1, 1, 1, 0.8))
	
	# 碰撞半径圆圈
	if show_collision_radius and current_target_data.has("collision_radius"):
		var col_r := float(current_target_data.get("collision_radius", "0"))
		if col_r > 0.0:
			grid_draw.draw_arc(Vector2.ZERO, col_r, 0.0, TAU, 64, Color(0.2, 0.8, 1.0, 0.4), 1.5)


func _on_handles_draw() -> void:
	var rad := deg_to_rad(preview_angle_deg)
	
	for i in mounts.size():
		var m = mounts[i]
		var vis := _load_weapon_cache(m.name)
		var is_sel := (i == current_mount_idx)
		
		var mount_pos: Vector2 = m.offset
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		var muz3: Vector3 = vis.get("muzzle", Vector3.ZERO)
		var muz_pos: Vector2 = mount_pos + Vector2(muz3.x, muz3.y).rotated(rad)
		var spr_pos: Vector2 = mount_pos - pivot.rotated(rad)
		
		# 1. 绘制挂载点至原点的辅助虚线
		if is_sel and mount_pos != Vector2.ZERO:
			handles_draw.draw_line(Vector2.ZERO, mount_pos, Color(0.4, 0.7, 1.0, 0.3), 1.0)
		
		# 2. 绘制挂载点标记 (Target Circle)
		var mount_col := Color(0.2, 0.7, 1.0) if is_sel else Color(0.4, 0.6, 0.8, 0.6)
		handles_draw.draw_circle(mount_pos, 4.0, mount_col)
		handles_draw.draw_arc(mount_pos, 8.0, 0.0, TAU, 24, mount_col, 1.5)
		
		# 3. 绘制旋转朝向射线与枪口位置
		var ray_len := 30.0
		var ray_end := mount_pos + Vector2.RIGHT.rotated(rad) * ray_len
		handles_draw.draw_line(mount_pos, ray_end, Color(1.0, 0.5, 0.2, 0.6 if is_sel else 0.25), 1.5)
		
		# 4. 绘制枪口 (Muzzle Handle)
		if edit_mode == EditMode.MUZZLE or is_sel:
			var muz_col := Color(1.0, 0.85, 0.2, 0.9) if is_sel else Color(0.8, 0.7, 0.3, 0.4)
			handles_draw.draw_line(mount_pos, muz_pos, Color(1.0, 0.8, 0.2, 0.4), 1.0)
			handles_draw.draw_circle(muz_pos, 3.5, muz_col)
			handles_draw.draw_arc(muz_pos, 6.5, 0.0, TAU, 16, muz_col, 1.0)
		
		# 5. 绘制旋转中心标记 (Pivot Cross)
		if edit_mode == EditMode.PIVOT or is_sel:
			var piv_col := Color(1.0, 0.25, 0.25, 0.9) if is_sel else Color(0.8, 0.3, 0.3, 0.5)
			var cross_sz := 7.0
			handles_draw.draw_line(mount_pos + Vector2(-cross_sz, 0), mount_pos + Vector2(cross_sz, 0), piv_col, 1.5)
			handles_draw.draw_line(mount_pos + Vector2(0, -cross_sz), mount_pos + Vector2(0, cross_sz), piv_col, 1.5)
		
		# 6. 文字标签标注
		if is_sel:
			var lines_arr: Array = [
				"[%d] %s" % [i, m.name],
				"Mount: (%.1f, %.1f)" % [mount_pos.x, mount_pos.y],
				"Pivot: (%.1f, %.1f)" % [pivot.x, pivot.y]
			]
			var bg_pos := mount_pos + Vector2(10, -32)
			handles_draw.draw_rect(Rect2(bg_pos, Vector2(130, 40)), Color(0, 0, 0, 0.65), true)
			for l_idx in lines_arr.size():
				handles_draw.draw_string(
					ThemeDB.fallback_font,
					bg_pos + Vector2(6, 12 + l_idx * 12),
					lines_arr[l_idx],
					HORIZONTAL_ALIGNMENT_LEFT,
					-1,
					10,
					Color(1, 1, 1, 0.95)
				)


# ==============================================================================
# 保存与写回配置
# ==============================================================================

func _save_current_unit() -> void:
	if current_target_path.is_empty():
		return
	
	var lines: PackedStringArray = FileAccess.get_file_as_string(current_target_path).split("\n")
	var new_lines: Array = []
	var mount_written: bool = false
	
	for line in lines:
		var stripped: String = line.strip_edges()
		if stripped.begins_with("weapon_mount"):
			if not mount_written:
				# 在第一条 weapon_mount 所在位置顺序写入所有挂载点
				for m in mounts:
					var m_comment: String = str(m.get("comment", "")).strip_edges()
					var comm_str: String = (" " + m_comment) if not m_comment.is_empty() else ""
					new_lines.append("weapon_mount = %s, %.1f, %.1f%s" % [m.name, m.offset.x, m.offset.y, comm_str])
				mount_written = true
			continue
		new_lines.append(line)
	
	# 如果原配置中没有 weapon_mount，直接追加到末尾
	if not mount_written and mounts.size() > 0:
		new_lines.append("")
		new_lines.append("; 武器系统挂载")
		for m in mounts:
			new_lines.append("weapon_mount = %s, %.1f, %.1f" % [m.name, m.offset.x, m.offset.y])
	
	var f := FileAccess.open(current_target_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(new_lines))
		f.close()
		for m in mounts:
			m["orig_offset"] = m.offset
		_show_toast("已保存单位配置: %s" % current_target_path.get_file())
	else:
		_show_toast("保存失败: 无法写入文件")


func _save_current_weapon() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		_show_toast("未选中有效武器")
		return
	
	var w_name: String = mounts[current_mount_idx].name
	if not weapon_cfgs.has(w_name) or not weapon_cache.has(w_name):
		return
	
	var w_path: String = weapon_cfgs[w_name].path
	var vis: Dictionary = weapon_cache[w_name]
	var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
	var muz: Vector3 = vis.get("muzzle", Vector3.ZERO)
	
	var lines := FileAccess.get_file_as_string(w_path).split("\n")
	var pivot_written := false
	var muzzle_written := false
	var new_lines: Array = []
	
	for line in lines:
		var stripped := line.strip_edges()
		if stripped.begins_with("rotation_center"):
			new_lines.append("rotation_center = %.1f, %.1f" % [pivot.x, pivot.y])
			pivot_written = true
			continue
		elif stripped.begins_with("muzzle_offset"):
			new_lines.append("muzzle_offset = %.1f, %.1f, %.1f" % [muz.x, muz.y, muz.z])
			muzzle_written = true
			continue
		new_lines.append(line)
	
	if not pivot_written:
		new_lines.append("rotation_center = %.1f, %.1f" % [pivot.x, pivot.y])
	if not muzzle_written:
		new_lines.append("muzzle_offset = %.1f, %.1f, %.1f" % [muz.x, muz.y, muz.z])
	
	var f := FileAccess.open(w_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(new_lines))
		f.close()
		_show_toast("已保存武器配置: %s" % w_path.get_file())
	else:
		_show_toast("保存失败: 无法写入武器配置")


func _save_all() -> void:
	_save_current_unit()
	_save_current_weapon()


func _reset_current_unit() -> void:
	if current_target_path.is_empty():
		return
	_load_target(current_target_path, is_current_building)
	_show_toast("已重置所有未保存改动")


func _show_toast(msg: String) -> void:
	toast_label.text = msg
	toast_label.modulate = Color(0.4, 1.0, 0.4, 1.0)
	toast_timer.start(3.0)


func _on_toast_timeout() -> void:
	var tw := create_tween()
	tw.tween_property(toast_label, "modulate:a", 0.0, 0.5)


# ==============================================================================
# 工具辅助函数
# ==============================================================================

func _get_dir_files(dir_path: String, extension: String) -> Array:
	var out: Array = []
	var d := DirAccess.open(dir_path)
	if d == null:
		return out
	d.list_dir_begin()
	var f := d.get_next()
	while f != "":
		if not d.current_is_dir() and f.ends_with(extension):
			out.append(dir_path.path_join(f))
		f = d.get_next()
	d.list_dir_end()
	out.sort()
	return out


func _parse_cfg_file(path: String) -> Dictionary:
	var d: Dictionary = {}
	if not FileAccess.file_exists(path):
		return d
	var f := FileAccess.open(path, FileAccess.READ)
	if f == null:
		return d
	while not f.eof_reached():
		var line := f.get_line().strip_edges()
		if line.is_empty() or line.begins_with(";") or line.begins_with("#") or line.begins_with("["):
			continue
		var eq := line.find("=")
		if eq <= 0:
			continue
		var k := line.substr(0, eq).strip_edges()
		var v := line.substr(eq + 1).strip_edges()
		var semi := v.find(";")
		if semi >= 0:
			v = v.substr(0, semi).strip_edges()
		d[k] = v
	f.close()
	return d
