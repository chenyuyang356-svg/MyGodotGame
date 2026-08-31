extends Node

const ModManager = preload("res://main/mod_manager.gd")
const Tuning = preload("res://main/game_tuning.tres")

@export var minimap_border: PanelContainer
@export var minimap_container: SubViewportContainer
@export var minimap_viewport: SubViewport
@export var minimap_camera: Camera3D
@onready var game_over_ui: Control = %GameOverUI
@onready var pause_menu: Control = %PauseMenu

func _ready() -> void:
	print("get ready")
	game_over_ui.change_ui.connect(_on_change_ui)
	pause_menu.change_ui.connect(_on_change_ui)
	
	var map_idx = GlobalGameManager.selected_map_index
	var map_res = GlobalGameManager.available_maps[map_idx]
	
	if map_res:
		setup_game_with_map(map_res)

func _on_change_ui(scene_path: String):
	get_parent().change_ui(scene_path)

func setup_game_with_map(map_res: MapResource) -> void:
	# 1. 实例化地图场景
	var map_instance = map_res.map_scene.instantiate()
	add_child(map_instance)
	
	# 2. 获取 TileMapLayer (假设它是场景根节点或者叫这个名字)
	var tile_map_layer: TileMapLayer = null
	if map_instance is TileMapLayer:
		tile_map_layer = map_instance
	else:
		tile_map_layer = map_instance.get_node("TileMapLayer")
	
	if not tile_map_layer:
		push_error("地图场景中未找到 TileMapLayer")
		return
	
	# 3. 提取出生点坐标
	var spawn_positions: Dictionary = {} # key: spawn_id, value: Vector2
	var spawn_points_node = map_instance.get_node_or_null("SpawnPoints")
	
	if spawn_points_node:
		for marker in spawn_points_node.get_children():
			if marker is Marker2D:
				var spawn_id = marker.name.to_int()
				spawn_positions[spawn_id] = marker.global_position
	
	
	var map_manager: MapManager = $MapManager
	var unit_manager: UnitManager = $UnitManager
	var building_manager: BuildingManager = $BuildingManager
	var flow_field_manager: FlowFieldManager = $FlowFieldManager
	var selection_manager: SelectionManager = $SelectionManager
	var group_manager: GroupManager = $GroupManager
	var projectile_manager: ProjectileManager = $ProjectileManager
	var attack_manager: AttackManager = $AttackManager
	var economy_manager: EconomyManager = $EconomyManager
	var fog_manager: FogManager = $FogManager
	var weapon_manager: WeaponManager = $WeaponManager
	var effect_manager: EffectManager = $EffectManager
	var audio_manager: AudioManager = get_node("/root/GlobalAudioManager")

	# 应用集中调参（摩擦/分离/流场/编组等，可在 game_tuning.tres 中修改）
	Tuning.apply_to(unit_manager, flow_field_manager, group_manager)
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	var debug_draw: Node2D = $DebugCanvas/DebugDraw
	var main_camera: Camera3D = $Camera3D
	
	var map_real_width = width * cell_size.x
	var map_real_height = height * cell_size.y
	var map_aspect_ratio = float(map_real_width) / float(map_real_height)
	
	$ProjectileManager.setup($UnitManager, $AttackManager)
	
	# 初始化 3D 渲染渲染
	map_manager.load_from_tilemap(tile_map_layer)
	tile_map_layer.hide() # 隐藏 2D，只看 MapManager 渲染的 3D
	
	# 初始化全局管理类
	GlobalGameManager.set_building_manager(building_manager)
	GlobalGameManager.set_unit_manager(unit_manager)
	GlobalGameManager.set_flow_field_manager(flow_field_manager)
	GlobalGameManager.set_selection_manager(selection_manager)
	GlobalGameManager.set_group_manager(group_manager)
	GlobalGameManager.set_attack_manager(attack_manager)
	GlobalGameManager.set_projectile_manager(projectile_manager)
	GlobalGameManager.set_economy_manager(economy_manager)
	GlobalGameManager.set_fog_manager(fog_manager)
	GlobalGameManager.set_weapon_manager(weapon_manager)
	GlobalGameManager.set_effect_manager(effect_manager)
	GlobalGameManager.set_audio_manager(audio_manager)
	
	flow_field_manager.setup_grid(width, height, grid_origin, cell_size)
	# --- 遍历 TileMap 填充元数据和代价地图 ---
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var coords: Vector2i = Vector2i(x, y)
			var data = tile_map_layer.get_cell_tile_data(coords)
			if not data: continue
			
			# 资源标记
			if data.get_custom_data("IsResource"):
				flow_field_manager.set_cell_meta_data(coords, 1, true)
			
			# 方向通行掩码（高地/悬崖，双向对称）
			# 掩码位 = 该格允许从哪些方向进入：UP=1, DOWN=2, LEFT=4, RIGHT=8
			# 例：南侧有坡口的高地 → DirMask=2(DIR_DOWN)，其余三面为崖壁不可通行
			# 默认15(全方向) = 普通地面
			# 注意：int 类型自定义数据未设置时返回 0（不是 null），0 也视为"未设置"→ 全方向
			var dir_mask_var = data.get_custom_data("DirMask")
			var dir_mask: int = 15
			if dir_mask_var != null and int(dir_mask_var) != 0:
				dir_mask = int(dir_mask_var)
			flow_field_manager.set_dir_mask(coords, dir_mask)
			
			# NAV_LAND (Index 0)
			if data.get_custom_data("IsWall") or data.get_custom_data("IsSea"):
				flow_field_manager.init_cost(coords, 255, 0)
			else:
				# 陆地基础权重统一为 1（近障碍检测半径来自 GameTuning，供避边逻辑使用）
				var is_near_obstacle = false
				var near_r: int = Tuning.near_obstacle_radius
				for dx in range(-near_r, near_r + 1):
					for dy in range(-near_r, near_r + 1):
						if Vector2i(dx, dy) == Vector2i.ZERO: continue
						var n_data = tile_map_layer.get_cell_tile_data(coords + Vector2i(dx, dy))
						if n_data == null or n_data.get_custom_data("IsWall") or n_data.get_custom_data("IsSea"):
							is_near_obstacle = true
							break
				flow_field_manager.init_cost(coords, 1 if is_near_obstacle else 1, 0)

			# NAV_SEA (Index 1)
			if not data.get_custom_data("IsSea"):
				flow_field_manager.init_cost(coords, 255, 1)
			else:
				var is_near_land = false
				for dx in range(-1, 2):
					for dy in range(-1, 2):
						if Vector2i(dx, dy) == Vector2i.ZERO: continue
						var n_data = tile_map_layer.get_cell_tile_data(coords + Vector2i(dx, dy))
						if n_data == null or not n_data.get_custom_data("IsSea"):
							is_near_land = true
							break
				flow_field_manager.init_cost(coords, 30 if is_near_land else 1, 1)
	
	# 调试：测试高地方向掩码（进入/离开场景）
	_test_dir_mask(tile_map_layer, flow_field_manager, cell_size)
	
	GlobalGameManager.setup_system(width, height, cell_size, grid_origin)
	
	# 注册配置（这部分建议放在 GlobalGameManager 的 init 里只运行一次）
	var dust_tex: Texture2D = ResourceLoader.load("res://asset/particle/dust.png")
	effect_manager.register_effect_type("Dust", dust_tex, 2000, -0.1, 0.9, 0.2)
	
	var water_foam_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_foam.png")
	effect_manager.register_effect_type("WaterFoam", water_foam_tex, 2000, 0.0, 0.85, 1.0)
	
	var water_splash_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_splash.png")
	effect_manager.register_effect_type("WaterSplash", water_splash_tex, 2000, -0.1, 0.98, 1.1)
	
	var water_ripple_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_ripple.png")
	effect_manager.register_effect_type("WaterRipple", water_ripple_tex, 2000, 0.0, 1.0, 0.3)
	
	var explosion_tex: Texture2D = ResourceLoader.load("res://asset/particle/explosion.png")
	effect_manager.register_effect_type("Explosion", explosion_tex, 1000, 0.0, 0.9, 1.0)
	
	var healing_tex: Texture2D = ResourceLoader.load("res://asset/particle/healing.png")
	effect_manager.register_effect_type("Healing", healing_tex, 1000, 9.8, 0.1, 1.2)
	
	# 炮口开火闪光 (像素风格程序化生成: 16px点阵量化 + 4级离散色阶 + 逐帧爆裂顿挫)
	# 参数依次为: 颜色, 像素大小, 调色板(0=火炮,1=机枪,2=重炮,3=能量), 白核, 火舌长, 侧喷度, 侧喷角(弧度), 侧喷长, 火花芒, 尺寸, 寿命, 前冲速度
	effect_manager.register_effect_type("MuzzleFlash", null, 500, 0.0, 0.85, 1.0, true, true)
	effect_manager.set_pixel_flash_params("MuzzleFlash", Color(1.0, 0.55, 0.10), 1.0, 0, 0.35, 0.90, 0.50, 0.87, 0.60, 0.40, 9.0, 0.12, 32.0)
	
	# 注册配置（本体 res://config + 玩家 mod user://mods，武器先行）
	_register_all_configs(weapon_manager, unit_manager, building_manager, projectile_manager)
	
	
	var players_settings: Dictionary = GlobalGameManager.get_all_player_settings()
	
	# 初始化玩家经济
	economy_manager.set_balance(2, 5000)
	
	var initialized_teams =[]
	for peer_id in players_settings.keys():
		var team_id = players_settings[peer_id]["team"]
		if not team_id in initialized_teams:
			economy_manager.set_balance(team_id, map_res.initial_gold)
			initialized_teams.append(team_id)
	
	# 设置本地视野相机
	var local_peer_id = multiplayer.get_unique_id()
	var local_spawn_id = 1
	
	# 获取自己选择的出生点
	if players_settings.has(local_peer_id):
		local_spawn_id = players_settings[local_peer_id]["spawn"]
	
	var camera_start_pos: Vector2 = Vector2.ZERO
	if spawn_positions.has(local_spawn_id):
		camera_start_pos = spawn_positions[local_spawn_id]
	else:
		# 如果超出了出生点数量，就把视野放在一号出生点 (如果有的话)
		print("本地出生点 ", local_spawn_id, " 无效，相机重置到 1 号出生点")
		if spawn_positions.has(1):
			camera_start_pos = spawn_positions[1]
		elif spawn_positions.size() > 0:
			# 如果连1号都没有，强制给第一个可用的
			camera_start_pos = spawn_positions.values()[0]

	main_camera.set_map_limits(used_rect, cell_size)
	main_camera.global_position.x = camera_start_pos.x
	main_camera.global_position.z = camera_start_pos.y
	
	# --- 初始化小地图 ---
	var max_minimap_ui_size = 256.0
	
	var map_min_vec = Vector2(used_rect.position * cell_size)
	var map_max_vec = Vector2(used_rect.end * cell_size)
	minimap_container.set_limits(map_min_vec, map_max_vec)
	
	if map_aspect_ratio > 1.0:
		# 横向较长的地图
		minimap_container.custom_minimum_size = Vector2(max_minimap_ui_size, max_minimap_ui_size / map_aspect_ratio)
	else:
		# 纵向较长或正方形的地图
		minimap_container.custom_minimum_size = Vector2(max_minimap_ui_size * map_aspect_ratio, max_minimap_ui_size)

	# 初始化小地图相机位置
	# 将相机定位在地图中心
	var map_center_world = Vector2(used_rect.position) * Vector2(cell_size) + (Vector2(used_rect.size) * Vector2(cell_size)) / 2.0
	minimap_camera.position.x = map_center_world.x
	minimap_camera.position.z = map_center_world.y
	
	# 正交相机的大小(size)代表垂直方向覆盖的单位距离
	# 如果我们要完美契合，需要根据比例设置
	minimap_camera.size = map_real_height
	# 如果地图特别宽，可能需要根据宽度来适配 size
	if map_aspect_ratio > (minimap_container.custom_minimum_size.x / minimap_container.custom_minimum_size.y):
		minimap_camera.size = map_real_width / (minimap_container.custom_minimum_size.x / minimap_container.custom_minimum_size.y)

	# --- 编队栏 UI (紧跟小地图下方) ---
	var control_group_bar: PanelContainer = preload("res://main/control_group_bar.gd").new()
	var right_sidebar: Container = minimap_border.get_parent()
	right_sidebar.add_child(control_group_bar)
	right_sidebar.move_child(control_group_bar, minimap_border.get_index() + 1)
	control_group_bar.selection_manager = selection_manager
	control_group_bar.unit_manager = unit_manager

	# 3. 生成初始单位 (仅由主机/服务器执行)
	if multiplayer.is_server():
		spawn_initial_units(spawn_positions, players_settings, cell_size)
		spawn_test_units()

	if debug_draw != null:
		debug_draw.unit_manager = unit_manager
		debug_draw.camera = main_camera
		debug_draw.flow_field_manager = flow_field_manager
	
	GlobalAudioManager.play_bgm("res://asset/audio/music/02_-_sam_goor00_collard_-_reign_supreme.ogg", -8.0, 0.5)
	fog_manager.fog_mode = GlobalGameManager.fog_mode
	GlobalGameManager.start_game()

