extends Node

func _ready() -> void:
	var map_idx = GlobalGameManager.selected_map_index
	var map_res = GlobalGameManager.available_maps[map_idx]
	
	if map_res:
		setup_game_with_map(map_res)

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
	
	var cell_size: Vector2i = tile_map_layer.tile_set.tile_size
	var used_rect: Rect2i = tile_map_layer.get_used_rect()
	var width: int = used_rect.size.x
	var height: int = used_rect.size.y
	var grid_origin: Vector2i = used_rect.position
	var debug_draw: Node2D = $DebugCanvas/DebugDraw
	var main_camera: Camera3D = $Camera3D
	var minimap_viewport: SubViewport = $MinimapBorder/SubViewportContainer/SubViewport
	var minimap_container: SubViewportContainer = $MinimapBorder/SubViewportContainer
	var minimap_border: PanelContainer = $MinimapBorder
	var minimap_camera: Camera3D = $MinimapBorder/SubViewportContainer/SubViewport/MinimapCamera3D
	
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
			
			# NAV_LAND (Index 0)
			if data.get_custom_data("IsWall") or data.get_custom_data("IsSea"):
				flow_field_manager.init_cost(coords, 255, 0)
			else:
				# 基础权重 1，靠近障碍物（Wall/Sea）设为 30 实现避让边缘
				var is_near_obstacle = false
				for dx in range(-1, 2):
					for dy in range(-1, 2):
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
	
	# 设置流场系统尺寸
	GlobalGameManager.setup_system(width, height, cell_size, grid_origin)
	
	# 注册配置（这部分建议放在 GlobalGameManager 的 init 里只运行一次）
	var dust_tex: Texture2D = ResourceLoader.load("res://asset/particle/dust.png")
	effect_manager.register_effect_type("Dust", dust_tex, 2000, -1.8, 0.9, 0.2)
	
	var water_foam_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_foam.png")
	effect_manager.register_effect_type("WaterFoam", water_foam_tex, 2000, 0.0, 0.85, 1.0)
	
	var water_splash_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_splash.png")
	effect_manager.register_effect_type("WaterSplash", water_splash_tex, 2000, -9.8, 0.98, 1.1)
	
	var water_ripple_tex: Texture2D = ResourceLoader.load("res://asset/particle/water_effect/water_ripple.png")
	effect_manager.register_effect_type("WaterRipple", water_ripple_tex, 2000, 0.0, 1.0, 0.3)
	
	var explosion_tex: Texture2D = ResourceLoader.load("res://asset/particle/explosion.png")
	effect_manager.register_effect_type("Explosion", explosion_tex, 1000, 0.0, 0.9, 1.0)
	
	weapon_manager.register_weapons_from_dir("res://config/weapon/")
	unit_manager.register_units_from_dir("res://config/unit/")
	building_manager.register_buildings_from_dir("res://config/building/")
	projectile_manager.register_projectiles_from_dir("res://config/projectile/")
	
	
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
	var max_minimap_ui_size = 512.0
	
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
	
	# 3. 生成初始单位 (仅由主机/服务器执行)
	if multiplayer.is_server():
		spawn_initial_units(spawn_positions, players_settings)
		spawn_test_units()

	if debug_draw != null:
		debug_draw.unit_manager = unit_manager
		debug_draw.camera = main_camera
	
	GlobalAudioManager.play_bgm("res://asset/audio/music/02_-_sam_goor00_collard_-_reign_supreme.ogg", -8.0, 3.0)

func spawn_initial_units(spawn_positions: Dictionary, players_settings: Dictionary):
	for peer_id in players_settings.keys():
		var p_info = players_settings[peer_id]
		var team_id = p_info["team"]
		var spawn_id = p_info["spawn"]
		
		# 只有玩家选择的 spawn_id 在地图里真实存在时，才为他生成单位
		if spawn_positions.has(spawn_id):
			var pos = spawn_positions[spawn_id]
			# 在基地旁边生成坦克 (注意传递的是 team_id，让单位归属于该阵营)
			GlobalGameManager.rpc_server_request_spawn_unit("Tank", pos + Vector2(300, 400), team_id)
			GlobalGameManager.rpc_server_request_spawn_unit("Tank", pos + Vector2(0, 400), team_id)
			#GlobalGameManager.rpc_server_request_place_building("HumanBarrack", Vector2i(pos), team_id)
			
			# 如果是陆地地图，生成防空
			GlobalGameManager.rpc_server_request_spawn_unit("Fighter", pos + Vector2(150, 350), team_id)
		else:
			print("Peer: ", peer_id, " (Team: ", team_id, ") 的出生点 ", spawn_id, " 无效，不生成初始单位。")


func spawn_test_units():
	for x in range(10):
		for y in range(10):
			#GlobalGameManager.rpc_server_request_spawn_unit("AttackHelicopter", -32 * Vector2(x, y), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("HeavyTank", -32 * Vector2(x, y), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("Gunboat", -32 * Vector2(x, y) + Vector2(-7000, 0), 1)
			#GlobalGameManager.rpc_server_request_spawn_unit("Tank", -32 * Vector2(x, y) + Vector2(0, 1600), 2)
			#GlobalGameManager.rpc_server_request_spawn_unit("Fighter", -32 * Vector2(x, y) + Vector2(11000, 3500), 2)
			continue
