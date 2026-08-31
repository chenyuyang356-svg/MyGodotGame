extends Control
## 炮塔与武器定位编辑器 (Turret & Weapon Positioner)
## 支持：挂载点拖拽微调、旋转中心(Pivot)标定、枪口(Muzzle)标定、实时旋转与开火预览、多炮塔管理与镜像

const UNIT_DIR := "res://config/unit"
const BUILDING_DIR := "res://config/building"
const WEAPON_DIR := "res://config/weapon"
const PROJECTILE_DIR := "res://config/projectile"
const PROJ_TEXTURE_DIR := "res://asset/projectile"

enum EditMode {
	MOUNT = 0,    ## 调整挂载点位置 (Unit cfg: weapon_mount)
	PIVOT = 1,    ## 调整武器旋转中心 (Weapon cfg: rotation_center)
	MUZZLE = 2,   ## 调整枪口发射位置 (Weapon cfg: muzzle_offset)
	TARGET = 3    ## 调整目标标靶位置 (Target Dummy)
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
var weapon_anim_time: float = 0.0
var preview_time_scale: float = 1.0
var is_anim_paused: bool = false
var current_body_frame: int = 0
var show_grid: bool = true
var show_collision_radius: bool = true
var show_turrets: bool = true

# --- 目标标靶状态 (Target Dummy) ---
var target_dummy_enabled: bool = true
var target_pos: Vector2 = Vector2(140.0, 0.0)
var target_height: float = 0.0
var target_radius: float = 14.0
var is_target_patrolling: bool = false
var target_patrol_time: float = 0.0
var target_hit_flash_timer: float = 0.0
var target_hit_count: int = 0
var is_aiming_at_target: bool = false
var target_tex: Texture2D = null

# --- 投射物系统状态 (Projectile System) ---
var proj_params: Dictionary = {
	"type": 0,                     # 0=Bullet (直线), 1=Shell (抛物线), 2=Missile (追踪)
	"preset_name": "MarineBullet",
	"tex_name": "bullet.png",
	"speed": 800.0,
	"arc_height": 15.0,
	"acceleration": 300.0,
	"turn_speed": 6.0,
	"splash_radius": 0.0,
	"damage": 10.0,
	"scale": 1.0,
	"is_healing": false,
	"trail_enabled": true,
	"shadow_enabled": true
}

var proj_enable_firing: bool = true
var proj_auto_fire: bool = false
var proj_fire_interval: float = 0.4
var proj_auto_fire_timer: float = 0.0
var proj_sync_anim: bool = true
var proj_all_mounts: bool = true

# 缓存与实例
var proj_tex_cache: Dictionary = {}        # filename -> Texture2D
var proj_configs: Dictionary = {}          # name -> parsed dict from config/projectile/*.cfg
var active_projectiles: Array = []         # Array of Dictionary
var active_impact_effects: Array = []      # Array of Dictionary
var floating_texts: Array = []             # Array of Dictionary
var last_anim_cycle_count: int = -1
var has_fired_this_anim_cycle: bool = false

# --- 炮口闪光预览状态 (参数与 shader/particle_add.gdshader 的 uniform 一一对应) ---
var flash_preview_enabled: bool = false
var flash_all_mounts: bool = true
var flash_time: float = 0.0
var flash_params: Dictionary = {
	"color": Color(1.0, 0.55, 0.10),  # 基础主色 (炽橙)
	"pixel_size": 1.0,                 # 像素网格块大小 (1.0对齐16px像素网格)
	"palette_type": 0,                 # 调色板类型 (0=火炮, 1=机枪, 2=重炮, 3=能量)
	"core_radius": 0.35,               # 白热亮核尺寸 (0~1)
	"cone_length": 0.90,               # 前向主火舌长度 (0~1.5)
	"side_strength": 0.50,             # 侧向制退器火羽强度 (0~1)
	"side_angle_deg": 50.0,            # 制退器喷流夹角 (度数, 30~90)
	"side_length": 0.60,               # 制退器火羽长度 (0~1)
	"spark_intensity": 0.40,           # 飞溅方块火星数量 (0~1)
	"scale": 9.0,                      # 世界尺寸 (像素宽度)
	"life": 0.12,                      # 寿命秒数
	"forward_speed": 32.0,             # 前冲速度 (模拟游戏内初速度)
}

const FLASH_PRESETS: Dictionary = {
	"cannon": {
		"color": Color(1.0, 0.55, 0.10),
		"pixel_size": 1.0,
		"palette_type": 0,
		"core_radius": 0.35,
		"cone_length": 0.90,
		"side_strength": 0.50,
		"side_angle_deg": 50.0,
		"side_length": 0.60,
		"spark_intensity": 0.40,
		"scale": 9.0,
		"life": 0.12,
		"forward_speed": 32.0
	},
	"autogun": {
		"color": Color(1.0, 0.72, 0.18),
		"pixel_size": 1.0,
		"palette_type": 1,
		"core_radius": 0.25,
		"cone_length": 0.70,
		"side_strength": 0.25,
		"side_angle_deg": 45.0,
		"side_length": 0.40,
		"spark_intensity": 0.50,
		"scale": 5.0,
		"life": 0.07,
		"forward_speed": 40.0
	},
	"heavy": {
		"color": Color(1.0, 0.45, 0.08),
		"pixel_size": 1.5,
		"palette_type": 0,
		"core_radius": 0.40,
		"cone_length": 1.15,
		"side_strength": 0.65,
		"side_angle_deg": 55.0,
		"side_length": 0.75,
		"spark_intensity": 0.60,
		"scale": 16.0,
		"life": 0.18,
		"forward_speed": 45.0
	},
	"energy": {
		"color": Color(0.20, 0.60, 1.0),
		"pixel_size": 1.0,
		"palette_type": 3,
		"core_radius": 0.45,
		"cone_length": 0.85,
		"side_strength": 0.40,
		"side_angle_deg": 90.0,
		"side_length": 0.55,
		"spark_intensity": 0.60,
		"scale": 8.0,
		"life": 0.10,
		"forward_speed": 15.0
	},
	"missile": {
		"color": Color(1.0, 0.65, 0.20),
		"pixel_size": 1.0,
		"palette_type": 2,
		"core_radius": 0.40,
		"cone_length": 1.20,
		"side_strength": 0.70,
		"side_angle_deg": 120.0,
		"side_length": 0.85,
		"spark_intensity": 0.75,
		"scale": 14.0,
		"life": 0.25,
		"forward_speed": 48.0
	}
}

# 视口变换
var canvas_zoom: float = 4.0
const ZOOM_MIN := 0.25
const ZOOM_MAX := 20.0
var canvas_pan: Vector2 = Vector2.ZERO
var is_panning: bool = false
var pan_start_mouse: Vector2 = Vector2.ZERO
var pan_start_pos: Vector2 = Vector2.ZERO

# 拖拽句柄
var is_dragging_handle: bool = false
var active_drag_type: String = ""         # "mount", "pivot", "muzzle"
var drag_handle_mount_idx: int = -1
var drag_handle_muzzle_idx: int = -1
var drag_start_val: Vector2 = Vector2.ZERO
var drag_mouse_start: Vector2 = Vector2.ZERO
var pivot_drag_anchor: Vector2 = Vector2.ZERO
var pivot_drag_rad: float = 0.0
var pivot_edit_active: bool = false
var hovered_handle: Dictionary = {}       # {type, index}

# --- 节点引用 ---
@onready var stage_container: Control = $MainVBox/BodyHBox/StageContainer
@onready var stage: Node2D = $MainVBox/BodyHBox/StageContainer/Stage
@onready var grid_draw: Node2D = $MainVBox/BodyHBox/StageContainer/Stage/GridDraw
@onready var body_sprite: Sprite2D = $MainVBox/BodyHBox/StageContainer/Stage/BodySprite
@onready var turret_container: Node2D = $MainVBox/BodyHBox/StageContainer/Stage/TurretContainer
@onready var handles_draw: Node2D = $MainVBox/BodyHBox/StageContainer/Stage/HandlesDraw

# 顶部工具栏引用
@onready var unit_selector: OptionButton = %UnitSelector
@onready var prev_unit_btn: Button = %PrevUnitBtn
@onready var next_unit_btn: Button = %NextUnitBtn
@onready var body_frame_hbox: HBoxContainer = %BodyFrameHBox
@onready var spin_body_frame: SpinBox = %SpinBodyFrame
@onready var mode_mount_btn: Button = %ModeMountBtn
@onready var mode_pivot_btn: Button = %ModePivotBtn
@onready var mode_muzzle_btn: Button = %ModeMuzzleBtn
@onready var mode_target_btn: Button = %ModeTargetBtn

@onready var save_all_btn: Button = %SaveAllBtn
@onready var save_unit_btn: Button = %SaveUnitBtn
@onready var save_weapon_btn: Button = %SaveWeaponBtn
@onready var reset_btn: Button = %ResetBtn

@onready var grid_check: CheckButton = %GridCheck
@onready var radius_check: CheckButton = %RadiusCheck
@onready var turrets_check: CheckButton = %TurretsCheck
@onready var legend_check: CheckButton = %LegendCheck
@onready var legend_overlay: Control = %LegendOverlay

@onready var zoom_out_btn: Button = %ZoomOutBtn
@onready var zoom_in_btn: Button = %ZoomInBtn
@onready var zoom_label: Label = %ZoomLabel
@onready var zoom_reset_btn: Button = %ZoomResetBtn
@onready var zoom_fit_btn: Button = %ZoomFitBtn

# 右侧属性侧边栏引用
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

@onready var muzzle_option: OptionButton = %MuzzleOption
@onready var add_muzzle_btn: Button = %AddMuzzleBtn
@onready var del_muzzle_btn: Button = %DelMuzzleBtn
@onready var mirror_muzzle_btn: Button = %MirrorMuzzleBtn
@onready var firing_mode_option: OptionButton = %FiringModeOption

@onready var spin_muzzle_x: SpinBox = %SpinMuzzleX
@onready var spin_muzzle_y: SpinBox = %SpinMuzzleY
@onready var spin_muzzle_z: SpinBox = %SpinMuzzleZ
@onready var spin_anim_fps: SpinBox = %SpinAnimFps

@onready var weapon_info_label: Label = %WeaponInfoLabel
@onready var angle_slider: HSlider = %AngleSlider
@onready var angle_value_label: Label = %AngleValueLabel
@onready var btn_angle_0: Button = %BtnAngle0
@onready var btn_angle_90: Button = %BtnAngle90
@onready var btn_angle_180: Button = %BtnAngle180
@onready var btn_angle_270: Button = %BtnAngle270

@onready var auto_rot_check: CheckButton = %AutoRotCheck
@onready var mouse_aim_check: CheckButton = %MouseAimCheck
@onready var fire_anim_check: CheckButton = %FireAnimCheck
@onready var time_scale_option: OptionButton = %TimeScaleOption
@onready var anim_play_btn: Button = %AnimPlayBtn
@onready var anim_step_btn: Button = %AnimStepBtn

# 炮口闪光预览面板引用
@onready var flash_weapon_enable_check: CheckButton = %FlashWeaponEnableCheck
@onready var flash_enable_check: CheckButton = %FlashEnableCheck
@onready var flash_all_mounts_check: CheckButton = %FlashAllMountsCheck
@onready var spin_flash_angle: SpinBox = %SpinFlashAngle
@onready var spin_flash_frame: SpinBox = %SpinFlashFrame
@onready var preset_cannon_btn: Button = %PresetCannonBtn
@onready var preset_autogun_btn: Button = %PresetAutoGunBtn
@onready var preset_heavy_btn: Button = %PresetHeavyBtn
@onready var preset_energy_btn: Button = %PresetEnergyBtn
@onready var preset_missile_btn: Button = %PresetMissileBtn

@onready var flash_color_picker: ColorPickerButton = %FlashColorPicker
@onready var flash_scale_slider: HSlider = %FlashScaleSlider
@onready var flash_life_slider: HSlider = %FlashLifeSlider
@onready var flash_speed_slider: HSlider = %FlashSpeedSlider
@onready var flash_core_slider: HSlider = %FlashCoreSlider
@onready var flash_cone_len_slider: HSlider = %FlashConeLenSlider
@onready var flash_cone_width_slider: HSlider = %FlashConeWidthSlider
@onready var flash_side_str_slider: HSlider = %FlashSideStrSlider
@onready var flash_side_angle_slider: HSlider = %FlashSideAngleSlider
@onready var flash_side_len_slider: HSlider = %FlashSideLenSlider
@onready var flash_sparks_slider: HSlider = %FlashSparksSlider
@onready var flash_pixel_slider: HSlider = %FlashPixelSlider
@onready var flash_save_btn: Button = %FlashSaveBtn
@onready var flash_copy_btn: Button = %FlashCopyBtn

# 标靶与投射物面板引用
@onready var target_dummy_enable_check: CheckButton = %TargetDummyEnableCheck
@onready var target_aim_check: CheckButton = %TargetAimCheck
@onready var spin_target_x: SpinBox = %SpinTargetX
@onready var spin_target_y: SpinBox = %SpinTargetY
@onready var spin_target_z: SpinBox = %SpinTargetZ
@onready var target_move_check: CheckButton = %TargetMoveCheck
@onready var target_reset_btn: Button = %TargetResetBtn
@onready var target_clear_hits_btn: Button = %TargetClearHitsBtn
@onready var target_hit_label: Label = %TargetHitLabel

@onready var proj_enable_check: CheckButton = %ProjEnableCheck
@onready var fire_proj_btn: Button = %FireProjBtn
@onready var auto_fire_check: CheckButton = %AutoFireCheck
@onready var spin_fire_interval: SpinBox = %SpinFireInterval
@onready var fire_sync_anim_check: CheckButton = %FireSyncAnimCheck
@onready var fire_all_mounts_check: CheckButton = %FireAllMountsCheck

@onready var proj_type_option: OptionButton = %ProjTypeOption
@onready var proj_texture_option: OptionButton = %ProjTextureOption
@onready var spin_proj_speed: SpinBox = %SpinProjSpeed
@onready var spin_proj_arc_height: SpinBox = %SpinProjArcHeight
@onready var spin_proj_accel: SpinBox = %SpinProjAccel
@onready var spin_proj_turn_speed: SpinBox = %SpinProjTurnSpeed
@onready var spin_proj_splash: SpinBox = %SpinProjSplash
@onready var spin_proj_scale: SpinBox = %SpinProjScale
@onready var proj_healing_check: CheckButton = %ProjHealingCheck
@onready var proj_trail_check: CheckButton = %ProjTrailCheck
@onready var proj_shadow_check: CheckButton = %ProjShadowCheck
@onready var proj_sync_weapon_btn: Button = %ProjSyncWeaponBtn
@onready var proj_save_btn: Button = %ProjSaveBtn

# 对话框与提示
@onready var toast_label: Label = %ToastLabel
@onready var toast_timer: Timer = $ToastTimer
@onready var add_weapon_dialog: ConfirmationDialog = %AddWeaponDialog
@onready var weapon_pick_list: ItemList = %WeaponPickList


func _ready() -> void:
	_scan_all_weapons()
	_scan_all_targets()
	_scan_all_projectiles()
	_setup_ui_signals()
	_update_flash_ui_from_params()
	_update_proj_ui_from_params()
	