# 按依赖顺序注册配置：武器先行（单位/建筑引用武器名），本体优先、mod 次之
func _register_all_configs(weapon_manager, unit_manager, building_manager, projectile_manager) -> void:
	for dir in ModManager.get_config_dirs("weapon"):
		weapon_manager.register_weapons_from_dir(dir)
	for dir in ModManager.get_config_dirs("unit"):
		unit_manager.register_units_from_dir(dir)
	for dir in ModManager.get_config_dirs("building"):
		building_manager.register_buildings_from_dir(dir)
	for dir in ModManager.get_config_dirs("projectile"):
		projectile_manager.register_projectiles_from_dir(dir)

func spawn_initial_units(spawn_positions: Dictionary, players_settings: Dictionary, cell_size: Vector2i):
	for peer_id in players_settings.keys():
		var p_info = players_settings[peer_id]
		var team_id = p_info["team"]
		var spawn_id = p_info["spawn"]
		
		# 只有玩家选择的 spawn_id 在地图里真实存在时，才为他生成单位
		if spawn_positions.has(spawn_id):
			var pos = spawn_positions[spawn_id]
			# 在基地旁边生成建筑
			GlobalGameManager.rpc_server_request_place_building("WarFactory", Vector2i(pos / Vector2(cell_size)), team_id, -1, true)
			# 测试用：网格化生成 50 辆 HeavyTank（10 列 x 5 行，间距 32px）
			var tank_cols := 10
			var tank_rows := 5
			var tank_spacing := 32.0
			var ffm: FlowFieldManager = $FlowFieldManager
			# 自动寻找出生点附近能容纳整块阵列的开阔锚点（避免卡进墙/海里）
			var anchor_offset: Vector2 = _find_tank_anchor(pos, tank_cols, tank_rows, tank_spacing, ffm)
			for r in tank_rows:
				for c in tank_cols:
					var tank_pos: Vector2 = pos + anchor_offset + Vector2(c * tank_spacing, r * tank_spacing)
					# 安全网：个别被挡住的坦克吸附到最近可走格
					tank_pos = _snap_to_walkable(tank_pos, cell_size, ffm)
					GlobalGameManager.rpc_server_request_spawn_unit("Fighter", tank_pos, team_id)
			# 基地附近生成工兵和防空
			GlobalGameManager.rpc_server_request_spawn_unit("Builder", pos + Vector2(0, 100), team_id)
			# 如果是陆地地图，生成防空
			GlobalGameManager.rpc_server_request_spawn_unit("Fighter", pos + Vector2(80, 100), team_id)
		else:
			print("Peer: ", peer_id, " (Team: ", team_id, ") 的出生点 ", spawn_id, " 无效，不生成初始单位。")

