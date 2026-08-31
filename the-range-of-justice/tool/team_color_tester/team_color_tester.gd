extends Control
## 队伍染色与 Alpha 遮罩测试器 (Team Color & Alpha Mask Tester)
## 提供：四阵营实时对比、车身+炮塔复合装配染色、像素级 Alpha 254 探针、遮罩高亮诊断、统计报表、动画播放与自定义染色测试

const UNIT_DIR := "res://config/unit"
const BUILDING_DIR := "res://config/building"
const WEAPON_DIR := "res://config/weapon"
const PREVIEW_SHADER_PATH := "res://tool/team_color_tester/team_color_preview.gdshader"

# 标准队伍颜色定义 (与 game_definitions.h 严格同步)
const TEAM_COLORS := {
	0: Color(1.0, 1.0, 1.0, 1.0), # 原色 / 白色
	1: Color(0.2, 1.0, 0.2, 1.0), # 队伍 1: 绿色
	2: Color(1.0, 0.2, 0.2, 1.0), # 队伍 2: 红色
	3: Color(1.0, 1.0, 0.2, 1.0), # 队伍 3: 黄色
	4: Color(0.2, 0.4, 1.0, 1.0), # 队伍 4: 蓝色
}

enum ViewMode {
	SPLIT_TEAMS = 0,   ## 四阵营并排对比
	SINGLE_INSPECT = 1,## 单体聚焦与像素探针
	MASK_DEBUG = 2,    ## 遮罩高亮 (品红标记 254)
	LUMINANCE = 3      ## 明度灰度分析
}

# --- 数据 ---
var target_list: Array = []        # [{type, name, path, tex_path, h_frames, v_frames, anim_fps, ...}]
var current_idx: int = -1
var current_entry: Dictionary = {}
var current_texture: Texture2D = null
var current_image: Image = null    # CPU 端像素分析用

# 武器/炮塔系统
var weapon_cfgs: Dictionary = {}   # w_name -> {path, data}
var weapon_cache: Dictionary = {}  # w_name -> {tex, h_frames, v_frames, idle_row, attacking_row, pivot}
var mounts: Array = []             # [{name, offset: Vector2}]
var show_turrets: bool = true
var turret_angle_deg: float = 0.0
var is_auto_rotate_turret: bool = false

var current_frame: int = 0
var current_row: int = 0
var is_playing_anim: bool = false
var anim_timer: float = 0.0
var anim_fps: float = 8.0

var view_mode: ViewMode = ViewMode.SPLIT_TEAMS
var custom_tint_color: Color = Color(0.9, 0.2, 0.8, 1.0) # 自定义测试颜色

# 视口缩放与平移
var canvas_zoom: float = 4.0
const ZOOM_MIN := 0.5
const ZOOM_MAX := 20.0
var canvas_pan: Vector2 = Vector2.ZERO
var is_panning: bool = false
var pan_start_mouse: Vector2 = Vector2.ZERO
var pan_start_pos: Vector2 = Vector2.ZERO
var show_grid: bool = true

# 材质缓存
var preview_shader: Shader = null
var split_materials: Array[ShaderMaterial] = []
var inspect_material: ShaderMaterial = null
var inspect_turret_materials: Array[ShaderMaterial] = []
var split_turret_materials: Array = [] # 6个队伍分别的材质数组

# --- 节点引用 ---
@onready var target_selector: OptionButton = %TargetSelector
@onready var prev_btn: Button = %PrevBtn
@onready var next_btn: Button = %NextBtn
@onready var load_file_btn: Button = %LoadFileBtn
@onready var file_dialog: FileDialog = %FileDialog

# 模式按钮
@onready var mode_split_btn: Button = %ModeSplitBtn
@onready var mode_inspect_btn: Button = %ModeInspectBtn
@onready var mode_mask_btn: Button = %ModeMaskBtn
@onready var mode_lum_btn: Button = %ModeLumBtn

# 视图容器
@onready var split_view_container: Control = %SplitViewContainer
@onready var inspect_view_container: Control = %InspectViewContainer
@onready var canvas_viewport: Control = %CanvasViewport
@onready var canvas_stage: Node2D = %CanvasStage
@onready var inspect_sprite: Sprite2D = %InspectSprite
@onready var turret_container: Node2D = %TurretContainer
@onready var grid_overlay: Node2D = %GridOverlay

# 分屏卡片节点引用
@onready var card_body_sprites: Array[Sprite2D] = [
	%CardOrigBody,
	%CardTeam1Body,
	%CardTeam2Body,
	%CardTeam3Body,
	%CardTeam4Body,
	%CardCustomBody
]