	if ResourceLoader.exists("res://asset/unit/target_dummy.png"):
		target_tex = load("res://asset/unit/target_dummy.png")
	
	if unit_selector.item_count > 0:
		unit_selector.select(0)
		_on_target_selected(0)
	
	_set_mode(EditMode.MOUNT)
	call_deferred("_fit_view")


func _process(delta: float) -> void:
	var scaled_delta: float = delta * preview_time_scale

	# 1. 标靶自动巡逻游走
	if is_target_patrolling:
		target_patrol_time += scaled_delta
		target_pos = Vector2(cos(target_patrol_time * 1.2) * 150.0 + 30.0, sin(target_patrol_time * 0.9) * 110.0)
		spin_target_x.set_value_no_signal(target_pos.x)
		spin_target_y.set_value_no_signal(target_pos.y)

	# 2. 标靶受击高亮闪烁计时
	if target_hit_flash_timer > 0.0:
		target_hit_flash_timer = maxf(0.0, target_hit_flash_timer - delta)

	# 3. 炮塔旋转与瞄准逻辑 (自动匀速 / 自动对准标靶 / 鼠标跟随)
	if not ((is_dragging_handle and active_drag_type == "pivot") or pivot_edit_active):
		if is_aiming_at_target and current_mount_idx >= 0 and current_mount_idx < mounts.size():
			var mount_pos: Vector2 = mounts[current_mount_idx].offset
			var dir: Vector2 = target_pos - mount_pos
			if dir.length_squared() > 1.0:
				var target_angle_deg: float = rad_to_deg(dir.angle() - PI / 2.0)
				preview_angle_deg = fmod(target_angle_deg + 360.0, 360.0)
				angle_slider.set_value_no_signal(preview_angle_deg)
				angle_value_label.text = "%.1f°" % preview_angle_deg
				_update_turret_transforms()
		elif is_auto_rotating:
			preview_angle_deg = fmod(preview_angle_deg + delta * 90.0, 360.0)
			angle_slider.set_value_no_signal(preview_angle_deg)
			angle_value_label.text = "%.1f°" % preview_angle_deg
			_update_turret_transforms()
		elif is_mouse_aiming and current_mount_idx >= 0 and current_mount_idx < mounts.size():
			var mount_pos: Vector2 = mounts[current_mount_idx].offset
			var mouse_local: Vector2 = stage.to_local(stage_container.get_local_mouse_position() + stage_container.global_position)
			var dir: Vector2 = mouse_local - mount_pos
			if dir.length_squared() > 1.0:
				var target_angle_deg: float = rad_to_deg(dir.angle() - PI / 2.0)
				preview_angle_deg = fmod(target_angle_deg + 360.0, 360.0)
				angle_slider.set_value_no_signal(preview_angle_deg)
				angle_value_label.text = "%.1f°" % preview_angle_deg
				_update_turret_transforms()

	# 4. 武器图集动画与开火帧对齐触发发射
	var target_has_body_anim: bool = false
	if body_sprite and body_sprite.texture:
		var h_f: int = maxi(1, int(current_target_data.get("h_frames", "1")))
		var v_f: int = maxi(1, int(current_target_data.get("v_frames", "1")))
		target_has_body_anim = (h_f * v_f > 1)

	if (is_attacking_preview or flash_preview_enabled or target_has_body_anim) and not is_anim_paused:
		weapon_anim_time += scaled_delta
		flash_time += scaled_delta
		_update_turret_animation_frames()

		if proj_enable_firing and is_attacking_preview and proj_sync_anim and current_mount_idx >= 0 and current_mount_idx < mounts.size():
			var w_name: String = mounts[current_mount_idx].name
			var vis: Dictionary = _load_weapon_cache(w_name)
			var cycle_dur: float = _get_attack_cycle_dur(vis)
			var cycle_idx: int = int(weapon_anim_time / maxf(0.01, cycle_dur))
			if cycle_idx != last_anim_cycle_count:
				last_anim_cycle_count = cycle_idx
				has_fired_this_anim_cycle = false

			var t_in_cycle: float = fmod(weapon_anim_time, cycle_dur)
			var trig_frame: int = vis.get("flash_trigger_frame", 0)
			var fps: int = vis.get("anim_fps", 10)
			if fps <= 0: fps = 10
			var trig_time: float = float(trig_frame) / float(fps)
			if t_in_cycle >= trig_time and not has_fired_this_anim_cycle:
				has_fired_this_anim_cycle = true
				_fire_projectiles()

	# 5. 自动独立连发计时器
	if proj_enable_firing and proj_auto_fire and not is_anim_paused:
		proj_auto_fire_timer += scaled_delta
		if proj_auto_fire_timer >= proj_fire_interval:
			proj_auto_fire_timer = 0.0
			_fire_projectiles()

	# 6. 更新所有飞行中的投射物物理、爆炸特效与飘字
	_update_projectiles(scaled_delta)
	_update_impact_effects(scaled_delta)
	_update_floating_texts(scaled_delta)