## 判断某个世界坐标是否可供陆地单位站立（非墙、非海；地图外视为可走）
func _tank_cell_walkable(world_pos: Vector2, ffm: FlowFieldManager) -> bool:
	var cell: Vector2i = ffm.world_to_grid(world_pos)
	var cost: float = ffm.get_cost(cell, 0) # 0 = NAV_LAND
	return cost < 255.0

## 在出生点附近从近到远搜索能放下整块坦克阵列的偏移量（步长 16px）
func _find_tank_anchor(base_pos: Vector2, cols: int, rows: int, spacing: float, ffm: FlowFieldManager) -> Vector2:
	for search_cells in [8, 16, 24, 32]:
		var half: int = search_cells * 16
		var best_offset := Vector2.ZERO
		var best_dist := INF
		for oy in range(-half, half + 1, 16):
			for ox in range(-half, half + 1, 16):
				var all_clear := true
				for r in rows:
					for c in cols:
						if not _tank_cell_walkable(base_pos + Vector2(ox + c * spacing, oy + r * spacing), ffm):
							all_clear = false
							break
					if not all_clear:
						break
				if all_clear:
					var d := float(ox * ox + oy * oy)
					if d < best_dist:
						best_dist = d
						best_offset = Vector2(ox, oy)
		if best_dist < INF:
			return best_offset
	return Vector2.ZERO