@onready var card_turret_containers: Array[Node2D] = [
	%CardOrigTurrets,
	%CardTeam1Turrets,
	%CardTeam2Turrets,
	%CardTeam3Turrets,
	%CardTeam4Turrets,
	%CardCustomTurrets
]

@onready var card_anchors: Array[Control] = [
	%CardOrigAnchor,
	%CardTeam1Anchor,
	%CardTeam2Anchor,
	%CardTeam3Anchor,
	%CardTeam4Anchor,
	%CardCustomAnchor
]

@onready var card_stages: Array[Node2D] = [
	%CardOrigStage,
	%CardTeam1Stage,
	%CardTeam2Stage,
	%CardTeam3Stage,
	%CardTeam4Stage,
	%CardCustomStage
]

# 炮塔控制 UI
@onready var turret_check: CheckButton = %TurretCheck
@onready var turret_angle_slider: HSlider = %TurretAngleSlider
@onready var turret_angle_label: Label = %TurretAngleLabel
@onready var turret_auto_rot_check: CheckButton = %TurretAutoRotCheck
@onready var turret_info_label: Label = %TurretInfoLabel

# 动画控制
@onready var frame_spin: SpinBox = %FrameSpin
@onready var row_spin: SpinBox = %RowSpin
@onready var max_frame_label: Label = %MaxFrameLabel
@onready var max_row_label: Label = %MaxRowLabel
@onready var play_btn: Button = %PlayBtn
@onready var fps_spin: SpinBox = %FpsSpin

# 像素探针 UI
@onready var probe_coord_label: Label = %ProbeCoordLabel
@onready var probe_rgba_label: Label = %ProbeRgbaLabel
@onready var probe_hsv_label: Label = %ProbeHsvLabel
@onready var probe_status_badge: Label = %ProbeStatusBadge
@onready var probe_color_rect: ColorRect = %ProbeColorRect

# 统计分析 UI
@onready var stat_size_label: Label = %StatSizeLabel
@onready var stat_opaque_label: Label = %StatOpaqueLabel
@onready var stat_mask_label: Label = %StatMaskLabel
@onready var stat_nonmask_label: Label = %StatNonMaskLabel
@onready var stat_turret_label: Label = %StatTurretLabel
@onready var stat_status_banner: Label = %StatStatusBanner

# 缩放控制
@onready var zoom_label: Label = %ZoomLabel
@onready var zoom_reset_btn: Button = %ZoomResetBtn
@onready var custom_color_picker: ColorPickerButton = %CustomColorPicker
@onready var grid_check: CheckButton = %GridCheck


func _ready() -> void:
	_init_shader_materials()
	_scan_all_weapons()
	_scan_all_targets()
	_setup_signals()
	
	if target_selector.item_count > 0:
		target_selector.select(0)
		_on_target_selected(0)
	
	_set_view_mode(ViewMode.SPLIT_TEAMS)
	_update_zoom_label()


func _process(delta: float) -> void:
	if is_playing_anim and current_entry.has("h_frames"):
		var h_f: int = current_entry.get("h_frames", 1)
		if h_f > 1:
			anim_timer += delta
			var frame_duration: float = 1.0 / maxf(anim_fps, 1.0)
			if anim_timer >= frame_duration:
				anim_timer = fmod(anim_timer, frame_duration)
				current_frame = (current_frame + 1) % h_f
				frame_spin.set_value_no_signal(current_frame)
				_update_frame_uniforms()
	
	if is_auto_rotate_turret and show_turrets and mounts.size() > 0:
		turret_angle_deg = fmod(turret_angle_deg + delta * 90.0, 360.0)
		turret_angle_slider.set_value_no_signal(turret_angle_deg)
		turret_angle_label.text = "%.1f°" % turret_angle_deg
		_update_turret_transforms()


func _init_shader_materials() -> void:
	if ResourceLoader.exists(PREVIEW_SHADER_PATH):
		preview_shader = load(PREVIEW_SHADER_PATH)
	
	# 初始化四阵营材质
	split_materials.clear()
	split_turret_materials.clear()
	for i in range(6): # 0:原图, 1:绿, 2:红, 3:黄, 4:蓝, 5:自定义
		var mat: ShaderMaterial = ShaderMaterial.new()
		mat.shader = preview_shader
		var team_col: Color = TEAM_COLORS.get(i, custom_tint_color) if i < 5 else custom_tint_color
		mat.set_shader_parameter("team_color", team_col)
		mat.set_shader_parameter("display_mode", 1 if i == 0 else 0)
		split_materials.append(mat)
		
		var t_mats: Array[ShaderMaterial] = []
		split_turret_materials.append(t_mats)
	
	# 单体聚焦材质
	inspect_material = ShaderMaterial.new()
	inspect_material.shader = preview_shader
	inspect_material.set_shader_parameter("team_color", TEAM_COLORS[1])
	inspect_material.set_shader_parameter("display_mode", 0)
	if inspect_sprite:
		inspect_sprite.material = inspect_material