	handles_draw.queue_redraw()


# ==============================================================================
# 数据加载与扫描
# ==============================================================================

func _scan_all_weapons() -> void:
	weapon_cfgs.clear()
	weapon_cache.clear()
	for path: String in _get_dir_files(WEAPON_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg_file(path)
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
	var atk_r: int = int(d.get("attacking_row", "1" if v_f > 1 else "0"))
	var idle_f: int = int(d.get("idle_frames", "1"))
	var atk_f: int = int(d.get("attacking_frames", "1"))
	var a_fps: int = int(d.get("anim_fps", "10"))
	var atk_interval: float = float(d.get("attack_interval", "1.0"))
	
	var pivot_str: String = str(d.get("rotation_center", "0,0"))
	var pivot_parts: PackedStringArray = pivot_str.split(",", false)
	var pivot: Vector2 = Vector2(
		float(pivot_parts[0]) if pivot_parts.size() >= 1 else 0.0,
		float(pivot_parts[1]) if pivot_parts.size() >= 2 else 0.0
	)
	
	var muzzles: Array = []
	if d.has("muzzle_offsets"):
		var m_raw: String = str(d.get("muzzle_offsets", ""))
		for m_item in m_raw.split("|", false):
			var m_p: PackedStringArray = m_item.strip_edges().split(",", false)
			if m_p.size() >= 2:
				muzzles.append(Vector3(
					float(m_p[0]),
					float(m_p[1]),
					float(m_p[2]) if m_p.size() >= 3 else 0.0
				))
	
	if muzzles.is_empty():
		var muz_str: String = str(d.get("muzzle_offset", "0,0,0"))
		var muz_parts: PackedStringArray = muz_str.split(",", false)
		var muzzle: Vector3 = Vector3(
			float(muz_parts[0]) if muz_parts.size() >= 1 else 0.0,
			float(muz_parts[1]) if muz_parts.size() >= 2 else 0.0,
			float(muz_parts[2]) if muz_parts.size() >= 3 else 0.0
		)
		muzzles.append(muzzle)
	
	var firing_mode_str: String = str(d.get("firing_mode", "Simultaneous")).to_lower()
	var firing_mode: int = 1 if firing_mode_str.contains("alter") else 0
	
	var flash_en: bool = str(d.get("muzzle_flash", d.get("flash_enabled", "true"))).to_lower() != "false"
	var flash_pre: String = str(d.get("flash_preset", "cannon"))
	var flash_sc: float = float(d.get("flash_scale", "0.0"))
	var flash_lf: float = float(d.get("flash_life", "0.0"))
	var flash_ang: float = float(d.get("muzzle_flash_angle", "0.0"))
	var flash_trig: int = int(d.get("flash_trigger_frame", d.get("flash_frame", "0")))

	var entry: Dictionary = {
		"name": w_name,
		"tex": tex,
		"tex_path": tex_path,
		"h_frames": maxi(1, h_f),
		"v_frames": maxi(1, v_f),
		"idle_row": idle_r,
		"attacking_row": atk_r,
		"idle_frames": maxi(1, idle_f),
		"attacking_frames": maxi(1, atk_f),
		"anim_fps": maxi(1, a_fps),
		"attack_interval": atk_interval,
		"pivot": pivot,
		"muzzle": muzzles[0],
		"muzzles": muzzles,
		"current_muzzle_idx": 0,
		"firing_mode": firing_mode,
		"muzzle_flash": flash_en,
		"flash_preset": flash_pre,
		"flash_scale": flash_sc,
		"flash_life": flash_lf,
		"flash_angle": flash_ang,
		"flash_trigger_frame": flash_trig
	}
	weapon_cache[w_name] = entry
	return entry


func _scan_all_targets() -> void:
	unit_entries.clear()
	unit_selector.clear()
	
	# 1. 扫描兵种单位
	for path: String in _get_dir_files(UNIT_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg_file(path)
		var u_name: String = str(d.get("unit_name", path.get_file().get_basename()))
		unit_entries.append({
			"name": u_name,
			"path": path,
			"is_building": false,
			"display_name": "[单位] " + u_name
		})
	
	# 2. 扫描建筑单位 (如 Fortress 等)
	for path: String in _get_dir_files(BUILDING_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg_file(path)
		var b_name: String = str(d.get("building_name", path.get_file().get_basename()))
		unit_entries.append({
			"name": b_name,
			"path": path,
			"is_building": true,
			"display_name": "[建筑] " + b_name
		})
	
	for entry: Dictionary in unit_entries:
		unit_selector.add_item(entry.display_name)


func _load_target(path: String, is_building: bool) -> void:
	pivot_edit_active = false
	current_target_path = path
	is_current_building = is_building
	current_target_data = _parse_cfg_file(path)
	
	# 1. 加载身体/底盘贴图
	var tex_path: String = str(current_target_data.get("texture_path", ""))
	var tex: Texture2D = null
	if ResourceLoader.exists(tex_path):
		tex = load(tex_path)
	
	body_sprite.texture = tex
	body_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	if tex:
		var h_f: int = maxi(1, int(current_target_data.get("h_frames", "1")))
		var v_f: int = maxi(1, int(current_target_data.get("v_frames", "1")))
		var total_f: int = h_f * v_f
		if total_f > 1:
			body_frame_hbox.visible = true
			spin_body_frame.max_value = total_f - 1
			# 针对武装直升机等飞行器默认切到第2帧 (Frame 1) 避开旋转螺旋桨对挂载点的遮挡
			if path.get_file().contains("helicopter"):
				current_body_frame = 1
			else:
				current_body_frame = 0
			spin_body_frame.set_value_no_signal(current_body_frame)
		else:
			body_frame_hbox.visible = false
			current_body_frame = 0
			spin_body_frame.set_value_no_signal(0)
		_update_body_sprite_frame()
		body_sprite.centered = true
		body_sprite.position = Vector2.ZERO
	else:
		body_frame_hbox.visible = false
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


func _update_body_sprite_frame() -> void:
	if not body_sprite or not body_sprite.texture:
		return
	var h_f: int = maxi(1, int(current_target_data.get("h_frames", "1")))
	var v_f: int = maxi(1, int(current_target_data.get("v_frames", "1")))
	var total_f: int = h_f * v_f
	var f: int = clampi(current_body_frame, 0, total_f - 1)
	var col: int = f % h_f
	var row: int = f / h_f
	var fw: float = body_sprite.texture.get_width() / float(h_f)
	var fh: float = body_sprite.texture.get_height() / float(v_f)
	body_sprite.region_enabled = true
	body_sprite.region_rect = Rect2(col * fw, row * fh, fw, fh)


func _on_spin_body_frame_changed(val: float) -> void:
	current_body_frame = int(val)
	_update_body_sprite_frame()
	handles_draw.queue_redraw()


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
		var comment: String = ""
		var semi: int = val.find(";")
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
	for child: Node in turret_container.get_children():
		child.queue_free()
	
	for i in mounts.size():
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var spr: Sprite2D = Sprite2D.new()
		spr.name = "TurretSprite_%d" % i
		spr.texture = vis.get("tex", null)
		spr.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
		if spr.texture:
			spr.region_enabled = true
			var h_f: int = vis.get("h_frames", 1)
			var v_f: int = vis.get("v_frames", 1)
			var fw: float = spr.texture.get_width() / float(h_f)
			var fh: float = spr.texture.get_height() / float(v_f)
			var row: int = vis.get("attacking_row", 0) if is_attacking_preview else vis.get("idle_row", 0)
			spr.region_rect = Rect2(0, row * fh, fw, fh)
		spr.centered = true
		turret_container.add_child(spr)
	
	_update_turret_animation_frames()
	_update_turret_transforms()


## 计算攻击动画完整周期时长 (保证后座力动作与闪光粒子完整播放完毕 + 冷却缓冲)
func _get_attack_cycle_dur(vis: Dictionary) -> float:
	var fps: int = vis.get("anim_fps", 10)
	if fps <= 0: fps = 10
	var fr: int = vis.get("attacking_frames", 1)
	var anim_dur: float = float(fr) / float(fps)
	var trig_frame: int = vis.get("flash_trigger_frame", 0)
	var trig_time: float = float(trig_frame) / float(fps)
	var flash_end: float = trig_time + flash_params.life
	return maxf(anim_dur + 0.35, flash_end + 0.20)


func _update_turret_animation_frames() -> void:
	# 1. 刷新底盘/建筑主体自身的动作动画（如指挥中心部署/开门、直升机螺旋桨旋转、造船厂下水等）
	if body_sprite and body_sprite.texture:
		var h_f: int = maxi(1, int(current_target_data.get("h_frames", "1")))
		var v_f: int = maxi(1, int(current_target_data.get("v_frames", "1")))
		var total_f: int = h_f * v_f
		var fps: int = maxi(1, int(current_target_data.get("anim_fps", "8")))
		
		if total_f > 1:
			var target_frame: int = 0
			if is_attacking_preview:
				# 建筑生产完成部署动画 (finish_frames) 或 单位攻击动作
				var c_frames: int = int(current_target_data.get("finish_frames", current_target_data.get("working_frames", total_f)))
				if c_frames <= 0: c_frames = total_f
				var anim_dur: float = float(c_frames) / float(fps)
				var hold_dur: float = float(current_target_data.get("working_hold_time", "0.5"))
				var cycle_dur: float = anim_dur + hold_dur + 0.6
				var t_cycle: float = fmod(weapon_anim_time, cycle_dur)
				
				if t_cycle < anim_dur:
					target_frame = int((t_cycle / anim_dur) * float(c_frames))
					target_frame = clampi(target_frame, 0, c_frames - 1)
				elif t_cycle < anim_dur + hold_dur:
					target_frame = c_frames - 1 # 停留在展开最后一帧
				else:
					target_frame = 0 # 收起回待机第1帧
			else:
				# 待机动画
				var idle_f: int = int(current_target_data.get("idle_frames", "1"))
				if idle_f > 1:
					var idle_dur: float = float(idle_f) / float(fps)
					target_frame = int(fmod(weapon_anim_time, idle_dur) * float(fps))
					target_frame = clampi(target_frame, 0, idle_f - 1)
				else:
					target_frame = current_body_frame
			
			if current_body_frame != target_frame:
				current_body_frame = target_frame
				spin_body_frame.set_value_no_signal(target_frame)
				_update_body_sprite_frame()

	# 2. 刷新炮塔/武器精灵的动作动画
	var children: Array[Node] = turret_container.get_children()
	for i in children.size():
		if i >= mounts.size():
			continue
		var spr: Sprite2D = children[i] as Sprite2D
		if not spr or not spr.texture:
			continue
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var h_f: int = vis.get("h_frames", 1)
		var v_f: int = vis.get("v_frames", 1)
		var fw: float = spr.texture.get_width() / float(h_f)
		var fh: float = spr.texture.get_height() / float(v_f)
		
		var fps: int = vis.get("anim_fps", 10)
		if fps <= 0: fps = 10
		var row: int = 0
		var fr: int = 1
		var f_idx: int = 0
		
		if is_attacking_preview:
			row = vis.get("attacking_row", 0)
			fr = vis.get("attacking_frames", 1)
			var anim_dur: float = float(fr) / float(fps)
			var cycle_dur: float = _get_attack_cycle_dur(vis)
			var t_cycle: float = fmod(weapon_anim_time, cycle_dur)
			if t_cycle < anim_dur:
				f_idx = int(t_cycle * float(fps))
				f_idx = clampi(f_idx, 0, fr - 1)
			else:
				# 后座力复位至初始静止帧 (通常为第0帧)
				f_idx = 0
		else:
			row = vis.get("idle_row", 0)
			fr = vis.get("idle_frames", 1)
			if fr > 1:
				var idle_dur: float = float(fr) / float(fps)
				f_idx = int(fmod(weapon_anim_time, idle_dur) * float(fps))
				f_idx = clampi(f_idx, 0, fr - 1)
			else:
				f_idx = 0
		
		spr.region_rect = Rect2(f_idx * fw, row * fh, fw, fh)


func _update_turret_transforms() -> void:
	var children: Array[Node] = turret_container.get_children()
	var rad: float = deg_to_rad(preview_angle_deg)
	
	for i in children.size():
		if i >= mounts.size():
			continue
		var spr: Sprite2D = children[i] as Sprite2D
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
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
			spr.modulate = Color(1.0, 1.0, 1.0, 0.65)


# ==============================================================================
# UI 逻辑与交互
# ==============================================================================

func _setup_ui_signals() -> void:
	unit_selector.item_selected.connect(_on_target_selected)
	prev_unit_btn.pressed.connect(func():
		var idx: int = (unit_selector.selected - 1 + unit_selector.item_count) % maxi(1, unit_selector.item_count)
		unit_selector.select(idx)
		_on_target_selected(idx)
	)
	next_unit_btn.pressed.connect(func():
		var idx: int = (unit_selector.selected + 1) % maxi(1, unit_selector.item_count)
		unit_selector.select(idx)
		_on_target_selected(idx)
	)
	
	spin_body_frame.value_changed.connect(_on_spin_body_frame_changed)
	
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
	
	firing_mode_option.clear()
	firing_mode_option.add_item("💥 齐射 (Simultaneous)", 0)
	firing_mode_option.add_item("🔄 轮射 (Alternating)", 1)
	firing_mode_option.item_selected.connect(_on_firing_mode_option_selected)
	
	muzzle_option.item_selected.connect(_on_muzzle_option_selected)
	add_muzzle_btn.pressed.connect(_on_add_muzzle_pressed)
	del_muzzle_btn.pressed.connect(_on_del_muzzle_pressed)
	mirror_muzzle_btn.pressed.connect(_on_mirror_muzzle_pressed)
	
	spin_muzzle_x.value_changed.connect(_on_spin_muzzle_x_changed)
	spin_muzzle_y.value_changed.connect(_on_spin_muzzle_y_changed)
	spin_muzzle_z.value_changed.connect(_on_spin_muzzle_z_changed)
	spin_anim_fps.value_changed.connect(_on_spin_anim_fps_changed)
	
	angle_slider.value_changed.connect(_on_angle_slider_changed)
	auto_rot_check.toggled.connect(func(v: bool): is_auto_rotating = v)
	mouse_aim_check.toggled.connect(func(v: bool): is_mouse_aiming = v)
	fire_anim_check.toggled.connect(_on_fire_anim_toggled)
	time_scale_option.item_selected.connect(_on_time_scale_selected)
	anim_play_btn.pressed.connect(_on_anim_play_pressed)
	anim_step_btn.pressed.connect(_on_anim_step_pressed)
	
	btn_angle_0.pressed.connect(func(): _set_angle(0.0))
	btn_angle_90.pressed.connect(func(): _set_angle(90.0))
	btn_angle_180.pressed.connect(func(): _set_angle(180.0))
	btn_angle_270.pressed.connect(func(): _set_angle(270.0))
	
	save_unit_btn.pressed.connect(_save_current_unit)
	save_weapon_btn.pressed.connect(_save_current_weapon)
	save_all_btn.pressed.connect(_save_all)
	reset_btn.pressed.connect(_reset_current_unit)
	
	zoom_in_btn.pressed.connect(func(): _set_zoom(canvas_zoom * 1.3))
	zoom_out_btn.pressed.connect(func(): _set_zoom(canvas_zoom / 1.3))
	zoom_reset_btn.pressed.connect(func(): _set_zoom(4.0))
	zoom_fit_btn.pressed.connect(_fit_view)
	
	grid_check.toggled.connect(func(v: bool): show_grid = v; grid_draw.queue_redraw())
	radius_check.toggled.connect(func(v: bool): show_collision_radius = v; grid_draw.queue_redraw())
	turrets_check.toggled.connect(func(v: bool): show_turrets = v; _update_turret_transforms())
	legend_check.toggled.connect(func(v: bool): legend_overlay.visible = v)
	
	add_weapon_dialog.confirmed.connect(_on_add_weapon_dialog_confirmed)

	# --- 炮口闪光预览面板 ---
	flash_weapon_enable_check.toggled.connect(func(v: bool):
		if current_mount_idx >= 0 and current_mount_idx < mounts.size():
			var w_name: String = mounts[current_mount_idx].name
			if weapon_cache.has(w_name):
				weapon_cache[w_name]["muzzle_flash"] = v
				handles_draw.queue_redraw()
	)
	flash_enable_check.toggled.connect(func(v: bool):
		flash_preview_enabled = v
		flash_time = 0.0
		handles_draw.queue_redraw()
	)
	flash_all_mounts_check.toggled.connect(func(v: bool):
		flash_all_mounts = v
		handles_draw.queue_redraw()
	)
	
	preset_cannon_btn.pressed.connect(func(): _apply_flash_preset("cannon"))
	preset_autogun_btn.pressed.connect(func(): _apply_flash_preset("autogun"))
	preset_heavy_btn.pressed.connect(func(): _apply_flash_preset("heavy"))
	preset_energy_btn.pressed.connect(func(): _apply_flash_preset("energy"))
	preset_missile_btn.pressed.connect(func(): _apply_flash_preset("missile"))

	flash_color_picker.color_changed.connect(func(c: Color): flash_params.color = c)
	spin_flash_angle.value_changed.connect(_on_spin_flash_angle_changed)
	spin_flash_frame.value_changed.connect(_on_spin_flash_frame_changed)
	flash_scale_slider.value_changed.connect(func(v: float): flash_params.scale = v)
	flash_life_slider.value_changed.connect(func(v: float): flash_params.life = v)
	flash_speed_slider.value_changed.connect(func(v: float): flash_params.forward_speed = v)
	flash_core_slider.value_changed.connect(func(v: float): flash_params.core_radius = v)
	flash_cone_len_slider.value_changed.connect(func(v: float): flash_params.cone_length = v)
	flash_cone_width_slider.value_changed.connect(func(v: float): flash_params.cone_width = v)
	flash_side_str_slider.value_changed.connect(func(v: float): flash_params.side_strength = v)
	flash_side_angle_slider.value_changed.connect(func(v: float): flash_params.side_angle_deg = v)
	flash_side_len_slider.value_changed.connect(func(v: float): flash_params.side_length = v)
	flash_sparks_slider.value_changed.connect(func(v: float): flash_params.spark_intensity = v)
	flash_pixel_slider.value_changed.connect(func(v: float): flash_params.pixel_size = v)
	flash_save_btn.pressed.connect(_save_current_weapon)
	flash_copy_btn.pressed.connect(_on_flash_copy_pressed)

	# --- 标靶与投射物面板信号绑定 ---
	mode_target_btn.pressed.connect(func(): _set_mode(EditMode.TARGET))

	target_dummy_enable_check.toggled.connect(func(v: bool):
		target_dummy_enabled = v
		handles_draw.queue_redraw()
	)
	target_aim_check.toggled.connect(func(v: bool):
		is_aiming_at_target = v
		if v:
			mouse_aim_check.button_pressed = false
			auto_rot_check.button_pressed = false
	)
	spin_target_x.value_changed.connect(func(v: float):
		target_pos.x = v
		handles_draw.queue_redraw()
	)
	spin_target_y.value_changed.connect(func(v: float):
		target_pos.y = v
		handles_draw.queue_redraw()
	)
	spin_target_z.value_changed.connect(func(v: float):
		target_height = v
		handles_draw.queue_redraw()
	)
	target_move_check.toggled.connect(func(v: bool):
		is_target_patrolling = v
	)
	target_reset_btn.pressed.connect(func():
		target_pos = Vector2(140.0, 0.0)
		target_height = 0.0
		spin_target_x.set_value_no_signal(target_pos.x)
		spin_target_y.set_value_no_signal(target_pos.y)
		spin_target_z.set_value_no_signal(target_height)
		handles_draw.queue_redraw()
		_show_toast("🎯 标靶位置已复位至 (140, 0)")
	)
	target_clear_hits_btn.pressed.connect(func():
		target_hit_count = 0
		target_hit_label.text = "🎯 标靶命中: 0 次"
		_show_toast("🎯 命中计数已清零")
	)

	proj_enable_check.toggled.connect(func(v: bool): proj_enable_firing = v)
	fire_proj_btn.pressed.connect(_fire_projectiles)
	auto_fire_check.toggled.connect(func(v: bool):
		proj_auto_fire = v
		proj_auto_fire_timer = 0.0
	)
	spin_fire_interval.value_changed.connect(func(v: float): proj_fire_interval = v)
	fire_sync_anim_check.toggled.connect(func(v: bool): proj_sync_anim = v)
	fire_all_mounts_check.toggled.connect(func(v: bool): proj_all_mounts = v)

	proj_type_option.item_selected.connect(func(idx: int):
		proj_params.type = idx
		if idx == 0:
			proj_params.preset_name = "MarineBullet"
			proj_params.tex_name = "bullet.png"
		elif idx == 1:
			proj_params.preset_name = "Shell"
			proj_params.tex_name = "shell.png"
		elif idx == 2:
			proj_params.preset_name = "Missile"
			proj_params.tex_name = "missle.png"
		_update_proj_ui_from_params()
		handles_draw.queue_redraw()
	)
	proj_texture_option.item_selected.connect(func(idx: int):
		proj_params.tex_name = proj_texture_option.get_item_text(idx)
		handles_draw.queue_redraw()
	)
	spin_proj_speed.value_changed.connect(func(v: float): proj_params.speed = v)
	spin_proj_arc_height.value_changed.connect(func(v: float): proj_params.arc_height = v)
	spin_proj_accel.value_changed.connect(func(v: float): proj_params.acceleration = v)
	spin_proj_turn_speed.value_changed.connect(func(v: float): proj_params.turn_speed = v)
	spin_proj_splash.value_changed.connect(func(v: float): proj_params.splash_radius = v)
	spin_proj_scale.value_changed.connect(func(v: float):
		proj_params.scale = v
		handles_draw.queue_redraw()
	)
	proj_healing_check.toggled.connect(func(v: bool):
		proj_params.is_healing = v
		if v:
			proj_params.tex_name = "repair_beam.png"
			_update_proj_ui_from_params()
		handles_draw.queue_redraw()
	)
	proj_trail_check.toggled.connect(func(v: bool): proj_params.trail_enabled = v)
	proj_shadow_check.toggled.connect(func(v: bool): proj_params.shadow_enabled = v)
	proj_sync_weapon_btn.pressed.connect(_sync_proj_from_current_weapon)
	proj_save_btn.pressed.connect(_save_projectile_to_weapon)


func _apply_flash_preset(preset_key: String) -> void:
	if not FLASH_PRESETS.has(preset_key):
		return
	var p: Dictionary = FLASH_PRESETS[preset_key]
	for k in p.keys():
		flash_params[k] = p[k]
	_update_flash_ui_from_params()
	flash_time = 0.0
	
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name]["flash_preset"] = preset_key
			weapon_cache[w_name]["flash_scale"] = flash_params.scale
			weapon_cache[w_name]["flash_life"] = flash_params.life

	handles_draw.queue_redraw()
	_show_toast("✨ 已应用像素预设: %s (按 Ctrl+S 保存到武器配置)" % preset_key)


## 调节炮口焰喷射方向偏移 (度, 相对炮管朝向), 存入武器缓存, 保存时写回 cfg
func _on_spin_flash_angle_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name]["flash_angle"] = val
			handles_draw.queue_redraw()


## 调节开火对齐帧 (在攻击动画第几帧喷射闪光), 存入武器缓存, 保存时写回 cfg
func _on_spin_flash_frame_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name]["flash_trigger_frame"] = int(val)
			handles_draw.queue_redraw()


## 调节武器基础动画帧率 (FPS), 存入武器缓存, 保存时写回 cfg
func _on_spin_anim_fps_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var new_fps: int = maxi(1, int(val))
			weapon_cache[w_name]["anim_fps"] = new_fps
			var vis: Dictionary = weapon_cache[w_name]
			var atk_f: int = vis.get("attacking_frames", 1)
			weapon_info_label.text = "武器: %s (图集 %dx%d | 开火 %d帧 @ %dfps)" % [w_name, vis.get("h_frames", 1), vis.get("v_frames", 1), atk_f, new_fps]
			handles_draw.queue_redraw()


## 切换预览播放倍速
func _on_time_scale_selected(idx: int) -> void:
	match idx:
		0: preview_time_scale = 0.1
		1: preview_time_scale = 0.25
		2: preview_time_scale = 0.5
		3: preview_time_scale = 1.0
		4: preview_time_scale = 2.0
		_: preview_time_scale = 1.0
	_show_toast("⏩ 预览播放倍速: %.2fx" % preview_time_scale)


## 暂停 / 继续播放开火动画
func _on_anim_play_pressed() -> void:
	is_anim_paused = not is_anim_paused
	if is_anim_paused:
		anim_play_btn.text = "▶ 继续"
		_show_toast("⏸ 动画已定格暂停")
	else:
		anim_play_btn.text = "⏸ 暂停"
		_show_toast("▶ 动画继续播放")


## 单帧步进前进 (Step Frame)
func _on_anim_step_pressed() -> void:
	if not is_anim_paused:
		is_anim_paused = true
		anim_play_btn.text = "▶ 继续"
	
