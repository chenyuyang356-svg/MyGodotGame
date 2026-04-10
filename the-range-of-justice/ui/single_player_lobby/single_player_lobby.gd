extends Control

@onready var map_option = %OptionButton_Map
@onready var fog_mode_option = %OptionButton_FogMode
@onready var spin_team = %SpinBox_Team
@onready var spin_spawn = %SpinBox_Spawn
@onready var preview_viewport = %SubViewport
@onready var preview_root = %PreviewRoot

var current_preview_map_idx: int = -1

func _ready():
	# 1. 填充文本翻译
	# 2. 初始化地图选项
	_populate_map_options()
	
	# 3. 设置默认本地玩家信息
	GlobalGameManager.local_player_name = "Player" # 或者从配置读取
	
	# 默认选择第一张地图
	if map_option.item_count > 0:
		_on_map_selected(0)
	_on_fog_mdoe_selected(2)

func _populate_map_options():
	map_option.clear()
	var maps: Array = GlobalGameManager.get_available_maps()
	for i in range(maps.size()):
		var map_res = maps[i]
		# 使用资源路径的文件名作为地图名，或者从资源内部读取变量
		var map_name = map_res.resource_path.get_file().get_basename().trim_suffix("_data") 
		map_name = map_name.replace("_", " ").capitalize()
		map_option.add_item(map_name, i)

func _on_map_selected(index: int):
	# 更新 C++ 中的全局地图索引（虽然是单人，但保持一致性）
	GlobalGameManager.set_selected_map_index(index)
	_update_map_preview(index)

func _on_fog_mdoe_selected(index: int):
	GlobalGameManager.fog_mode = index

func _on_btn_back_pressed():
	get_parent().change_ui("res://ui/single_player_menu/single_player_menu.tscn")

func _on_btn_start_pressed():
	# --- 核心逻辑：伪装成 Host 启动 ---
	
	# 1. 设置本地玩家在 C++ 中的配置
	# 由于是单人，我们只设置自己的 peer_id (通常本地是 1)
	var my_team = int(spin_team.value)
	var my_spawn = int(spin_spawn.value)
	
	# 2. 告诉 GlobalGameManager 启动单人游戏
	# 这里建议你在 C++ 的 GlobalGameManager 中加一个 start_single_player() 
	# 或者直接调用 host_game 并在之后立即关闭外部连接功能
	GlobalGameManager.host_game(0) # 端口 0 表示系统随机或不开放
	GlobalGameManager.rpc_id(1, "rpc_server_update_player_settings", my_team, my_spawn)
	GlobalGameManager.host_start_game()

# ================= 复用预览逻辑 =================
func _update_map_preview(map_idx: int):
	if map_idx == current_preview_map_idx: return
	current_preview_map_idx = map_idx
	
	for child in preview_root.get_children(): child.queue_free()
	
	var maps = GlobalGameManager.get_available_maps()
	var map_res = maps[map_idx]
	if not map_res or not map_res.map_scene: return
	
	var map_instance = map_res.map_scene.instantiate()
	map_instance.process_mode = Node.PROCESS_MODE_DISABLED 
	preview_root.add_child(map_instance)
	
	var tile_map_layer: TileMapLayer = _find_tile_map_layer(map_instance)
	var spawn_points_node = map_instance.get_node_or_null("SpawnPoints")
	
	var cell_size = Vector2(32, 32)
	if tile_map_layer:
		cell_size = tile_map_layer.tile_set.tile_size
		var used_rect = tile_map_layer.get_used_rect()
		var pixel_rect = Rect2(used_rect.position * Vector2i(cell_size), used_rect.size * Vector2i(cell_size))
		
		# 创建一个摄像机来观察全图
		var camera = Camera2D.new()
		preview_root.add_child(camera)
		camera.position = pixel_rect.position + pixel_rect.size / 2.0
		
		# 等待一帧，确保 SubViewport 布局完成拥有正确的 size
		await get_tree().process_frame
		
		# 自适应缩放 (让地图刚好填满视口)
		var vp_size = preview_viewport.size
		if vp_size.x > 0 and vp_size.y > 0 and pixel_rect.size.x > 0:
			var zoom_x = vp_size.x / pixel_rect.size.x
			var zoom_y = vp_size.y / pixel_rect.size.y
			# 取较小的缩放比例，并乘以 0.9 留出 10% 的边缘空白
			var min_zoom = min(zoom_x, zoom_y) * 0.9 
			camera.zoom = Vector2(min_zoom, min_zoom)
	
	# 4. 绘制醒目的出生点标识
	if spawn_points_node:
		for marker in spawn_points_node.get_children():
			if marker is Marker2D:
				_create_spawn_indicator(marker, cell_size)


# 递归寻找 TileMapLayer (防止它被套在其他节点下)
func _find_tile_map_layer(node: Node) -> TileMapLayer:
	if node is TileMapLayer: return node
	for child in node.get_children():
		var res = _find_tile_map_layer(child)
		if res: return res
	return null

# 在 Marker2D 位置生成一个漂亮的带编号的圆形徽章
func _create_spawn_indicator(marker: Marker2D, cell_size: Vector2):
	# 设定徽章大小 (假设横跨 4 个格子，在缩小的预览图中会非常醒目)
	var s_size = max(cell_size.x, cell_size.y) * 4.0 
	
	# 利用 StyleBoxFlat 动态生成一个纯色圆圈
	var style = StyleBoxFlat.new()
	style.bg_color = Color(0.1, 0.6, 1.0, 0.85) # 半透明科技蓝
	style.corner_radius_top_left = 100
	style.corner_radius_top_right = 100
	style.corner_radius_bottom_left = 100
	style.corner_radius_bottom_right = 100
	style.set_border_width_all(int(s_size * 0.1))
	style.border_color = Color.WHITE
	
	var panel = Panel.new()
	panel.add_theme_stylebox_override("panel", style)
	panel.custom_minimum_size = Vector2(s_size, s_size)
	# 让圆圈中心对准 Marker2D
	panel.position = -panel.custom_minimum_size / 2.0 
	
	# 编号文字
	var label = Label.new()
	label.text = marker.name
	label.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.add_theme_font_size_override("font_size", int(s_size * 0.6))
	label.add_theme_color_override("font_outline_color", Color.BLACK)
	label.add_theme_constant_override("outline_size", int(s_size * 0.15))
	
	panel.add_child(label)
	marker.add_child(panel)