# ==============================================================================
# 数据扫描与加载
# ==============================================================================

func _scan_all_weapons() -> void:
	weapon_cfgs.clear()
	weapon_cache.clear()
	for path: String in _get_dir_files(WEAPON_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg(path)
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
	
	var h_f: int = int(d.get("h_frames", "1"))
	var v_f: int = int(d.get("v_frames", "1"))
	var idle_r: int = int(d.get("idle_row", "0"))
	var atk_r: int = int(d.get("attacking_row", "0"))
	
	var pivot_str: String = str(d.get("rotation_center", "0,0"))
	var pivot_parts: PackedStringArray = pivot_str.split(",", false)
	var pivot: Vector2 = Vector2(
		float(pivot_parts[0]) if pivot_parts.size() >= 1 else 0.0,
		float(pivot_parts[1]) if pivot_parts.size() >= 2 else 0.0
	)
	
	var entry: Dictionary = {
		"name": w_name,
		"tex": tex,
		"tex_path": tex_path,
		"h_frames": maxi(1, h_f),
		"v_frames": maxi(1, v_f),
		"idle_row": idle_r,
		"attacking_row": atk_r,
		"pivot": pivot
	}
	weapon_cache[w_name] = entry
	return entry


func _scan_all_targets() -> void:
	target_list.clear()
	target_selector.clear()
	
	# 1. 扫描兵种单位
	for path: String in _get_dir_files(UNIT_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg(path)
		var uname: String = str(d.get("unit_name", path.get_file().get_basename()))
		target_list.append({
			"type": "Unit",
			"name": uname,
			"path": path,
			"tex_path": str(d.get("texture_path", "")),
			"h_frames": int(d.get("h_frames", "1")),
			"v_frames": int(d.get("v_frames", "1")),
			"anim_fps": float(d.get("anim_fps", "10")),
			"display": "[单位] " + uname
		})
	
	# 2. 扫描建筑
	for path: String in _get_dir_files(BUILDING_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg(path)
		var bname: String = str(d.get("building_name", path.get_file().get_basename()))
		target_list.append({
			"type": "Building",
			"name": bname,
			"path": path,
			"tex_path": str(d.get("texture_path", "")),
			"h_frames": int(d.get("h_frames", "1")),
			"v_frames": int(d.get("v_frames", "1")),
			"anim_fps": float(d.get("anim_fps", "8")),
			"display": "[建筑] " + bname
		})
	
	# 3. 扫描武器/炮塔
	for path: String in _get_dir_files(WEAPON_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg(path)
		var wname: String = str(d.get("weapon_name", path.get_file().get_basename()))
		target_list.append({
			"type": "Weapon",
			"name": wname,
			"path": path,
			"tex_path": str(d.get("texture_path", "")),
			"h_frames": int(d.get("h_frames", "1")),
			"v_frames": int(d.get("v_frames", "1")),
			"anim_fps": float(d.get("anim_fps", "8")),
			"display": "[武器] " + wname
		})
	
	for entry: Dictionary in target_list:
		target_selector.add_item(entry.display)


func _load_target_entry(entry: Dictionary) -> void:
	current_entry = entry
	var tex_path: String = entry.get("tex_path", "")
	var cfg_path: String = entry.get("path", "")
	
	current_texture = null
	current_image = null
	
	if ResourceLoader.exists(tex_path):
		current_texture = load(tex_path)
		if current_texture:
			current_image = current_texture.get_image()
			if current_image:
				current_image.decompress()
	
	# 解析挂载武器/炮塔
	mounts = _parse_all_mounts(cfg_path)
	_update_turret_info_ui()
	
	current_frame = 0
	current_row = 0
	anim_fps = float(entry.get("anim_fps", 8.0))
	fps_spin.set_value_no_signal(anim_fps)
	
	var h_f: int = maxi(1, int(entry.get("h_frames", 1)))
	var v_f: int = maxi(1, int(entry.get("v_frames", 1)))
	
	frame_spin.max_value = h_f - 1
	frame_spin.value = 0
	max_frame_label.text = "/ %d" % h_f
	
	row_spin.max_value = v_f - 1
	row_spin.value = 0
	max_row_label.text = "/ %d" % v_f
	
	_rebuild_turrets()
	_update_frame_uniforms()
	_update_card_stages_positions()
	_run_pixel_statistics()
	_reset_canvas_view()


func _parse_all_mounts(path: String) -> Array:
	var out: Array = []
	if not FileAccess.file_exists(path):
		return out
	var lines: PackedStringArray = FileAccess.get_file_as_string(path).split("\n")
	for i in lines.size():
		var line: String = lines[i].strip_edges()
		if not line.begins_with("weapon_mount"):
			continue
		var eq: int = line.find("=")
		if eq <= 0:
			continue
		var val: String = line.substr(eq + 1).strip_edges()
		var semi: int = val.find(";")
		if semi >= 0:
			val = val.substr(0, semi).strip_edges()
		var parts: PackedStringArray = val.split(",", false)
		if parts.size() >= 1:
			var w_name: String = parts[0].strip_edges()
			var ox: float = float(parts[1]) if parts.size() >= 2 else 0.0
			var oy: float = float(parts[2]) if parts.size() >= 3 else 0.0
			out.append({
				"name": w_name,
				"offset": Vector2(ox, oy)
			})
	return out


func _update_turret_info_ui() -> void:
	if mounts.size() > 0:
		var names: Array[String] = []
		for m: Dictionary in mounts:
			names.append(m.name)
		turret_info_label.text = "🎯 挂载武器: " + ", ".join(names)
		turret_check.disabled = false
		turret_angle_slider.editable = true
		turret_auto_rot_check.disabled = false
	else:
		turret_info_label.text = "🎯 挂载武器: 无"
		turret_check.disabled = true
		turret_angle_slider.editable = false
		turret_auto_rot_check.disabled = true


# ==============================================================================
# 炮塔与车体重构 (对齐 Turret Positioner 几何逻辑)
# ==============================================================================

func _rebuild_turrets() -> void:
	# 1. 重建单体聚焦视口中的炮塔
	for child: Node in turret_container.get_children():
		child.queue_free()
	inspect_turret_materials.clear()
	
	for i in mounts.size():
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var spr: Sprite2D = Sprite2D.new()
		spr.name = "InspectTurret_%d" % i
		spr.texture = vis.get("tex", null)
		spr.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		
		if spr.texture:
			spr.region_enabled = true
			var th_f: int = vis.get("h_frames", 1)
			var tv_f: int = vis.get("v_frames", 1)
			var tfw: float = spr.texture.get_width() / float(th_f)
			var tfh: float = spr.texture.get_height() / float(tv_f)
			var row: int = vis.get("idle_row", 0)
			spr.region_rect = Rect2(0, row * tfh, tfw, tfh)
		spr.centered = true
		
		var t_mat: ShaderMaterial = ShaderMaterial.new()
		t_mat.shader = preview_shader
		var shader_mode: int = 0
		if view_mode == ViewMode.MASK_DEBUG:
			shader_mode = 2
		elif view_mode == ViewMode.LUMINANCE:
			shader_mode = 3
		t_mat.set_shader_parameter("team_color", custom_tint_color if view_mode == ViewMode.SINGLE_INSPECT else TEAM_COLORS[1])
		t_mat.set_shader_parameter("display_mode", shader_mode)
		
		spr.material = t_mat
		inspect_turret_materials.append(t_mat)
		turret_container.add_child(spr)
	
	# 2. 重建多阵营对比卡片上的炮塔
	for team_idx in range(card_turret_containers.size()):
		var c_cont: Node2D = card_turret_containers[team_idx]
		for child: Node in c_cont.get_children():
			child.queue_free()
		split_turret_materials[team_idx].clear()
		
		for m_idx in mounts.size():
			var m: Dictionary = mounts[m_idx]
			var vis: Dictionary = _load_weapon_cache(m.name)
			var spr: Sprite2D = Sprite2D.new()
			spr.name = "Turret_%d" % m_idx
			spr.texture = vis.get("tex", null)
			spr.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
			
			if spr.texture:
				spr.region_enabled = true
				var th_f: int = vis.get("h_frames", 1)
				var tv_f: int = vis.get("v_frames", 1)
				var tfw: float = spr.texture.get_width() / float(th_f)
				var tfh: float = spr.texture.get_height() / float(tv_f)
				var row: int = vis.get("idle_row", 0)
				spr.region_rect = Rect2(0, row * tfh, tfw, tfh)
			spr.centered = true
			
			var t_mat: ShaderMaterial = ShaderMaterial.new()
			t_mat.shader = preview_shader
			var team_col: Color = TEAM_COLORS.get(team_idx, custom_tint_color) if team_idx < 5 else custom_tint_color
			t_mat.set_shader_parameter("team_color", team_col)
			t_mat.set_shader_parameter("display_mode", 1 if team_idx == 0 else 0)
			
			spr.material = t_mat
			split_turret_materials[team_idx].append(t_mat)
			c_cont.add_child(spr)
	
	_update_turret_transforms()


func _update_turret_transforms() -> void:
	var rad: float = deg_to_rad(turret_angle_deg)
	
	# 1. 单体视图更新 (与 Turret Positioner 几何完全一致: m.offset - pivot.rotated(rad))
	var children: Array[Node] = turret_container.get_children()
	for i in children.size():
		if i >= mounts.size(): continue
		var spr: Sprite2D = children[i] as Sprite2D
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		
		spr.visible = show_turrets
		spr.rotation = rad
		spr.position = m.offset - pivot.rotated(rad)
	
	# 2. 分屏卡片更新
	for c_cont: Node2D in card_turret_containers:
		var c_children: Array[Node] = c_cont.get_children()
		for i in c_children.size():
			if i >= mounts.size(): continue
			var spr: Sprite2D = c_children[i] as Sprite2D
			var m: Dictionary = mounts[i]
			var vis: Dictionary = _load_weapon_cache(m.name)
			var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
			
			spr.visible = show_turrets
			spr.rotation = rad
			spr.position = m.offset - pivot.rotated(rad)


func _update_frame_uniforms() -> void:
	if not current_texture:
		inspect_sprite.texture = null
		for spr: Sprite2D in card_body_sprites:
			spr.texture = null
		return
	
	var h_f: int = maxi(1, int(current_entry.get("h_frames", 1)))
	var v_f: int = maxi(1, int(current_entry.get("v_frames", 1)))
	var fw: float = current_texture.get_width() / float(h_f)
	var fh: float = current_texture.get_height() / float(v_f)
	var rect := Rect2(current_frame * fw, current_row * fh, fw, fh)
	
	# 1. 更新 Inspect 聚焦车体
	inspect_sprite.texture = current_texture
	inspect_sprite.region_enabled = true
	inspect_sprite.region_rect = rect
	inspect_sprite.centered = true
	inspect_sprite.position = Vector2.ZERO
	inspect_sprite.material = inspect_material
	
	# 2. 更新 Split 卡片车体
	for i in range(card_body_sprites.size()):
		var spr: Sprite2D = card_body_sprites[i]
		spr.texture = current_texture
		spr.region_enabled = true
		spr.region_rect = rect
		spr.centered = true
		spr.position = Vector2.ZERO
		spr.material = split_materials[i]
	
	grid_overlay.queue_redraw()


func _update_card_stages_positions() -> void:
	for i in range(card_anchors.size()):
		var anchor: Control = card_anchors[i]
		var stage: Node2D = card_stages[i]
		if anchor and stage:
			stage.position = anchor.size * 0.5


# ==============================================================================
# 统计与像素分析
# ==============================================================================

func _run_pixel_statistics() -> void:
	if not current_image:
		stat_size_label.text = "贴图: 无"
		stat_opaque_label.text = "不透明像素: 0"
		stat_mask_label.text = "Alpha 254 染色区: 0 (0%)"
		stat_nonmask_label.text = "Alpha 255 原色区: 0 (0%)"
		stat_turret_label.text = "挂载炮塔: 无"
		stat_status_banner.text = "❌ 未加载有效贴图"
		stat_status_banner.modulate = Color(1.0, 0.4, 0.4)
		return
	
	var w: int = current_image.get_width()
	var h: int = current_image.get_height()
	var total_pixels: int = w * h
	var opaque_count: int = 0
	var mask_254_count: int = 0
	var nonmask_255_count: int = 0
	var semitrans_count: int = 0
	
	for py in range(h):
		for px in range(w):
			var col: Color = current_image.get_pixel(px, py)
			var a8: int = int(round(col.a * 255.0))
			if a8 == 0:
				continue
			opaque_count += 1
			if a8 == 254:
				mask_254_count += 1
			elif a8 == 255:
				nonmask_255_count += 1
			else:
				semitrans_count += 1
	
	stat_size_label.text = "车体尺寸: %d × %d (共 %d px)" % [w, h, total_pixels]
	stat_opaque_label.text = "车体实心像素: %d" % opaque_count
	
	var mask_pct: float = (float(mask_254_count) / float(maxi(1, opaque_count))) * 100.0
	var nonmask_pct: float = (float(nonmask_255_count) / float(maxi(1, opaque_count))) * 100.0
	
	stat_mask_label.text = "Alpha 254 染色区: %d px (%.1f%%)" % [mask_254_count, mask_pct]
	stat_nonmask_label.text = "Alpha 255 原色区: %d px (%.1f%%)" % [nonmask_255_count, nonmask_pct]
	
	if mounts.size() > 0:
		var t_m: Dictionary = mounts[0]
		var t_vis: Dictionary = _load_weapon_cache(t_m.name)
		var t_tex: Texture2D = t_vis.get("tex", null)
		var t_mask_count: int = 0
		if t_tex:
			var t_img: Image = t_tex.get_image()
			if t_img:
				t_img.decompress()
				for py in range(t_img.get_height()):
					for px in range(t_img.get_width()):
						var col: Color = t_img.get_pixel(px, py)
						if int(round(col.a * 255.0)) == 254:
							t_mask_count += 1
		stat_turret_label.text = "挂载炮塔: %d 个 (%s, 染色区: %d px)" % [mounts.size(), t_m.name, t_mask_count]
	else:
		stat_turret_label.text = "挂载炮塔: 0 个"
	
	if mask_254_count > 0:
		stat_status_banner.text = "✅ 染色遮罩配置正常 (共 %d 像素支持变色)" % mask_254_count
		stat_status_banner.modulate = Color(0.3, 1.0, 0.3)
	else:
		stat_status_banner.text = "⚠️ 未检测到 Alpha 254 像素 (请在 Aseprite 运行 TeamColorMask.lua)"
		stat_status_banner.modulate = Color(1.0, 0.7, 0.2)


# ==============================================================================
# 交互与探针
# ==============================================================================

func _on_canvas_gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb: InputEventMouseButton = event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_MIDDLE or mb.button_index == MOUSE_BUTTON_RIGHT:
			if mb.pressed:
				is_panning = true
				pan_start_mouse = mb.position
				pan_start_pos = canvas_pan
			else:
				is_panning = false
		elif mb.button_index == MOUSE_BUTTON_WHEEL_UP and mb.pressed:
			_zoom_at(mb.position, 1.15)
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and mb.pressed:
			_zoom_at(mb.position, 1.0 / 1.15)
	
	elif event is InputEventMouseMotion:
		var mm: InputEventMouseMotion = event as InputEventMouseMotion
		if is_panning:
			canvas_pan = pan_start_pos + (mm.position - pan_start_mouse)
			_update_canvas_transform()
		_update_pixel_probe(mm.position)


func _update_pixel_probe(mouse_viewport_pos: Vector2) -> void:
	if not current_image or not current_texture:
		return
	
	var local_pos: Vector2 = canvas_stage.to_local(mouse_viewport_pos + canvas_viewport.global_position)
	
	var h_f: int = maxi(1, int(current_entry.get("h_frames", 1)))
	var v_f: int = maxi(1, int(current_entry.get("v_frames", 1)))
	var frame_w: float = current_texture.get_width() / float(h_f)
	var frame_h: float = current_texture.get_height() / float(v_f)
	
	var pixel_in_frame_x: int = int(floor(local_pos.x + frame_w * 0.5))
	var pixel_in_frame_y: int = int(floor(local_pos.y + frame_h * 0.5))
	
	if pixel_in_frame_x < 0 or pixel_in_frame_x >= int(frame_w) or pixel_in_frame_y < 0 or pixel_in_frame_y >= int(frame_h):
		probe_coord_label.text = "坐标: (超出贴图)"
		probe_status_badge.text = "[-]"
		probe_status_badge.modulate = Color(0.7, 0.7, 0.7)
		return
	
	var atlas_x: int = current_frame * int(frame_w) + pixel_in_frame_x
	var atlas_y: int = current_row * int(frame_h) + pixel_in_frame_y
	
	atlas_x = clampi(atlas_x, 0, current_image.get_width() - 1)
	atlas_y = clampi(atlas_y, 0, current_image.get_height() - 1)
	
	var col: Color = current_image.get_pixel(atlas_x, atlas_y)
	var r8: int = int(round(col.r * 255.0))
	var g8: int = int(round(col.g * 255.0))
	var b8: int = int(round(col.b * 255.0))
	var a8: int = int(round(col.a * 255.0))
	
	probe_coord_label.text = "帧内: (%d, %d) | 图集: (%d, %d)" % [pixel_in_frame_x, pixel_in_frame_y, atlas_x, atlas_y]
	probe_rgba_label.text = "RGBA: (%d, %d, %d, %d)" % [r8, g8, b8, a8]
	probe_hsv_label.text = "HSV: (H: %.1f°, S: %.1f%%, V: %.1f%%)" % [col.h * 360.0, col.s * 100.0, col.v * 100.0]
	probe_color_rect.color = Color(col.r, col.g, col.b, 1.0)
	
	if a8 == 0:
		probe_status_badge.text = "⬛ 完全透明 (Alpha 0)"
		probe_status_badge.modulate = Color(0.6, 0.6, 0.6)
	elif a8 == 254:
		probe_status_badge.text = "✅ 团队染色区 (Alpha 254)"
		probe_status_badge.modulate = Color(0.2, 1.0, 0.4)
	elif a8 == 255:
		probe_status_badge.text = "⚪ 普通原色区 (Alpha 255)"
		probe_status_badge.modulate = Color(0.4, 0.8, 1.0)
	else:
		probe_status_badge.text = "⚠️ 半透明像素 (Alpha %d)" % a8
		probe_status_badge.modulate = Color(1.0, 0.8, 0.2)


func _zoom_at(mouse_pos: Vector2, factor: float) -> void:
	var old_zoom: float = canvas_zoom
	canvas_zoom = clamp(canvas_zoom * factor, ZOOM_MIN, ZOOM_MAX)
	var stage_center: Vector2 = canvas_viewport.size * 0.5
	canvas_pan = (canvas_pan - (mouse_pos - stage_center)) * (canvas_zoom / old_zoom) + (mouse_pos - stage_center)
	_update_canvas_transform()
	_update_zoom_label()


func _update_canvas_transform() -> void:
	if not canvas_stage or not canvas_viewport: return
	var center: Vector2 = canvas_viewport.size * 0.5
	canvas_stage.position = center + canvas_pan
	canvas_stage.scale = Vector2(canvas_zoom, canvas_zoom)
	grid_overlay.queue_redraw()


func _reset_canvas_view() -> void:
	canvas_pan = Vector2.ZERO
	canvas_zoom = 4.0
	_update_canvas_transform()
	_update_zoom_label()
	_update_card_stages_positions()


func _update_zoom_label() -> void:
	if zoom_label:
		zoom_label.text = "x%.1f" % canvas_zoom


func _set_view_mode(mode: ViewMode) -> void:
	view_mode = mode
	mode_split_btn.button_pressed = (mode == ViewMode.SPLIT_TEAMS)
	mode_inspect_btn.button_pressed = (mode == ViewMode.SINGLE_INSPECT)
	mode_mask_btn.button_pressed = (mode == ViewMode.MASK_DEBUG)
	mode_lum_btn.button_pressed = (mode == ViewMode.LUMINANCE)
	
	if mode == ViewMode.SPLIT_TEAMS:
		split_view_container.show()
		inspect_view_container.hide()
		_update_card_stages_positions()
	else:
		split_view_container.hide()
		inspect_view_container.show()
		
		var shader_mode: int = 0
		if mode == ViewMode.SINGLE_INSPECT:
			shader_mode = 0 # 队伍染色
		elif mode == ViewMode.MASK_DEBUG:
			shader_mode = 2 # 遮罩高亮
		elif mode == ViewMode.LUMINANCE:
			shader_mode = 3 # 明度分析
		
		if inspect_material:
			inspect_material.set_shader_parameter("display_mode", shader_mode)
			inspect_material.set_shader_parameter("team_color", custom_tint_color)
		
		for t_mat: ShaderMaterial in inspect_turret_materials:
			t_mat.set_shader_parameter("display_mode", shader_mode)
			t_mat.set_shader_parameter("team_color", custom_tint_color)


# ==============================================================================
# UI 信号绑定
# ==============================================================================

func _setup_signals() -> void:
	target_selector.item_selected.connect(_on_target_selected)
	prev_btn.pressed.connect(func():
		var new_idx: int = (current_idx - 1 + target_list.size()) % maxi(1, target_list.size())
		target_selector.select(new_idx)
		_on_target_selected(new_idx)
	)
	next_btn.pressed.connect(func():
		var new_idx: int = (current_idx + 1) % maxi(1, target_list.size())
		target_selector.select(new_idx)
		_on_target_selected(new_idx)
	)
	
	load_file_btn.pressed.connect(func():
		file_dialog.popup_centered_ratio(0.7)
	)
	file_dialog.file_selected.connect(_on_external_file_selected)
	
	mode_split_btn.pressed.connect(func(): _set_view_mode(ViewMode.SPLIT_TEAMS))
	mode_inspect_btn.pressed.connect(func(): _set_view_mode(ViewMode.SINGLE_INSPECT))
	mode_mask_btn.pressed.connect(func(): _set_view_mode(ViewMode.MASK_DEBUG))
	mode_lum_btn.pressed.connect(func(): _set_view_mode(ViewMode.LUMINANCE))
	
	turret_check.toggled.connect(func(on: bool):
		show_turrets = on
		_update_turret_transforms()
	)
	turret_angle_slider.value_changed.connect(func(val: float):
		turret_angle_deg = val
		turret_angle_label.text = "%.1f°" % val
		_update_turret_transforms()
	)
	turret_auto_rot_check.toggled.connect(func(on: bool):
		is_auto_rotate_turret = on
	)
	
	frame_spin.value_changed.connect(func(v: float):
		current_frame = int(v)
		_update_frame_uniforms()
	)
	row_spin.value_changed.connect(func(v: float):
		current_row = int(v)
		_update_frame_uniforms()
	)
	play_btn.toggled.connect(func(on: bool):
		is_playing_anim = on
		play_btn.text = "⏸ 暂停" if on else "▶ 播放"
	)
	fps_spin.value_changed.connect(func(v: float):
		anim_fps = v
	)
	
	zoom_reset_btn.pressed.connect(_reset_canvas_view)
	
	custom_color_picker.color_changed.connect(func(c: Color):
		custom_tint_color = c
		if split_materials.size() >= 6:
			split_materials[5].set_shader_parameter("team_color", c)
		if split_turret_materials.size() >= 6:
			for t_mat: ShaderMaterial in split_turret_materials[5]:
				t_mat.set_shader_parameter("team_color", c)
		if inspect_material and view_mode == ViewMode.SINGLE_INSPECT:
			inspect_material.set_shader_parameter("team_color", c)
		for t_mat: ShaderMaterial in inspect_turret_materials:
			if view_mode == ViewMode.SINGLE_INSPECT:
				t_mat.set_shader_parameter("team_color", c)
	)
	
	grid_check.toggled.connect(func(on: bool):
		show_grid = on
		grid_overlay.queue_redraw()
	)
	
	canvas_viewport.gui_input.connect(_on_canvas_gui_input)
	canvas_viewport.resized.connect(_update_canvas_transform)
	
	for anchor: Control in card_anchors:
		if anchor:
			anchor.resized.connect(_update_card_stages_positions)


func _on_target_selected(idx: int) -> void:
	if idx < 0 or idx >= target_list.size(): return
	current_idx = idx
	_load_target_entry(target_list[idx])


func _on_external_file_selected(path: String) -> void:
	var entry: Dictionary = {
		"type": "CustomFile",
		"name": path.get_file().get_basename(),
		"path": path,
		"tex_path": path,
		"h_frames": 1,
		"v_frames": 1,
		"anim_fps": 8.0,
		"display": "[外部文件] " + path.get_file()
	}
	target_list.append(entry)
	target_selector.add_item(entry.display)
	target_selector.select(target_list.size() - 1)
	_on_target_selected(target_list.size() - 1)


func _on_grid_overlay_draw() -> void:
	if not show_grid or not current_texture: return
	var h_f: int = maxi(1, int(current_entry.get("h_frames", 1)))
	var v_f: int = maxi(1, int(current_entry.get("v_frames", 1)))
	var frame_w: float = current_texture.get_width() / float(h_f)
	var frame_h: float = current_texture.get_height() / float(v_f)
	
	var rect: Rect2 = Rect2(-frame_w * 0.5, -frame_h * 0.5, frame_w, frame_h)
	# 边界框
	grid_overlay.draw_rect(rect, Color(0.3, 0.7, 1.0, 0.4), false, 1.0 / max(1.0, canvas_zoom))
	# 中心十字准星
	grid_overlay.draw_line(Vector2(-6, 0), Vector2(6, 0), Color(1.0, 0.3, 0.3, 0.7), 1.0 / max(1.0, canvas_zoom))
	grid_overlay.draw_line(Vector2(0, -6), Vector2(0, 6), Color(0.3, 1.0, 0.3, 0.7), 1.0 / max(1.0, canvas_zoom))


# ==============================================================================
# 文件与配置解析工具
# ==============================================================================

func _get_dir_files(dir_path: String, extension: String) -> Array[String]:
	var result: Array[String] = []
	var dir: DirAccess = DirAccess.open(dir_path)
	if not dir: return result
	dir.list_dir_begin()
	var file_name: String = dir.get_next()
	while file_name != "":
		if not dir.current_is_dir() and file_name.ends_with(extension):
			result.append(dir_path.path_join(file_name))
		file_name = dir.get_next()
	dir.list_dir_end()
	result.sort()
	return result


func _parse_cfg(path: String) -> Dictionary:
	var dict: Dictionary = {}
	var file: FileAccess = FileAccess.open(path, FileAccess.READ)
	if not file: return dict
	while not file.eof_reached():
		var line: String = file.get_line().strip_edges()
		if line.is_empty() or line.begins_with(";") or line.begins_with("#"):
			continue
		var eq: int = line.find("=")
		if eq != -1:
			var key: String = line.substr(0, eq).strip_edges()
			var val: String = line.substr(eq + 1).strip_edges()
			dict[key] = val
	return dict