	var cur_fps: float = 10.0
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			cur_fps = float(weapon_cache[w_name].get("anim_fps", 10))
	if cur_fps <= 0.0:
		cur_fps = 10.0
	
	var frame_step: float = 1.0 / cur_fps
	weapon_anim_time += frame_step
	flash_time += frame_step
	_update_turret_animation_frames()
	handles_draw.queue_redraw()
	_show_toast("⏭ 单步前进 1 帧 (+%.2fs)" % frame_step)


func _update_flash_ui_from_params() -> void:
	flash_color_picker.color = flash_params.color
	flash_scale_slider.set_value_no_signal(flash_params.scale)
	flash_life_slider.set_value_no_signal(flash_params.life)
	flash_speed_slider.set_value_no_signal(flash_params.forward_speed)
	flash_core_slider.set_value_no_signal(flash_params.core_radius)
	flash_cone_len_slider.set_value_no_signal(flash_params.cone_length)
	flash_side_str_slider.set_value_no_signal(flash_params.side_strength)
	flash_side_angle_slider.set_value_no_signal(flash_params.side_angle_deg)
	flash_side_len_slider.set_value_no_signal(flash_params.side_length)
	flash_sparks_slider.set_value_no_signal(flash_params.spark_intensity)
	flash_pixel_slider.set_value_no_signal(flash_params.get("pixel_size", 1.0))


func _set_mode(mode: EditMode) -> void:
	_end_pivot_edit()
	edit_mode = mode
	mode_mount_btn.button_pressed = (mode == EditMode.MOUNT)
	mode_pivot_btn.button_pressed = (mode == EditMode.PIVOT)
	mode_muzzle_btn.button_pressed = (mode == EditMode.MUZZLE)
	mode_target_btn.button_pressed = (mode == EditMode.TARGET)
	handles_draw.queue_redraw()
	if mode == EditMode.MUZZLE:
		_show_toast("⚡ 枪口模式: Shift+点击画布新建枪口 | M 键镜像 | Delete 键删除")
	elif mode == EditMode.MOUNT:
		_show_toast("📍 挂载点模式: 鼠标拖拽挂载底座物理位置")
	elif mode == EditMode.PIVOT:
		_show_toast("🎯 枢轴模式: 鼠标拖拽旋转中心/开火轴心")
	elif mode == EditMode.TARGET:
		_show_toast("🎯 标靶模式: 鼠标拖拽标靶位置测试弹道与制导追踪")


func _set_angle(deg: float) -> void:
	_end_pivot_edit()
	preview_angle_deg = deg
	angle_slider.set_value_no_signal(deg)
	angle_value_label.text = "%.1f°" % deg
	_update_turret_transforms()
	handles_draw.queue_redraw()


func _on_angle_slider_changed(val: float) -> void:
	_end_pivot_edit()
	preview_angle_deg = val
	angle_value_label.text = "%.1f°" % val
	_update_turret_transforms()
	handles_draw.queue_redraw()


func _on_fire_anim_toggled(pressed: bool) -> void:
	is_attacking_preview = pressed
	weapon_anim_time = 0.0
	flash_time = 0.0
	if not pressed:
		if current_target_path.get_file().contains("helicopter"):
			current_body_frame = 1
		else:
			current_body_frame = 0
		spin_body_frame.set_value_no_signal(current_body_frame)
		_update_body_sprite_frame()
	_update_turret_animation_frames()


func _on_target_selected(idx: int) -> void:
	if idx < 0 or idx >= unit_entries.size():
		return
	var entry: Dictionary = unit_entries[idx]
	_load_target(entry.path, entry.is_building)


func _update_mount_list_ui() -> void:
	mount_item_list.clear()
	for i in mounts.size():
		var m: Dictionary = mounts[i]
		var label: String = "[%d] %s (%.1f, %.1f)" % [i, m.name, m.offset.x, m.offset.y]
		mount_item_list.add_item(label)
	
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		mount_item_list.select(current_mount_idx)


func _select_mount(idx: int) -> void:
	_end_pivot_edit()
	current_mount_idx = idx
	_update_turret_transforms()
	
	if idx >= 0 and idx < mounts.size():
		var m: Dictionary = mounts[idx]
		spin_mount_x.set_value_no_signal(m.offset.x)
		spin_mount_y.set_value_no_signal(m.offset.y)
		
		var vis: Dictionary = _load_weapon_cache(m.name)
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		
		spin_pivot_x.set_value_no_signal(pivot.x)
		spin_pivot_y.set_value_no_signal(pivot.y)
		_update_muzzle_ui(vis)
		
		flash_weapon_enable_check.set_pressed_no_signal(vis.get("muzzle_flash", true))
		spin_flash_angle.set_value_no_signal(float(vis.get("flash_angle", 0.0)))
		spin_flash_frame.set_value_no_signal(float(vis.get("flash_trigger_frame", 0)))
		
		var atk_f: int = vis.get("attacking_frames", 1)
		var a_fps: int = vis.get("anim_fps", 10)
		spin_anim_fps.set_value_no_signal(float(a_fps))
		weapon_info_label.text = "武器: %s (图集 %dx%d | 开火 %d帧 @ %dfps)" % [m.name, vis.get("h_frames", 1), vis.get("v_frames", 1), atk_f, a_fps]

		# 同步当前武器的专属开火闪光配置
		var flash_pre: String = str(vis.get("flash_preset", "cannon"))
		if FLASH_PRESETS.has(flash_pre):
			var p: Dictionary = FLASH_PRESETS[flash_pre]
			for k in p.keys():
				flash_params[k] = p[k]
		var cfg_scale: float = float(vis.get("flash_scale", 0.0))
		if cfg_scale > 0.0:
			flash_params.scale = cfg_scale
		var cfg_life: float = float(vis.get("flash_life", 0.0))
		if cfg_life > 0.0:
			flash_params.life = cfg_life
		_update_flash_ui_from_params()
		_sync_proj_from_current_weapon()
	else:
		spin_mount_x.set_value_no_signal(0.0)
		spin_mount_y.set_value_no_signal(0.0)
		spin_pivot_x.set_value_no_signal(0.0)
		spin_pivot_y.set_value_no_signal(0.0)
		muzzle_option.clear()
		spin_muzzle_x.set_value_no_signal(0.0)
		spin_muzzle_y.set_value_no_signal(0.0)
		spin_muzzle_z.set_value_no_signal(0.0)
		flash_weapon_enable_check.set_pressed_no_signal(false)
		spin_flash_angle.set_value_no_signal(0.0)
		weapon_info_label.text = "未选中武器"
	
	handles_draw.queue_redraw()


func _update_muzzle_ui(vis: Dictionary) -> void:
	muzzle_option.clear()
	var muzzles: Array = vis.get("muzzles", [])
	if muzzles.is_empty():
		muzzles = [Vector3.ZERO]
		vis["muzzles"] = muzzles
	
	var cur_idx: int = clampi(int(vis.get("current_muzzle_idx", 0)), 0, muzzles.size() - 1)
	vis["current_muzzle_idx"] = cur_idx
	
	for mi in muzzles.size():
		var mv: Vector3 = muzzles[mi]
		muzzle_option.add_item("枪口 %d: (%.1f, %.1f)" % [mi + 1, mv.x, mv.y])
	
	if muzzle_option.item_count > 0:
		muzzle_option.select(cur_idx)
	
	var cur_muz: Vector3 = muzzles[cur_idx]
	vis["muzzle"] = cur_muz
	spin_muzzle_x.set_value_no_signal(cur_muz.x)
	spin_muzzle_y.set_value_no_signal(cur_muz.y)
	spin_muzzle_z.set_value_no_signal(cur_muz.z)
	