## 若位置卡在墙/海里，吸附到最近可走格的格心
func _snap_to_walkable(world_pos: Vector2, cell_size: Vector2i, ffm: FlowFieldManager) -> Vector2:
	if _tank_cell_walkable(world_pos, ffm):
		return world_pos
	var cell: Vector2i = ffm.world_to_grid(world_pos)
	var near_cell: Vector2i = ffm.find_nearest_walkable_cell(cell, 0)
	return Vector2(near_cell * cell_size) + Vector2(cell_size) * 0.5


func spawn_test_units():
	for x in range(5):
		for y in range(5):
			#GlobalGameManager.rpc_server_request_spawn_unit("Tank", -32 * Vector2(x, y), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("Fighter", -32 * Vector2(x, y), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("Battleship", -32 * Vector2(x, y) + Vector2(-7000, 0), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("TinyGunboat", -32 * Vector2(x, y) + Vector2(-7000, 0), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("HeavyTank", -32 * Vector2(x, y) + Vector2(0, 1600), 2)
			#GlobalGameManager.rpc_server_request_spawn_unit("Helicopter", -32 * Vector2(x, y) + Vector2(0, 1600), 2)
			continue


## 调试：测试高地方向掩码（进入/离开场景）
func _test_dir_mask(tile_map_layer: TileMapLayer, ffm: FlowFieldManager, cell_size: Vector2i) -> void:
	var found: Vector2i = Vector2i(-999999, -999999)
	var used_rect := tile_map_layer.get_used_rect()
	for x in range(used_rect.position.x, used_rect.end.x):
		for y in range(used_rect.position.y, used_rect.end.y):
			var data = tile_map_layer.get_cell_tile_data(Vector2i(x, y))
			if not data: continue
			var dm = data.get_custom_data("DirMask")
			if dm != null and int(dm) != 0 and int(dm) != 15:
				found = Vector2i(x, y)
				break
		if found.x != -999999: break
	
	if found.x == -999999:
		print("[TEST] 未找到高地")
		return
	
	var h = found
	var mask: int = ffm.get_dir_mask(h)
	print("[TEST] 高地 ", h, " 掩码=", mask, " U=", (mask & 1) != 0, " D=", (mask & 2) != 0, " L=", (mask & 4) != 0, " R=", (mask & 8) != 0)
	
	var cs := Vector2(cell_size)
	var center := Vector2(h) * cs + cs * 0.5
	# 各方向 1.5 格外的点（越过边界）
	var left := center - Vector2(cs.x * 1.5, 0)
	var right := center + Vector2(cs.x * 1.5, 0)
	var up := center - Vector2(0, cs.y * 1.5)
	var down := center + Vector2(0, cs.y * 1.5)
	
	# 进入高地（终点在高地中心，起点在四个方向）
	print("[TEST] 从左(崖壁?)进入高地: ", ffm.is_path_traversable(left, center, 0, 100000.0))
	print("[TEST] 从右(坡口?)进入高地: ", ffm.is_path_traversable(right, center, 0, 100000.0))
	print("[TEST] 从上(崖壁?)进入高地: ", ffm.is_path_traversable(up, center, 0, 100000.0))
	print("[TEST] 从下(坡口?)进入高地: ", ffm.is_path_traversable(down, center, 0, 100000.0))
	# 离开高地（起点在高地中心，终点在四个方向）
	print("[TEST] 从左离开高地: ", ffm.is_path_traversable(center, left, 0, 100000.0))
	print("[TEST] 从右离开高地: ", ffm.is_path_traversable(center, right, 0, 100000.0))
	print("[TEST] 从上离开高地: ", ffm.is_path_traversable(center, up, 0, 100000.0))
	print("[TEST] 从下离开高地: ", ffm.is_path_traversable(center, down, 0, 100000.0))
	
	# 测试流场方向：目标在高地右边，检查高地左边（崖壁）格子的流场方向
	var target := right + Vector2(cs.x * 2, 0)  # 高地右边更远
	ffm.create_flow_field(ffm.world_to_grid(target), 0, true)
	# 多次触发 get_flow_direction 确保流场入队，并等待足够帧数
	for i in range(120):
		ffm.get_flow_direction(left, target, 0)
		await get_tree().process_frame
	var left_grid := ffm.world_to_grid(left)
	print("[TEST] 高地左边格子 ", left_grid, " 的流场方向: ", ffm.get_flow_direction(left, target, 0))
	print("[TEST] 高地左边格子 ", left_grid, " 的 integration: ", ffm.get_integration(left, target, 0))
	var down_grid := ffm.world_to_grid(down)
	print("[TEST] 高地下边格子 ", down_grid, " 的流场方向: ", ffm.get_flow_direction(down, target, 0))
	print("[TEST] 高地下边格子 ", down_grid, " 的 integration: ", ffm.get_integration(down, target, 0))