	firing_mode_option.select(vis.get("firing_mode", 0))


func _on_muzzle_option_selected(idx: int) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			vis["current_muzzle_idx"] = idx
			var muzzles: Array = vis.get("muzzles", [])
			if idx < muzzles.size():
				var cur_muz: Vector3 = muzzles[idx]
				vis["muzzle"] = cur_muz
				spin_muzzle_x.set_value_no_signal(cur_muz.x)
				spin_muzzle_y.set_value_no_signal(cur_muz.y)
				spin_muzzle_z.set_value_no_signal(cur_muz.z)
				handles_draw.queue_redraw()


func _on_add_muzzle_pressed() -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var last_muz: Vector3 = muzzles[-1] if not muzzles.is_empty() else Vector3.ZERO
			var new_muz: Vector3 = Vector3(-last_muz.x if last_muz.x != 0.0 else 4.0, last_muz.y, last_muz.z)
			muzzles.append(new_muz)
			vis["current_muzzle_idx"] = muzzles.size() - 1
			_update_muzzle_ui(vis)
			handles_draw.queue_redraw()
			_show_toast("已为 %s 添加枪口 %d" % [w_name, muzzles.size()])


func _on_del_muzzle_pressed() -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			if muzzles.size() <= 1:
				_show_toast("至少保留一个枪口！")
				return
			var cur_idx: int = vis.get("current_muzzle_idx", 0)
			muzzles.remove_at(cur_idx)
			vis["current_muzzle_idx"] = clampi(cur_idx, 0, muzzles.size() - 1)
			_update_muzzle_ui(vis)
			handles_draw.queue_redraw()
			_show_toast("已删除枪口")


func _on_mirror_muzzle_pressed() -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var cur_idx: int = vis.get("current_muzzle_idx", 0)
			var cur_muz: Vector3 = muzzles[cur_idx]
			var mir_muz: Vector3 = Vector3(-cur_muz.x, cur_muz.y, cur_muz.z)
			if muzzles.size() == 1:
				muzzles.append(mir_muz)
				vis["current_muzzle_idx"] = 1
				_show_toast("已水平镜像添加枪口 2 (X=%.1f)" % mir_muz.x)
			else:
				muzzles[cur_idx] = mir_muz
				_show_toast("已水平镜像当前枪口 (X=%.1f)" % mir_muz.x)
			_update_muzzle_ui(vis)
			handles_draw.queue_redraw()


func _on_firing_mode_option_selected(idx: int) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			weapon_cache[w_name]["firing_mode"] = idx
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
	_edit_weapon_pivot_component(true, val)


func _on_spin_pivot_y_changed(val: float) -> void:
	_edit_weapon_pivot_component(false, val)


func _edit_weapon_pivot_component(is_x: bool, val: float) -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		return
	var w_name: String = mounts[current_mount_idx].name
	if not weapon_cache.has(w_name):
		return
	var rad: float = deg_to_rad(preview_angle_deg)
	if not pivot_edit_active:
		# 进入枢轴编辑会话：冻结贴图当前显示位置，仅十字标记随数值移动
		pivot_edit_active = true
		pivot_drag_rad = rad
		pivot_drag_anchor = mounts[current_mount_idx].offset - weapon_cache[w_name].pivot.rotated(rad)
		drag_handle_mount_idx = current_mount_idx
	if is_x:
		weapon_cache[w_name].pivot.x = val
	else:
		weapon_cache[w_name].pivot.y = val
	handles_draw.queue_redraw()


func _end_pivot_edit() -> void:
	if pivot_edit_active:
		pivot_edit_active = false
		_update_turret_transforms()


func _on_spin_muzzle_x_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var idx: int = vis.get("current_muzzle_idx", 0)
			if idx < muzzles.size():
				muzzles[idx].x = val
				vis["muzzle"] = muzzles[idx]
				muzzle_option.set_item_text(idx, "枪口 %d: (%.1f, %.1f)" % [idx + 1, muzzles[idx].x, muzzles[idx].y])
				handles_draw.queue_redraw()


func _on_spin_muzzle_y_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var idx: int = vis.get("current_muzzle_idx", 0)
			if idx < muzzles.size():
				muzzles[idx].y = val
				vis["muzzle"] = muzzles[idx]
				muzzle_option.set_item_text(idx, "枪口 %d: (%.1f, %.1f)" % [idx + 1, muzzles[idx].x, muzzles[idx].y])
				handles_draw.queue_redraw()


func _on_spin_muzzle_z_changed(val: float) -> void:
	if current_mount_idx >= 0 and current_mount_idx < mounts.size():
		var w_name: String = mounts[current_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var idx: int = vis.get("current_muzzle_idx", 0)
			if idx < muzzles.size():
				muzzles[idx].z = val
				vis["muzzle"] = muzzles[idx]
				handles_draw.queue_redraw()


func _on_add_mount_pressed() -> void:
	weapon_pick_list.clear()
	for w_name: String in weapon_cfgs.keys():
		weapon_pick_list.add_item(w_name)
	if weapon_pick_list.item_count > 0:
		weapon_pick_list.select(0)
	add_weapon_dialog.popup_centered(Vector2i(360, 420))


func _on_add_weapon_dialog_confirmed() -> void:
	var selected: PackedInt32Array = weapon_pick_list.get_selected_items()
	if selected.is_empty():
		return
	var w_name: String = weapon_pick_list.get_item_text(selected[0])
	var new_mount: Dictionary = {
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
	var removed_name: String = mounts[current_mount_idx].name
	mounts.remove_at(current_mount_idx)
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	if mounts.size() > 0:
		_select_mount(clampi(current_mount_idx, 0, mounts.size() - 1))
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
	var src: Dictionary = mounts[current_mount_idx]
	var dup: Dictionary = {
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
	var tmp: Dictionary = mounts[current_mount_idx]
	mounts[current_mount_idx] = mounts[current_mount_idx - 1]
	mounts[current_mount_idx - 1] = tmp
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(current_mount_idx - 1)


func _on_move_down_pressed() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size() - 1:
		return
	var tmp: Dictionary = mounts[current_mount_idx]
	mounts[current_mount_idx] = mounts[current_mount_idx + 1]
	mounts[current_mount_idx + 1] = tmp
	_rebuild_turret_sprites()
	_update_mount_list_ui()
	_select_mount(current_mount_idx + 1)


# ==============================================================================
# 画布视口缩放与平移
# ==============================================================================

func _set_zoom(z: float, focus_center: Vector2 = Vector2.ZERO) -> void:
	var old_zoom: float = canvas_zoom
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


func _on_stage_container_resized() -> void:
	if canvas_pan == Vector2.ZERO:
		_fit_view()
	else:
		_apply_canvas_transform()


# ==============================================================================
# 鼠标输入处理与拖拽句柄
# ==============================================================================

func _on_stage_container_gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var mb: InputEventMouseButton = event as InputEventMouseButton
		if mb.button_index == MOUSE_BUTTON_MIDDLE or (mb.button_index == MOUSE_BUTTON_RIGHT and not is_dragging_handle):
			if mb.pressed:
				is_panning = true
				pan_start_mouse = mb.position
				pan_start_pos = canvas_pan
			else:
				is_panning = false
		elif mb.button_index == MOUSE_BUTTON_WHEEL_UP and mb.pressed:
			_set_zoom(canvas_zoom * 1.2, mb.position)
		elif mb.button_index == MOUSE_BUTTON_WHEEL_DOWN and mb.pressed:
			_set_zoom(canvas_zoom / 1.2, mb.position)
		elif mb.button_index == MOUSE_BUTTON_LEFT:
			if mb.pressed:
				_handle_left_click_pressed(mb.position)
			else:
				_handle_left_click_released()
				
	elif event is InputEventMouseMotion:
		var mm: InputEventMouseMotion = event as InputEventMouseMotion
		if is_panning:
			canvas_pan = pan_start_pos + (mm.position - pan_start_mouse)
			_apply_canvas_transform()
		elif is_dragging_handle:
			_handle_drag_motion(mm.position)
		else:
			_update_hovered_handle(mm.position)


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
		elif event.keycode == KEY_F:
			_fire_projectiles()
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
		elif event.keycode == KEY_4:
			_set_mode(EditMode.TARGET)
		elif event.keycode == KEY_DELETE or event.keycode == KEY_BACKSPACE:
			if edit_mode == EditMode.MUZZLE:
				_on_del_muzzle_pressed()
				get_viewport().set_input_as_handled()
		elif event.keycode == KEY_M:
			if edit_mode == EditMode.MUZZLE and not event.is_command_or_control_pressed():
				_on_mirror_muzzle_pressed()
				get_viewport().set_input_as_handled()
		elif event.keycode == KEY_A or event.keycode == KEY_N:
			if edit_mode == EditMode.MUZZLE and not event.is_command_or_control_pressed():
				_on_add_muzzle_pressed()
				get_viewport().set_input_as_handled()


func _update_hovered_handle(mouse_pos: Vector2) -> void:
	var prev_hover: Dictionary = hovered_handle.duplicate()
	hovered_handle.clear()
	
	var stage_mouse: Vector2 = stage.to_local(mouse_pos + stage_container.global_position)
	var hit_radius_world: float = 14.0 / maxf(1.0, canvas_zoom)
	var rad: float = deg_to_rad(preview_angle_deg)

	# 0. 目标标靶(Target Dummy)检测
	if target_dummy_enabled:
		var t_visual_pos: Vector2 = target_pos - Vector2(0, target_height)
		if stage_mouse.distance_to(t_visual_pos) <= hit_radius_world * 2.0 or stage_mouse.distance_to(target_pos) <= hit_radius_world:
			hovered_handle = {"type": "target", "index": 0}
			if prev_hover != hovered_handle:
				handles_draw.queue_redraw()
			return
	
	# 逆序检测 (后绘制的在上层)
	for i in range(mounts.size() - 1, -1, -1):
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var mount_pos: Vector2 = m.offset
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		var spr_pos: Vector2 = mount_pos - pivot.rotated(rad)
		
		# 1. 枪口句柄检测 (检测当前武器的所有枪口)
		if edit_mode == EditMode.MUZZLE or i == current_mount_idx:
			var muzzles: Array = vis.get("muzzles", [Vector3.ZERO])
			for mi in range(muzzles.size() - 1, -1, -1):
				var mv: Vector3 = muzzles[mi]
				var m_pos: Vector2 = mount_pos + Vector2(mv.x, mv.y).rotated(rad)
				if stage_mouse.distance_to(m_pos) <= hit_radius_world * 1.5:
					hovered_handle = {"type": "muzzle", "index": i, "muzzle_idx": mi}
					break
			if not hovered_handle.is_empty():
				break
		
		# 2. 旋转中心(Pivot)句柄检测
		if edit_mode == EditMode.PIVOT:
			if stage_mouse.distance_to(spr_pos) <= hit_radius_world * 2.5 or stage_mouse.distance_to(mount_pos) <= hit_radius_world:
				hovered_handle = {"type": "pivot", "index": i}
				break
		
		# 3. 挂载点(Mount)句柄检测
		if stage_mouse.distance_to(mount_pos) <= hit_radius_world:
			hovered_handle = {"type": "mount", "index": i}
			break
		
		# 4. 炮塔精灵检测
		if stage_mouse.distance_to(spr_pos) <= hit_radius_world * 2.5:
			hovered_handle = {"type": "mount" if edit_mode == EditMode.MOUNT else "pivot", "index": i}
			break
	
	if prev_hover != hovered_handle:
		handles_draw.queue_redraw()


func _handle_left_click_pressed(mouse_pos: Vector2) -> void:
	var stage_mouse: Vector2 = stage.to_local(mouse_pos + stage_container.global_position)
	var rad: float = deg_to_rad(preview_angle_deg)
	
	# 【快捷交互】Shift + 左键点击：在点击处瞬间创建新枪口并立刻支持拖拽
	if (edit_mode == EditMode.MUZZLE or Input.is_key_pressed(KEY_SHIFT)) and Input.is_key_pressed(KEY_SHIFT):
		if current_mount_idx >= 0 and current_mount_idx < mounts.size():
			var m: Dictionary = mounts[current_mount_idx]
			var vis: Dictionary = _load_weapon_cache(m.name)
			var click_rel: Vector2 = (stage_mouse - m.offset).rotated(-rad).snapped(Vector2(0.5, 0.5))
			var muzzles: Array = vis.get("muzzles", [])
			var new_muz: Vector3 = Vector3(click_rel.x, click_rel.y, 0.0)
			muzzles.append(new_muz)
			vis["current_muzzle_idx"] = muzzles.size() - 1
			_update_muzzle_ui(vis)
			
			is_dragging_handle = true
			drag_handle_mount_idx = current_mount_idx
			drag_handle_muzzle_idx = muzzles.size() - 1
			drag_mouse_start = stage_mouse
			drag_start_val = click_rel
			active_drag_type = "muzzle"
			
			handles_draw.queue_redraw()
			_show_toast("✨ 已在 (%.1f, %.1f) 新建枪口 %d" % [click_rel.x, click_rel.y, muzzles.size()])
			return

	_update_hovered_handle(mouse_pos)
	
	if not hovered_handle.is_empty():
		if hovered_handle.type == "target" or edit_mode == EditMode.TARGET:
			is_dragging_handle = true
			drag_mouse_start = stage_mouse
			active_drag_type = "target"
			drag_start_val = target_pos
			handles_draw.queue_redraw()
			return

		var idx: int = int(hovered_handle.index)
		_select_mount(idx)
		
		is_dragging_handle = true
		drag_handle_mount_idx = idx
		drag_mouse_start = stage_mouse
		
		var m: Dictionary = mounts[idx]
		var vis: Dictionary = _load_weapon_cache(m.name)
		
		if edit_mode == EditMode.MOUNT or hovered_handle.type == "mount":
			active_drag_type = "mount"
			drag_start_val = m.offset
		elif edit_mode == EditMode.PIVOT or hovered_handle.type == "pivot":
			active_drag_type = "pivot"
			pivot_edit_active = true
			drag_start_val = vis.get("pivot", Vector2.ZERO)
			pivot_drag_rad = deg_to_rad(preview_angle_deg)
			pivot_drag_anchor = m.offset - drag_start_val.rotated(pivot_drag_rad)
		elif edit_mode == EditMode.MUZZLE or hovered_handle.type == "muzzle":
			active_drag_type = "muzzle"
			drag_handle_muzzle_idx = int(hovered_handle.get("muzzle_idx", vis.get("current_muzzle_idx", 0)))
			vis["current_muzzle_idx"] = drag_handle_muzzle_idx
			_update_muzzle_ui(vis)
			var muzzles: Array = vis.get("muzzles", [Vector3.ZERO])
			var muz3: Vector3 = muzzles[drag_handle_muzzle_idx]
			drag_start_val = Vector2(muz3.x, muz3.y)


func _handle_left_click_released() -> void:
	_end_pivot_edit()
	is_dragging_handle = false
	active_drag_type = ""
	handles_draw.queue_redraw()


func _handle_drag_motion(mouse_pos: Vector2) -> void:
	var stage_mouse: Vector2 = stage.to_local(mouse_pos + stage_container.global_position)
	var delta_mouse: Vector2 = stage_mouse - drag_mouse_start
	var rad: float = deg_to_rad(preview_angle_deg)
	
	if active_drag_type == "target":
		var new_tgt: Vector2 = (drag_start_val + delta_mouse).snapped(Vector2(1.0, 1.0))
		target_pos = new_tgt
		spin_target_x.set_value_no_signal(new_tgt.x)
		spin_target_y.set_value_no_signal(new_tgt.y)
		handles_draw.queue_redraw()
		return
	
	if drag_handle_mount_idx < 0 or drag_handle_mount_idx >= mounts.size():
		return
	
	if active_drag_type == "mount":
		var new_pos: Vector2 = (drag_start_val + delta_mouse).snapped(Vector2(0.5, 0.5))
		mounts[drag_handle_mount_idx].offset = new_pos
		spin_mount_x.set_value_no_signal(new_pos.x)
		spin_mount_y.set_value_no_signal(new_pos.y)
		_update_mount_list_ui()
		_update_turret_transforms()
		
	elif active_drag_type == "pivot":
		var w_name: String = mounts[drag_handle_mount_idx].name
		if weapon_cache.has(w_name):
			var new_pivot: Vector2 = (drag_start_val + (stage_mouse - drag_mouse_start).rotated(-pivot_drag_rad)).snapped(Vector2(0.5, 0.5))
			new_pivot = Vector2(
				clampf(new_pivot.x, spin_pivot_x.min_value, spin_pivot_x.max_value),
				clampf(new_pivot.y, spin_pivot_y.min_value, spin_pivot_y.max_value)
			)
			weapon_cache[w_name].pivot = new_pivot
			spin_pivot_x.set_value_no_signal(new_pivot.x)
			spin_pivot_y.set_value_no_signal(new_pivot.y)
			
	elif active_drag_type == "muzzle":
		var w_name: String = mounts[drag_handle_mount_idx].name
		if weapon_cache.has(w_name):
			var vis: Dictionary = weapon_cache[w_name]
			var muzzles: Array = vis.get("muzzles", [])
			var new_muz: Vector2 = (drag_start_val + delta_mouse.rotated(-rad)).snapped(Vector2(0.5, 0.5))
			if drag_handle_muzzle_idx < muzzles.size():
				muzzles[drag_handle_muzzle_idx].x = new_muz.x
				muzzles[drag_handle_muzzle_idx].y = new_muz.y
				vis["muzzle"] = muzzles[drag_handle_muzzle_idx]
				muzzle_option.set_item_text(drag_handle_muzzle_idx, "枪口 %d: (%.1f, %.1f)" % [drag_handle_muzzle_idx + 1, new_muz.x, new_muz.y])
			spin_muzzle_x.set_value_no_signal(new_muz.x)
			spin_muzzle_y.set_value_no_signal(new_muz.y)
	
	handles_draw.queue_redraw()


# ==============================================================================
# 自定义渲染层 (Grid & Handles)
# ==============================================================================

func _on_grid_draw() -> void:
	if not show_grid:
		return
	
	var r: float = 600.0
	var step: float = 16.0
	
	# 绘制细网格
	for x in range(int(-r), int(r) + 1, int(step)):
		var col: Color = Color(0.2, 0.23, 0.28, 0.35 if x % 64 == 0 else 0.15)
		grid_draw.draw_line(Vector2(x, -r), Vector2(x, r), col, 1.0 / maxf(1.0, canvas_zoom))
	for y in range(int(-r), int(r) + 1, int(step)):
		var col: Color = Color(0.2, 0.23, 0.28, 0.35 if y % 64 == 0 else 0.15)
		grid_draw.draw_line(Vector2(-r, y), Vector2(r, y), col, 1.0 / maxf(1.0, canvas_zoom))
	
	# 绘制坐标轴 (红X, 绿Y)
	grid_draw.draw_line(Vector2(-r, 0), Vector2(r, 0), Color(0.9, 0.3, 0.3, 0.6), 1.5 / maxf(1.0, canvas_zoom))
	grid_draw.draw_line(Vector2(0, -r), Vector2(0, r), Color(0.3, 0.8, 0.4, 0.6), 1.5 / maxf(1.0, canvas_zoom))
	grid_draw.draw_circle(Vector2.ZERO, 3.0 / maxf(1.0, canvas_zoom), Color(1, 1, 1, 0.8))
	
	# 碰撞半径圆圈
	if show_collision_radius and current_target_data.has("collision_radius"):
		var col_r: float = float(current_target_data.get("collision_radius", "0"))
		if col_r > 0.0:
			grid_draw.draw_arc(Vector2.ZERO, col_r, 0.0, TAU, 64, Color(0.2, 0.8, 1.0, 0.4), 1.5 / maxf(1.0, canvas_zoom))


func _on_handles_draw() -> void:
	var rad: float = deg_to_rad(preview_angle_deg)
	var line_w: float = 1.5 / maxf(1.0, canvas_zoom)
	
	for i in mounts.size():
		var m: Dictionary = mounts[i]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var is_sel: bool = (i == current_mount_idx)
		var mount_pos: Vector2 = m.offset
		var pivot: Vector2 = vis.get("pivot", Vector2.ZERO)
		
		# 1. 绘制挂载点至车体中心的辅助虚线
		if is_sel and mount_pos != Vector2.ZERO:
			handles_draw.draw_line(Vector2.ZERO, mount_pos, Color(0.4, 0.7, 1.0, 0.3), line_w)
		
		# 2. 绘制挂载点标记 (Mount Anchor: 蓝色圆环与十字)
		var mount_col: Color = Color(0.2, 0.7, 1.0) if is_sel else Color(0.4, 0.6, 0.8, 0.6)
		handles_draw.draw_circle(mount_pos, 3.5 / maxf(1.0, canvas_zoom), mount_col)
		handles_draw.draw_arc(mount_pos, 7.0 / maxf(1.0, canvas_zoom), 0.0, TAU, 24, mount_col, line_w)
		
		# 3. 绘制旋转朝向射线
		var ray_len: float = 24.0 / maxf(1.0, canvas_zoom * 0.25)
		var ray_end: Vector2 = mount_pos + Vector2.DOWN.rotated(rad) * ray_len
		handles_draw.draw_line(mount_pos, ray_end, Color(1.0, 0.5, 0.2, 0.6 if is_sel else 0.25), line_w)
		
		# 4. 绘制枢轴标记 (Pivot Mode: 红色旋转中心十字)
		if edit_mode == EditMode.PIVOT or is_sel:
			var piv_col: Color = Color(1.0, 0.25, 0.25, 0.9) if is_sel else Color(0.8, 0.3, 0.3, 0.5)
			var cross_sz: float = 6.0 / maxf(1.0, canvas_zoom)
			var drag_this: bool = (pivot_edit_active and drag_handle_mount_idx == i)
			var piv_mark: Vector2 = (pivot_drag_anchor + pivot.rotated(pivot_drag_rad)) if drag_this else mount_pos
			# 轴心十字 (拖拽中跟随鼠标, 否则位于 mount_pos)
			handles_draw.draw_line(piv_mark + Vector2(-cross_sz, 0), piv_mark + Vector2(cross_sz, 0), piv_col, line_w * 1.5)
			handles_draw.draw_line(piv_mark + Vector2(0, -cross_sz), piv_mark + Vector2(0, cross_sz), piv_col, line_w * 1.5)

			if drag_this:
				handles_draw.draw_line(piv_mark, mount_pos, Color(1.0, 0.4, 0.4, 0.45), line_w)
		
		# 5. 绘制枪口 (Muzzle Handle: 金黄色圆环，支持绘制全部枪口)
		var muzzles: Array = vis.get("muzzles", [Vector3.ZERO])
		var cur_muz_idx: int = vis.get("current_muzzle_idx", 0)
		
		for mi in muzzles.size():
			var mv: Vector3 = muzzles[mi]
			var cur_muz_pos: Vector2 = mount_pos + Vector2(mv.x, mv.y).rotated(rad)
			var is_active_muz: bool = (is_sel and mi == cur_muz_idx)
			
			if edit_mode == EditMode.MUZZLE or is_sel:
				var muz_col: Color = Color(1.0, 0.9, 0.2, 0.95) if is_active_muz else Color(0.8, 0.7, 0.3, 0.5)
				handles_draw.draw_line(mount_pos, cur_muz_pos, Color(1.0, 0.8, 0.2, 0.3 if not is_active_muz else 0.5), line_w)
				handles_draw.draw_circle(cur_muz_pos, 3.0 / maxf(1.0, canvas_zoom), muz_col)
				handles_draw.draw_arc(cur_muz_pos, 6.0 / maxf(1.0, canvas_zoom), 0.0, TAU, 16, muz_col, line_w * (1.5 if is_active_muz else 1.0))
				if is_active_muz:
					handles_draw.draw_arc(cur_muz_pos, 9.0 / maxf(1.0, canvas_zoom), 0.0, TAU, 16, Color(1.0, 1.0, 0.4, 0.5), line_w)
			
			# 6. 悬停高亮提示圈
			if not hovered_handle.is_empty() and hovered_handle.get("index", -1) == i:
				if hovered_handle.type == "muzzle" and hovered_handle.get("muzzle_idx", 0) == mi:
					handles_draw.draw_arc(cur_muz_pos, 12.0 / maxf(1.0, canvas_zoom), 0.0, TAU, 24, Color(1.0, 1.0, 1.0, 0.8), line_w * 2.0)

		# 7. 炮口闪光循环预览 (支持齐射模式与交替轮射模式)
		if flash_preview_enabled:
			var should_draw: bool = is_sel if not flash_all_mounts else true
			if should_draw:
				var fw_vis: Dictionary = _load_weapon_cache(m.name)
				if not fw_vis.get("muzzle_flash", true):
					continue
				var flash_offs: float = deg_to_rad(float(fw_vis.get("flash_angle", 0.0)))
				var f_mode: int = fw_vis.get("firing_mode", 0) # 0=齐射, 1=轮射
				var life: float = flash_params.life
				var cycle_dur: float = _get_attack_cycle_dur(fw_vis) if is_attacking_preview else (life * 2.2)
				var curr_t: float = weapon_anim_time if is_attacking_preview else flash_time
				var cycle_count: int = int(curr_t / maxf(0.01, cycle_dur))
				
				for mi in muzzles.size():
					# 轮射模式下严格只在当前周期的枪口喷火 (每发射1次切换下一个枪口)
					if f_mode == 1 and muzzles.size() > 1:
						if mi != (cycle_count % muzzles.size()):
							continue
					var mv: Vector3 = muzzles[mi]
					var f_pos: Vector2 = mount_pos + Vector2(mv.x, mv.y).rotated(rad)
					_draw_muzzle_flash_preview(f_pos, rad + flash_offs, fw_vis)

	# --- 8. 绘制受击目标标靶 (Target Dummy) ---
	if target_dummy_enabled:
		var line_w_tgt: float = 2.0 / maxf(1.0, canvas_zoom)
		var t_ground_pos: Vector2 = target_pos
		var t_visual_pos: Vector2 = target_pos - Vector2(0, target_height)
		
		# A. 地面阴影与高度连线
		if target_height > 0.0:
			handles_draw.draw_circle(t_ground_pos, (target_radius * 0.8) / maxf(1.0, canvas_zoom * 0.25), Color(0.0, 0.0, 0.0, 0.35))
			handles_draw.draw_line(t_ground_pos, t_visual_pos, Color(1.0, 0.3, 0.3, 0.4), line_w_tgt, false)
			handles_draw.draw_string(ThemeDB.fallback_font, t_ground_pos + Vector2(10, 4), "Z: %.0f" % target_height, HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(1, 0.6, 0.6, 0.7))
		else:
			handles_draw.draw_circle(t_ground_pos + Vector2(2, 2), target_radius, Color(0.0, 0.0, 0.0, 0.3))
		
		# B. 标靶本体绘制
		var is_tgt_hovered: bool = (hovered_handle.get("type", "") == "target" or edit_mode == EditMode.TARGET)
		var tgt_color: Color = Color(1.0, 1.0, 1.0) if target_hit_flash_timer <= 0.0 else Color(1.0, 0.3, 0.3)
		
		if target_tex:
			var tw: float = target_tex.get_width()
			var th: float = target_tex.get_height()
			var trect: Rect2 = Rect2(t_visual_pos - Vector2(tw * 0.5, th * 0.5), Vector2(tw, th))
			handles_draw.draw_texture_rect(target_tex, trect, false, tgt_color)
		
		# 同心圆标靶环
		var r_outer: float = target_radius
		var r_inner: float = target_radius * 0.55
		var r_core: float = target_radius * 0.25
		
		handles_draw.draw_arc(t_visual_pos, r_outer, 0.0, TAU, 32, Color(1.0, 0.2, 0.2, 0.85 if not is_tgt_hovered else 1.0), line_w_tgt)
		handles_draw.draw_arc(t_visual_pos, r_inner, 0.0, TAU, 24, Color(1.0, 0.9, 0.9, 0.8), line_w_tgt)
		handles_draw.draw_circle(t_visual_pos, r_core, Color(1.0, 0.2, 0.2, 0.9))
		
		# 十字准星瞄准线
		var c_len: float = r_outer * 1.35
		handles_draw.draw_line(t_visual_pos + Vector2(-c_len, 0), t_visual_pos + Vector2(c_len, 0), Color(1.0, 0.3, 0.3, 0.7), line_w_tgt * 0.8)
		handles_draw.draw_line(t_visual_pos + Vector2(0, -c_len), t_visual_pos + Vector2(0, c_len), Color(1.0, 0.3, 0.3, 0.7), line_w_tgt * 0.8)
		
		if is_tgt_hovered:
			handles_draw.draw_arc(t_visual_pos, r_outer + 5.0 / maxf(1.0, canvas_zoom), 0.0, TAU, 32, Color(1.0, 1.0, 0.3, 0.9), line_w_tgt * 1.5)
			
		# 受击波纹动画
		if target_hit_flash_timer > 0.0:
			var ring_r: float = r_outer + (0.16 - target_hit_flash_timer) * 80.0
			var ring_alpha: float = target_hit_flash_timer / 0.16
			handles_draw.draw_arc(t_visual_pos, ring_r, 0.0, TAU, 32, Color(1.0, 0.9, 0.2, ring_alpha), line_w_tgt * 2.0)

	# --- 9. 绘制飞行中的投射物 (Projectiles) ---
	for p in active_projectiles:
		# 抛物线高度视觉位移 (仅炮弹的抛物线弧度向上偏移，弹丸直接从枪口发射)
		var arc_disp: float = (p.current_height - p.start_height) if p.type == 1 else 0.0
		var p_visual_pos: Vector2 = p.position - Vector2(0, arc_disp)
		var rot_rad: float = p.velocity.angle()
		var p_scale: float = p.scale
		
		# A. 地面假光与阴影
		if proj_params.shadow_enabled:
			var sh_pos: Vector2 = p.position + Vector2(1, 1) * (p.current_height * 0.15 + 1.0)
			var sh_alpha: float = clampf(1.0 - (p.current_height / 80.0), 0.2, 0.6)
			handles_draw.draw_circle(sh_pos, 4.0 * p_scale, Color(0.0, 0.0, 0.0, sh_alpha))
			
			var l_color: Color = Color(0.2, 1.0, 0.4, 0.35) if p.is_healing else Color(1.0, 0.5, 0.1, 0.35)
			handles_draw.draw_circle(p.position, 8.0 * p_scale, l_color * sh_alpha)
		
		# B. 尾随烟尘粒子
		for pt in p.trail_particles:
			var pt_alpha: float = (pt.life / pt.max_life) * 0.8
			var pt_col: Color = pt.color
			pt_col.a = pt_alpha
			handles_draw.draw_circle(pt.pos, pt.size, pt_col)
		
		# C. 弹丸本体绘制
		if p.tex:
			var tw: float = p.tex.get_width()
			var th: float = p.tex.get_height()
			var h_f: int = 1
			var t_name: String = str(p.get("tex_name", "")).to_lower()
			if t_name.contains("miss") and tw > th:
				h_f = int(round(tw / 16.0))
			elif tw >= th * 2.0:
				h_f = int(round(tw / th))
			if h_f < 1: h_f = 1

			var fw: float = tw / float(h_f)
			var fh: float = th
			var frame_idx: int = int(p.life_time * 12.0) % h_f
			var src_rect: Rect2 = Rect2(frame_idx * fw, 0, fw, fh)

			# 纵向朝向导弹(Nose在顶部, 尾焰在底部)需附加 PI/2 旋转对齐飞行前向
			var draw_rot: float = rot_rad + (PI / 2.0 if fh >= fw or t_name.contains("miss") else 0.0)
			var p_xform: Transform2D = Transform2D(draw_rot, p_visual_pos)
			p_xform = p_xform.scaled_local(Vector2(p_scale, p_scale))
			handles_draw.draw_set_transform_matrix(handles_draw.get_transform() * p_xform)
			handles_draw.draw_texture_rect_region(p.tex, Rect2(-fw * 0.5, -fh * 0.5, fw, fh), src_rect, Color.WHITE)
			handles_draw.draw_set_transform_matrix(handles_draw.get_transform())
		else:
			# 程序化弹丸样式
			var f_dir: Vector2 = p.velocity.normalized()
			var bullet_col: Color = Color(0.2, 1.0, 0.4) if p.is_healing else (Color(1.0, 0.6, 0.1) if p.type == 2 else Color(1.0, 0.9, 0.3))
			handles_draw.draw_line(p_visual_pos - f_dir * 6.0 * p_scale, p_visual_pos + f_dir * 6.0 * p_scale, bullet_col, 3.0 * p_scale)
			handles_draw.draw_circle(p_visual_pos + f_dir * 6.0 * p_scale, 2.5 * p_scale, Color(1, 1, 1, 0.9))

	# --- 10. 绘制命中爆炸与火花特效 (Impact Effects) ---
	for eff in active_impact_effects:
		var eff_pos: Vector2 = eff.pos - Vector2(0, target_height if target_dummy_enabled else 0.0)
		var progress: float = eff.anim_time / eff.max_life
		var alpha: float = 1.0 - progress
		var eff_scale: float = eff.scale
		
		# 溅射范围圆圈 (AOE Splash Ring)
		if eff.splash_radius > 0.0:
			handles_draw.draw_arc(eff.pos, eff.splash_radius, 0.0, TAU, 36, Color(1.0, 0.4, 0.1, alpha * 0.6), 1.5 / maxf(1.0, canvas_zoom))
			handles_draw.draw_circle(eff.pos, eff.splash_radius, Color(1.0, 0.4, 0.1, alpha * 0.12))
		
		# 火球膨胀圈
		var burst_r: float = (8.0 + progress * 20.0) * eff_scale
		var core_col: Color = Color(0.3, 1.0, 0.5, alpha) if eff.is_healing else Color(1.0, 0.8, 0.2, alpha)
		handles_draw.draw_circle(eff_pos, burst_r * 0.6, core_col)
		handles_draw.draw_arc(eff_pos, burst_r, 0.0, TAU, 24, Color(1.0, 0.4, 0.1, alpha * 0.8), 2.0 / maxf(1.0, canvas_zoom))
		
		# 飞溅火星
		for pt in eff.particles:
			var pt_a: float = (pt.life / pt.max_life)
			var pt_col: Color = pt.color
			pt_col.a = pt_a
			handles_draw.draw_circle(eff_pos + pt.pos, pt.size * (1.0 - progress * 0.5), pt_col)

	# --- 11. 绘制浮动伤害/治疗飘字 (Floating Texts) ---
	for ft in floating_texts:
		var ft_alpha: float = ft.life / ft.max_life
		var ft_col: Color = ft.color
		ft_col.a = ft_alpha
		handles_draw.draw_string(ThemeDB.fallback_font, ft.pos + Vector2(1, 1), ft.text, HORIZONTAL_ALIGNMENT_CENTER, -1, 13, Color(0, 0, 0, ft_alpha * 0.8))
		handles_draw.draw_string(ThemeDB.fallback_font, ft.pos, ft.text, HORIZONTAL_ALIGNMENT_CENTER, -1, 13, ft_col)


# ==============================================================================
# 保存与写回配置
# ==============================================================================

func _save_current_unit() -> void:
	if current_target_path.is_empty():
		return
	
	var lines: PackedStringArray = FileAccess.get_file_as_string(current_target_path).split("\n")
	var new_lines: Array = []
	var mount_written: bool = false
	
	for line: String in lines:
		var stripped: String = line.strip_edges()
		if stripped.begins_with("weapon_mount"):
			if not mount_written:
				# 在第一条 weapon_mount 所在位置顺序写入所有挂载点
				for m: Dictionary in mounts:
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
		for m: Dictionary in mounts:
			new_lines.append("weapon_mount = %s, %.1f, %.1f" % [m.name, m.offset.x, m.offset.y])
	
	var f: FileAccess = FileAccess.open(current_target_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(new_lines))
		f.close()
		for m: Dictionary in mounts:
			m["orig_offset"] = m.offset
		_show_toast("✅ 已保存单位配置: %s" % current_target_path.get_file())
	else:
		_show_toast("❌ 保存失败: 无法写入文件")


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
	var muzzles: Array = vis.get("muzzles", [])
	if muzzles.is_empty():
		muzzles = [vis.get("muzzle", Vector3.ZERO)]
	var muz: Vector3 = muzzles[0]
	
	var m_strs: PackedStringArray = []
	for m_item in muzzles:
		var mv: Vector3 = m_item as Vector3 if m_item is Vector3 else Vector3.ZERO
		m_strs.append("%.1f, %.1f, %.1f" % [mv.x, mv.y, mv.z])
	var muzzles_line_val: String = " | ".join(m_strs)
	var f_mode_val: String = "Alternating" if vis.get("firing_mode", 0) == 1 else "Simultaneous"

	var lines: PackedStringArray = FileAccess.get_file_as_string(w_path).split("\n")
	var pivot_written: bool = false
	var muzzle_written: bool = false
	var fps_written: bool = false
	var new_lines: Array = []
	var cur_fps: int = maxi(1, int(spin_anim_fps.value))
	
	for line: String in lines:
		var stripped: String = line.strip_edges()
		if stripped.begins_with("rotation_center"):
			new_lines.append("rotation_center = %.1f, %.1f" % [pivot.x, pivot.y])
			pivot_written = true
			continue
		elif stripped.begins_with("muzzle_offset") and not stripped.begins_with("muzzle_offsets"):
			new_lines.append("muzzle_offset = %.1f, %.1f, %.1f" % [muz.x, muz.y, muz.z])
			if muzzles.size() > 1:
				new_lines.append("muzzle_offsets = %s" % muzzles_line_val)
				new_lines.append("firing_mode = %s" % f_mode_val)
			muzzle_written = true
			continue
		elif stripped.begins_with("muzzle_offsets") or stripped.begins_with("firing_mode"):
			# 跳过已存在的 muzzle_offsets / firing_mode 字段，在上方或下方集中写入
			continue
		elif stripped.begins_with("anim_fps"):
			new_lines.append("anim_fps = %d" % cur_fps)
			fps_written = true
			continue
		elif stripped.begins_with("flash_preset") or stripped.begins_with("flash_scale") or stripped.begins_with("flash_life") or stripped.begins_with("muzzle_flash") or stripped.begins_with("flash_trigger_frame") or stripped.begins_with("flash_frame") or stripped.begins_with("muzzle_flash_angle"):
			# 跳过旧的枪口闪光字段，统一写入
			continue
		new_lines.append(line)
	
	if not pivot_written:
		new_lines.append("rotation_center = %.1f, %.1f" % [pivot.x, pivot.y])
	if not muzzle_written:
		new_lines.append("muzzle_offset = %.1f, %.1f, %.1f" % [muz.x, muz.y, muz.z])
		if muzzles.size() > 1:
			new_lines.append("muzzle_offsets = %s" % muzzles_line_val)
			new_lines.append("firing_mode = %s" % f_mode_val)
	if not fps_written:
		new_lines.append("anim_fps = %d" % cur_fps)
	
	# 自动追加并保存开火特效配置
	var cur_flash_en: bool = bool(vis.get("muzzle_flash", true))
	var cur_preset: String = str(vis.get("flash_preset", "cannon"))
	var cur_flash_angle: float = float(vis.get("flash_angle", 0.0))
	var cur_flash_trig: int = int(spin_flash_frame.value)
	new_lines.append("")
	new_lines.append("; 枪口开火特效配置")
	new_lines.append("muzzle_flash = %s" % ("true" if cur_flash_en else "false"))
	new_lines.append("flash_preset = %s" % cur_preset)
	new_lines.append("flash_scale = %.1f" % flash_params.scale)
	new_lines.append("flash_life = %.2f" % flash_params.life)
	new_lines.append("muzzle_flash_angle = %.1f" % cur_flash_angle)
	new_lines.append("flash_trigger_frame = %d" % cur_flash_trig)
	
	var f: FileAccess = FileAccess.open(w_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(new_lines))
		f.close()
		vis["anim_fps"] = cur_fps
		vis["flash_preset"] = cur_preset
		vis["flash_scale"] = flash_params.scale
		vis["flash_life"] = flash_params.life
		vis["flash_angle"] = cur_flash_angle
		vis["flash_trigger_frame"] = cur_flash_trig
		_show_toast("✅ 已保存武器配置 (FPS: %d) 与专属开火特效: %s" % [cur_fps, w_path.get_file()])
	else:
		_show_toast("❌ 保存失败: 无法写入武器配置")


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
	var tw: Tween = create_tween()
	tw.tween_property(toast_label, "modulate:a", 0.0, 0.5)


# ==============================================================================
# 炮口闪光预览 (像素风格点阵预览与调参)
# ==============================================================================

func _get_pixel_flash_color(level: int, palette_type: int, base_col: Color) -> Color:
	if level >= 4:
		return Color(1.0, 1.0, 0.95, 1.0) # 纯白白热
	if palette_type == 3:
		# 能量脉冲: 白 -> 亮青 -> 霓虹蓝 -> 深紫
		if level == 3: return Color(0.40, 0.95, 1.0, 1.0)
		if level == 2: return Color(0.15, 0.55, 1.0, 1.0)
		return Color(0.45, 0.10, 0.85, 1.0)
	elif palette_type == 2:
		# 导弹尾焰与排气烟幕: 白 -> 炽橙尾焰 -> 浅灰浓烟 -> 深灰排气
		if level == 3: return Color(1.0, 0.72, 0.18, 1.0)
		if level == 2: return Color(0.82, 0.84, 0.88, 1.0)
		return Color(0.38, 0.40, 0.45, 1.0)
	elif palette_type == 1:
		# 速射机枪: 白 -> 金白 -> 明黄 -> 橙褐
		if level == 3: return Color(1.0, 0.95, 0.45, 1.0)
		if level == 2: return Color(1.0, 0.72, 0.18, 1.0)
		return Color(0.80, 0.30, 0.08, 1.0)
	else:
		# 经典火炮 / 战列巨炮
		if level == 3: return Color(1.0, 0.86, 0.22, 1.0)
		if level == 2: return base_col
		return Color(0.75, 0.15, 0.05, 1.0)


func _draw_muzzle_flash_preview(center: Vector2, rot_rad: float, vis: Dictionary = {}) -> void:
	var life: float = flash_params.life
	var lr: float = 1.0

	if is_attacking_preview:
		var fps: int = vis.get("anim_fps", 10)
		if fps <= 0: fps = 10
		var cycle_dur: float = _get_attack_cycle_dur(vis)
		var t_in_cycle: float = fmod(weapon_anim_time, cycle_dur)
		var trig_frame: int = vis.get("flash_trigger_frame", 0)
		var trig_time: float = float(trig_frame) / float(fps)
		if t_in_cycle >= trig_time and t_in_cycle < trig_time + life:
			lr = (t_in_cycle - trig_time) / life
		else:
			return
	else:
		# 独立闪光循环: 闪光寿命 + 1.2倍间歇, 模拟开火节奏
		var cycle: float = life * 2.2
		lr = fmod(flash_time, cycle) / life
		if lr >= 1.0:
			return

	# 像素动画阶段 (4 帧爆裂节奏)
	var frame: int = int(lr * 4.0)
	if frame >= 4:
		return

	var px: float = maxf(float(flash_params.get("pixel_size", 1.0)), 0.5)
	var scale: float = maxf(flash_params.scale * 0.5, 1.0)
	var dir: Vector2 = Vector2.RIGHT.rotated(rot_rad)
	var pos: Vector2 = center + dir * flash_params.forward_speed * life * 0.35 * lr

	var palette_type: int = int(flash_params.get("palette_type", 0))
	var base_col: Color = flash_params.color

	# 网格范围
	var grid_span: int = int(ceil(scale / px)) + 2

	for iy in range(-grid_span, grid_span + 1):
		for ix in range(-grid_span, grid_span + 1):
			var local_px: float = float(ix) * px
			var local_py: float = float(iy) * px
			var gx: float = local_px / scale
			var gy: float = local_py / scale
			var gr: float = sqrt(gx * gx + gy * gy)
			
			var level: int = 0
			if frame == 0:
				var r_px: float = sqrt(local_px * local_px + local_py * local_py)
				var white_r: float = maxf(px * 1.0, scale * flash_params.core_radius * 0.42)
				var yellow_r: float = white_r + px * 1.0
				var orange_r: float = yellow_r + px * 0.9
				var manhattan: float = absf(local_px) + absf(local_py)
				if manhattan <= white_r * 1.35 or r_px <= white_r:
					level = 4
				elif manhattan <= yellow_r * 1.38 or r_px <= yellow_r:
					level = 3
				elif manhattan <= orange_r * 1.35:
					level = 2
			elif frame == 1:
				var cone_x: float = gx - 0.1
				var cone_len: float = flash_params.cone_length * 0.95
				var cone_shape: float = (absf(gy) / (1.0 - cone_x / cone_len * 0.6)) if (cone_x > 0.0 and cone_x < cone_len) else 99.0
				var side_tan: float = tan(deg_to_rad(flash_params.side_angle_deg))
				var s_diff1: float = absf(gy - gx * side_tan)
				var s_diff2: float = absf(-gy - gx * side_tan)
				var is_side: bool = (flash_params.side_strength > 0.15) and (s_diff1 < 0.18 or s_diff2 < 0.18) and (absf(gx) < flash_params.side_length * 0.85)

				if gr <= flash_params.core_radius * 0.4 and absf(gx) < 0.25:
					level = 4
				elif cone_shape <= 0.14 and cone_x > 0.0:
					level = 3
				elif cone_shape <= 0.26 or is_side:
					level = 2
				elif cone_shape <= 0.38 or (is_side and (s_diff1 < 0.25 or s_diff2 < 0.25)):
					level = 1
			elif frame == 2:
				var burst_center: Vector2 = Vector2(0.45, 0.0)
				var d_burst: float = Vector2(gx - burst_center.x, gy * 1.5).length()
				var smoke_puff: bool = false
				if flash_params.spark_intensity > 0.1:
					var sp_r: float = px / scale * 1.25
					if Vector2(gx - 0.65, gy - 0.22).length() <= sp_r or \
					   Vector2(gx - 0.72, gy + 0.18).length() <= sp_r or \
					   Vector2(gx + 0.25, gy - 0.32).length() <= sp_r or \
					   Vector2(gx + 0.25, gy + 0.32).length() <= sp_r or \
					   Vector2(gx - 0.15, gy - 0.38).length() <= sp_r or \
					   Vector2(gx - 0.15, gy + 0.38).length() <= sp_r:
						smoke_puff = true
				if d_burst <= 0.16:
					level = 3
				elif d_burst <= 0.32 or smoke_puff:
					level = 2
				elif d_burst <= 0.48:
					level = 1
			elif frame == 3:
				var em_r: float = px / scale * 1.1
				if Vector2(gx - 0.55, gy - 0.15).length() <= em_r or \
				   Vector2(gx - 0.68, gy + 0.12).length() <= em_r or \
				   Vector2(gx + 0.20, gy - 0.35).length() <= em_r or \
				   Vector2(gx + 0.20, gy + 0.35).length() <= em_r:
					level = 1

			if level > 0:
				var col: Color = _get_pixel_flash_color(level, palette_type, base_col)
				var world_p: Vector2 = pos + Vector2(local_px, local_py).rotated(rot_rad)
				# 绘制方形实心像素块 (Square pixel rect)
				var r_rect: Rect2 = Rect2(world_p - Vector2(px * 0.5, px * 0.5), Vector2(px, px))
				handles_draw.draw_rect(r_rect, col)


## 把当前调参结果复制为 GDScript 代码, 粘贴回 main.gd 注册区即可在游戏中生效
func _on_flash_copy_pressed() -> void:
	var c: Color = flash_params.color
	var side_rad: float = deg_to_rad(flash_params.side_angle_deg)
	var px: float = float(flash_params.get("pixel_size", 1.0))
	var pal: int = int(flash_params.get("palette_type", 0))
	var code: String = ('effect_manager.register_effect_type("MuzzleFlash", null, 500, 0.0, 0.85, 1.0, true, true)\n'
		+ 'effect_manager.set_pixel_flash_params("MuzzleFlash", Color(%.3f, %.3f, %.3f), %.1f, %d, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.1f, %.2f, %.1f)') % [
		c.r, c.g, c.b,
		px,
		pal,
		flash_params.core_radius,
		flash_params.cone_length,
		flash_params.side_strength,
		side_rad,
		flash_params.side_length,
		flash_params.spark_intensity,
		flash_params.scale,
		flash_params.life,
		flash_params.forward_speed
	]
	DisplayServer.clipboard_set(code)
	_show_toast("✅ 已复制完整像素开火参数代码, 请粘贴到 main.gd 的注册区")


# ==============================================================================
# 工具辅助函数
# ==============================================================================

func _get_dir_files(dir_path: String, extension: String) -> Array[String]:
	var out: Array[String] = []
	var d: DirAccess = DirAccess.open(dir_path)
	if d == null:
		return out
	d.list_dir_begin()
	var f: String = d.get_next()
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
	var f: FileAccess = FileAccess.open(path, FileAccess.READ)
	if f == null:
		return d
	while not f.eof_reached():
		var line: String = f.get_line().strip_edges()
		if line.is_empty() or line.begins_with(";") or line.begins_with("#") or line.begins_with("["):
			continue
		var eq: int = line.find("=")
		if eq <= 0:
			continue
		var k: String = line.substr(0, eq).strip_edges()
		var v: String = line.substr(eq + 1).strip_edges()
		var semi: int = v.find(";")
		if semi >= 0:
			v = v.substr(0, semi).strip_edges()
		d[k] = v
	f.close()
	return d


# ==============================================================================
# 投射物与标靶攻击系统 (Projectile Simulation & Target Dummy)
# ==============================================================================

func _scan_all_projectiles() -> void:
	proj_configs.clear()
	proj_tex_cache.clear()
	
	# 扫描 config/projectile
	for path: String in _get_dir_files(PROJECTILE_DIR, ".cfg"):
		var d: Dictionary = _parse_cfg_file(path)
		var p_name: String = str(d.get("projectile_name", path.get_file().get_basename()))
		proj_configs[p_name] = {
			"path": path,
			"data": d
		}
	
	# 预设类型选项
	proj_type_option.clear()
	proj_type_option.add_item("🔫 直线子弹 (Bullet)", 0)
	proj_type_option.add_item("💣 抛物线炮弹 (Shell)", 1)
	proj_type_option.add_item("🚀 制导追踪导弹 (Missile)", 2)
	proj_type_option.select(proj_params.type)
	
	# 贴图选项
	proj_texture_option.clear()
	var proj_tex_files: Array = [
		"bullet.png",
		"shell.png",
		"mortar_shell.png",
		"missle.png",
		"guide_missile.png",
		"heavy_missile.png",
		"repair_beam.png"
	]
	
	for tex_f in proj_tex_files:
		proj_texture_option.add_item(tex_f)
		_load_projectile_texture(tex_f)
	
	proj_texture_option.select(0)


func _load_projectile_texture(tex_name: String) -> Texture2D:
	if proj_tex_cache.has(tex_name):
		return proj_tex_cache[tex_name]
	
	var full_p: String = PROJ_TEXTURE_DIR.path_join(tex_name)
	var tex: Texture2D = null
	if ResourceLoader.exists(full_p):
		tex = load(full_p)
	if tex == null:
		var global_p: String = ProjectSettings.globalize_path(full_p)
		if FileAccess.file_exists(global_p):
			var img: Image = Image.load_from_file(global_p)
			if img != null and not img.is_empty():
				tex = ImageTexture.create_from_image(img)
	if tex:
		proj_tex_cache[tex_name] = tex
	return tex


func _update_proj_ui_from_params() -> void:
	proj_type_option.select(proj_params.type)
	for i in range(proj_texture_option.item_count):
		if proj_texture_option.get_item_text(i) == proj_params.tex_name:
			proj_texture_option.select(i)
			break
	spin_proj_speed.set_value_no_signal(proj_params.speed)
	spin_proj_arc_height.set_value_no_signal(proj_params.arc_height)
	spin_proj_accel.set_value_no_signal(proj_params.acceleration)
	spin_proj_turn_speed.set_value_no_signal(proj_params.turn_speed)
	spin_proj_splash.set_value_no_signal(proj_params.splash_radius)
	spin_proj_scale.set_value_no_signal(proj_params.scale)
	proj_healing_check.set_pressed_no_signal(proj_params.is_healing)
	proj_trail_check.set_pressed_no_signal(proj_params.trail_enabled)
	proj_shadow_check.set_pressed_no_signal(proj_params.shadow_enabled)


func _fire_projectiles() -> void:
	if not proj_enable_firing or mounts.is_empty():
		return
	
	var rad: float = deg_to_rad(preview_angle_deg)
	var target_mount_indices: Array = []
	if proj_all_mounts:
		for i in mounts.size():
			target_mount_indices.append(i)
	else:
		if current_mount_idx >= 0 and current_mount_idx < mounts.size():
			target_mount_indices.append(current_mount_idx)
		elif mounts.size() > 0:
			target_mount_indices.append(0)
			
	for m_idx in target_mount_indices:
		var m: Dictionary = mounts[m_idx]
		var vis: Dictionary = _load_weapon_cache(m.name)
		var mount_pos: Vector2 = m.offset
		var muzzles: Array = vis.get("muzzles", [Vector3.ZERO])
		var f_mode: int = vis.get("firing_mode", 0) # 0=齐射, 1=轮射
		var cur_muz_idx: int = vis.get("current_muzzle_idx", 0)
		
		var start_indices: Array = []
		if f_mode == 1 and muzzles.size() > 1:
			start_indices.append(cur_muz_idx % muzzles.size())
			vis["current_muzzle_idx"] = (cur_muz_idx + 1) % muzzles.size()
			_update_muzzle_ui(vis)
		else:
			for mi in muzzles.size():
				start_indices.append(mi)
				
		for mi in start_indices:
			var mv: Vector3 = muzzles[mi]
			var f_pos: Vector2 = mount_pos + Vector2(mv.x, mv.y).rotated(rad)
			var base_h: float = float(current_target_data.get("base_height", "0.0"))
			var start_h: float = base_h + mv.z
			_spawn_single_projectile(f_pos, start_h, rad, vis)


func _spawn_single_projectile(start_pos: Vector2, start_height: float, muz_dir_rad: float, _vis: Dictionary) -> void:
	var tex_name: String = str(proj_params.get("tex_name", "bullet.png"))
	var tex: Texture2D = _load_projectile_texture(tex_name)
	var dir_to_target: Vector2 = (target_pos - start_pos).normalized()
	if dir_to_target.length_squared() < 0.001:
		dir_to_target = Vector2.DOWN.rotated(muz_dir_rad)
	
	var shoot_dir: Vector2 = Vector2.DOWN.rotated(muz_dir_rad)
	var init_vel: Vector2 = dir_to_target if (is_aiming_at_target or proj_params.type != 2) else shoot_dir
	var total_d: float = start_pos.distance_to(target_pos)
	
	var p: Dictionary = {
		"start_pos": start_pos,
		"position": start_pos,
		"target_pos": target_pos,
		"start_height": start_height,
		"target_height": target_height,
		"current_height": start_height,
		"velocity": init_vel,
		"speed": float(proj_params.speed),
		"acceleration": float(proj_params.acceleration),
		"turn_speed": float(proj_params.turn_speed),
		"arc_height": float(proj_params.arc_height),
		"splash_radius": float(proj_params.splash_radius),
		"damage": float(proj_params.damage),
		"is_healing": bool(proj_params.is_healing),
		"type": int(proj_params.type),
		"scale": float(proj_params.scale),
		"tex": tex,
		"tex_name": tex_name,
		"traveled_dist": 0.0,
		"total_dist": total_d,
		"life_time": 0.0,
		"trail_timer": 0.0,
		"trail_particles": []
	}
	active_projectiles.append(p)


func _update_projectiles(delta: float) -> void:
	var dead_indices: Array = []
	
	for i in active_projectiles.size():
		var p: Dictionary = active_projectiles[i]
		p.life_time += delta
		
		# 1. 导弹追踪运动
		if p.type == 2: # Missile
			p.speed += p.acceleration * delta
			var dir_to_t: Vector2 = (target_pos - p.position).normalized()
			var cur_ang: float = p.velocity.angle()
			var target_ang: float = dir_to_t.angle()
			var new_ang: float = rotate_toward(cur_ang, target_ang, p.turn_speed * delta)
			p.velocity = Vector2.RIGHT.rotated(new_ang)
			
			var move_step: float = p.speed * delta
			p.position += p.velocity * move_step
			p.traveled_dist += move_step
			p.current_height = lerpf(p.current_height, target_height, clampf(p.turn_speed * delta * 0.5, 0.0, 1.0))
			
			if proj_params.trail_enabled:
				p.trail_timer += delta
				if p.trail_timer >= 0.025:
					p.trail_timer = 0.0
					var noz_pos: Vector2 = p.position - p.velocity * (10.0 * p.scale)
					p.trail_particles.append({
						"pos": noz_pos,
						"vel": -p.velocity * 12.0 + Vector2(randf_range(-6, 6), randf_range(-6, 6)),
						"life": 0.28,
						"max_life": 0.28,
						"size": randf_range(2.5, 4.5) * p.scale,
						"color": Color(1.0, randf_range(0.5, 0.9), 0.2, 0.9)
					})
			
			var dist_to_tgt: float = p.position.distance_to(target_pos)
			if dist_to_tgt <= maxf(12.0 * p.scale, move_step * 1.2) or p.life_time >= 5.0:
				_on_projectile_hit(p)
				dead_indices.append(i)
				continue
				
		# 2. 抛物线炮弹运动
		elif p.type == 1: # Shell
			var move_step: float = p.speed * delta
			p.traveled_dist += move_step
			var t: float = clampf(p.traveled_dist / maxf(1.0, p.total_dist), 0.0, 1.0)
			p.position = p.start_pos.lerp(p.target_pos, t)
			var base_h: float = lerpf(p.start_height, p.target_height, t)
			p.current_height = base_h + (4.0 * p.arc_height * t * (1.0 - t))
			p.velocity = (p.target_pos - p.start_pos).normalized()
			
			if proj_params.trail_enabled and randf() < 0.4:
				p.trail_particles.append({
					"pos": p.position,
					"vel": Vector2(randf_range(-4, 4), randf_range(-4, 4)),
					"life": 0.2,
					"max_life": 0.2,
					"size": 2.5 * p.scale,
					"color": Color(0.8, 0.8, 0.8, 0.4)
				})
			
			if t >= 1.0 or p.life_time >= 5.0:
				_on_projectile_hit(p)
				dead_indices.append(i)
				continue
				
		# 3. 直线子弹运动
		else: # Bullet
			var move_step: float = p.speed * delta
			p.traveled_dist += move_step
			var t: float = clampf(p.traveled_dist / maxf(1.0, p.total_dist), 0.0, 1.0)
			p.position = p.start_pos.lerp(p.target_pos, t)
			p.current_height = lerpf(p.start_height, p.target_height, t)
			p.velocity = (p.target_pos - p.start_pos).normalized()
			
			if t >= 1.0 or p.life_time >= 5.0:
				_on_projectile_hit(p)
				dead_indices.append(i)
				continue
				
		# 更新尾随粒子
		var part_dead: Array = []
		for pi in p.trail_particles.size():
			var pt: Dictionary = p.trail_particles[pi]
			pt.life -= delta
			pt.pos += pt.vel * delta
			if pt.life <= 0.0:
				part_dead.append(pi)
		for pi in range(part_dead.size() - 1, -1, -1):
			p.trail_particles.remove_at(part_dead[pi])
			
	for i in range(dead_indices.size() - 1, -1, -1):
		active_projectiles.remove_at(dead_indices[i])


func _on_projectile_hit(p: Dictionary) -> void:
	target_hit_count += 1
	target_hit_flash_timer = 0.16
	target_hit_label.text = "🎯 标靶命中: %d 次" % target_hit_count
	
	# 创建命中爆炸与火花特效
	var eff: Dictionary = {
		"pos": p.position,
		"height": p.current_height,
		"type": p.type,
		"splash_radius": p.splash_radius,
		"is_healing": p.is_healing,
		"anim_time": 0.0,
		"max_life": 0.35,
		"scale": p.scale,
		"particles": []
	}
	
	var part_count: int = 16 if p.type > 0 else 8
	var base_col: Color = Color(0.2, 1.0, 0.4) if p.is_healing else (Color(1.0, 0.55, 0.1) if p.type == 2 else Color(1.0, 0.8, 0.2))
	for _k in range(part_count):
		var ang: float = randf() * TAU
		var spd: float = randf_range(30.0, 100.0) * p.scale
		eff.particles.append({
			"pos": Vector2.ZERO,
			"vel": Vector2.RIGHT.rotated(ang) * spd,
			"life": randf_range(0.15, 0.35),
			"max_life": 0.35,
			"size": randf_range(2.0, 5.0) * p.scale,
			"color": base_col
		})
	active_impact_effects.append(eff)
	
	# 浮动飘字
	var dmg_str: String = ("+%.0f HP" % p.damage) if p.is_healing else ("-%.0f" % p.damage)
	if p.splash_radius > 0.0:
		dmg_str += " (AOE)"
	var f_col: Color = Color(0.3, 1.0, 0.5) if p.is_healing else Color(1.0, 0.9, 0.3)
	floating_texts.append({
		"text": dmg_str,
		"pos": target_pos - Vector2(0, target_height + 16.0),
		"vel": Vector2(randf_range(-8.0, 8.0), -35.0),
		"life": 0.65,
		"max_life": 0.65,
		"color": f_col
	})


func _update_impact_effects(delta: float) -> void:
	var dead: Array = []
	for i in active_impact_effects.size():
		var eff: Dictionary = active_impact_effects[i]
		eff.anim_time += delta
		for pt in eff.particles:
			pt.life -= delta
			pt.pos += pt.vel * delta
		if eff.anim_time >= eff.max_life:
			dead.append(i)
	for i in range(dead.size() - 1, -1, -1):
		active_impact_effects.remove_at(dead[i])


func _update_floating_texts(delta: float) -> void:
	var dead: Array = []
	for i in floating_texts.size():
		var ft: Dictionary = floating_texts[i]
		ft.life -= delta
		ft.pos += ft.vel * delta
		if ft.life <= 0.0:
			dead.append(i)
	for i in range(dead.size() - 1, -1, -1):
		floating_texts.remove_at(dead[i])


func _sync_proj_from_current_weapon() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		return
	var w_name: String = mounts[current_mount_idx].name
	if not weapon_cfgs.has(w_name):
		return
	var d: Dictionary = weapon_cfgs[w_name].data
	var p_type_name: String = str(d.get("projectile_type_name", "MarineBullet"))
	var p_spd: float = float(d.get("projectile_speed", "800.0"))
	var p_splash: float = float(d.get("splash_radius", "0.0"))
	var p_dmg: float = float(d.get("damage", "10.0"))
	
	proj_params.preset_name = p_type_name
	proj_params.speed = p_spd
	proj_params.splash_radius = p_splash
	proj_params.damage = p_dmg
	
	if proj_configs.has(p_type_name):
		var pd: Dictionary = proj_configs[p_type_name].data
		proj_params.type = int(pd.get("projectile_type", "0"))
		proj_params.arc_height = float(pd.get("arc_height", "15.0"))
		proj_params.acceleration = float(pd.get("acceleration", "300.0"))
		proj_params.turn_speed = float(pd.get("turn_speed", "6.0"))
		proj_params.is_healing = str(pd.get("is_healing", "0")).to_lower() == "true" or pd.get("is_healing", "0") == "1"
		var v_path: String = str(pd.get("visual_path", ""))
		if not v_path.is_empty():
			proj_params.tex_name = v_path.get_file()
	else:
		if p_type_name.to_lower().contains("missile") or p_type_name.to_lower().contains("missle"):
			proj_params.type = 2
			proj_params.tex_name = "missle.png"
		elif p_type_name.to_lower().contains("shell"):
			proj_params.type = 1
			proj_params.tex_name = "shell.png"
		elif p_type_name.to_lower().contains("repair"):
			proj_params.type = 0
			proj_params.is_healing = true
			proj_params.tex_name = "repair_beam.png"
		else:
			proj_params.type = 0
			proj_params.tex_name = "bullet.png"
			
	_update_proj_ui_from_params()


func _save_projectile_to_weapon() -> void:
	if current_mount_idx < 0 or current_mount_idx >= mounts.size():
		_show_toast("未选中有效武器")
		return
	var w_name: String = mounts[current_mount_idx].name
	if not weapon_cfgs.has(w_name):
		_show_toast("未找到武器配置")
		return
	var w_path: String = weapon_cfgs[w_name].path
	var lines: PackedStringArray = FileAccess.get_file_as_string(w_path).split("\n")
	var new_lines: Array = []
	var proj_type_written: bool = false
	var spd_written: bool = false
	var splash_written: bool = false
	var dmg_written: bool = false
	
	for line: String in lines:
		var stripped: String = line.strip_edges()
		if stripped.begins_with("projectile_type_name"):
			new_lines.append("projectile_type_name = %s" % proj_params.preset_name)
			proj_type_written = true
			continue
		elif stripped.begins_with("projectile_speed"):
			new_lines.append("projectile_speed = %.1f" % proj_params.speed)
			spd_written = true
			continue
		elif stripped.begins_with("splash_radius"):
			new_lines.append("splash_radius = %.1f" % proj_params.splash_radius)
			splash_written = true
			continue
		elif stripped.begins_with("damage"):
			new_lines.append("damage = %.1f" % proj_params.damage)
			dmg_written = true
			continue
		new_lines.append(line)
		
	if not proj_type_written:
		new_lines.append("projectile_type_name = %s" % proj_params.preset_name)
	if not spd_written:
		new_lines.append("projectile_speed = %.1f" % proj_params.speed)
	if not splash_written:
		new_lines.append("splash_radius = %.1f" % proj_params.splash_radius)
	if not dmg_written:
		new_lines.append("damage = %.1f" % proj_params.damage)
		
	var f: FileAccess = FileAccess.open(w_path, FileAccess.WRITE)
	if f:
		f.store_string("\n".join(new_lines))
		f.close()
		_show_toast("💾 投射物参数已成功写入武器配置: %s" % w_path.get_file())
	else:
		_show_toast("❌ 保存失败")
