extends Control

# UI 节点引用
@onready var panel_connection = $Panel_Connection
@onready var panel_lobby = $Panel_Lobby
@onready var ip_input = $Panel_Connection/VBoxContainer/HBoxContainer/LineEdit_IP
@onready var port_input = $Panel_Connection/VBoxContainer/HBoxContainer2/LineEdit_Port
@onready var name_input = $Panel_Connection/VBoxContainer/LineEdit_Name
@onready var btn_join = $Panel_Connection/VBoxContainer/Button_Join

@onready var map_option = $Panel_Lobby/VBoxContainer/HBoxContainer/OptionButton_Map
@onready var btn_start = $Panel_Lobby/VBoxContainer/Button_StartGame
@onready var btn_disconnect = $Panel_Lobby/VBoxContainer/Button_Disconnect
@onready var players_container = $Panel_Lobby/VBoxContainer/VBoxContainer_Players

@onready var spin_team = $Panel_Lobby/VBoxContainer/HBoxContainer2/SpinBox_Team
@onready var spin_spawn = $Panel_Lobby/VBoxContainer/HBoxContainer2/SpinBox_Spawn

@onready var preview_root = $Panel_Lobby/VBoxContainer/MapPreviewContainer/SubViewportContainer/SubViewport/PreviewRoot
@onready var preview_viewport = $Panel_Lobby/VBoxContainer/MapPreviewContainer/SubViewportContainer/SubViewport

var current_preview_map_idx: int = -1

func _ready():
	# 绑定 GlobalGameManager 的大厅刷新信号
	GlobalGameManager.lobby_updated.connect(_on_lobby_updated)
	GlobalGameManager.game_left.connect(_on_game_left)
	
	multiplayer.connected_to_server.connect(_on_connected_ok)
	
	# 初始化 UI
	panel_connection.show()
	panel_lobby.hide()
	
	# 监听本地设置变化，向服务器发送更新
	spin_team.value_changed.connect(_on_local_settings_changed)
	spin_spawn.value_changed.connect(_on_local_settings_changed)


# ================= 1. 连接阶段 =================

func _on_Button_Host_pressed():
	var port = int(port_input.text)
	
	var player_name = name_input.text.strip_edges()
	if player_name == "": player_name = "Host"
	GlobalGameManager.local_player_name = player_name
	
	GlobalGameManager.host_game(port)
	
	# 初始化主机UI状态
	enter_lobby(true)
	
	# 填充地图选项 (仅主机)
	populate_map_options()

func _on_Button_Join_pressed():
	var ip = ip_input.text
	var port = int(port_input.text)
	
	var player_name = name_input.text.strip_edges()
	if player_name == "": player_name = "Client"
	GlobalGameManager.local_player_name = player_name
	
	GlobalGameManager.join_game(ip, port)
	
	btn_join.disabled = true
	btn_join.text = "Connecting..."

func _on_Button_Back_pressed():
	get_tree().change_scene_to_file("res://ui/main menu/main_menu.tscn")

func _on_Button_Disconnect_pressed():
	# 通知 C++ 底层切断网络并清理数据，这会触发底层的 game_left 信号
	GlobalGameManager.leave_game()

func _on_connected_ok():
	# 此时才是真正连上了服务器，切入大厅界面
	enter_lobby(false)

func _on_game_left():
	# 切换 UI 面板
	panel_lobby.hide()
	panel_connection.show()
	
	btn_join.disabled = false
	btn_join.text = "Join"
	
	# 清理玩家列表缓存
	for child in players_container.get_children():
		child.queue_free()
		
	# 清理地图预览缓存
	for child in preview_root.get_children():
		child.queue_free()
	current_preview_map_idx = -1

func enter_lobby(is_host: bool):
	panel_connection.hide()
	panel_lobby.show()
	
	# 权限控制
	map_option.disabled = not is_host
	btn_start.visible = is_host
	_on_lobby_updated()

# ================= 2. 大厅同步阶段 =================

# 只有主机能修改地图
func _on_OptionButton_Map_item_selected(index: int):
	if multiplayer.is_server():
		# 调用 C++ 方法更新地图并广播
		GlobalGameManager.rpc_server_set_map(index)

# 玩家修改自己的队伍或出生点
func _on_local_settings_changed(_val):
	var my_team = int(spin_team.value)
	var my_spawn = int(spin_spawn.value)
	
	# 调用 C++ 设置更新的 RPC（ANY_PEER -> SERVER）
	# 由于你的 C++ 设置了 RPC_MODE_ANY_PEER，我们在 GDScript 使用 rpc 发送
	GlobalGameManager.rpc_id(1, "rpc_server_update_player_settings", my_team, my_spawn)

# 当 C++ 端收到 rpc_client_sync_lobby 并触发信号时执行
func _on_lobby_updated():
	# 1. 刷新地图显示
	# 注意：如果客户端刚进来还没获取过 map 列表，这里需要保护一下
	# C++ 中需要暴露 selected_map_index 的 get 方法，或者通过同步数据传过来。
	# 这里为了简便，假设地图顺序都一致。
	
	# 2. 清理旧的玩家列表
	for child in players_container.get_children():
		child.queue_free()
		
	# 3. 获取所有玩家数据 (调用 C++ 暴露的 get_all_player_settings)
	var all_players : Dictionary = GlobalGameManager.get_all_player_settings()
	
	for peer_id in all_players.keys():
		var p_data = all_players[peer_id]
		
		if peer_id == multiplayer.get_unique_id():
			spin_team.set_value_no_signal(p_data["team"])
			spin_spawn.set_value_no_signal(p_data["spawn"])
		
		# 创建一个简单的 Label 来显示玩家信息
		var p_label = Label.new()
		
		# 将名字格式化到 UI 显示中
		var display_text = "[%s] Team: %d | Spawn: %d" % [
			p_data["name"], 
			p_data["team"], 
			p_data["spawn"]
		]
		
		# 标记自己
		if peer_id == multiplayer.get_unique_id():
			display_text += " (You)"
			
		p_label.text = display_text
		players_container.add_child(p_label)
	
	# 获取当前地图索引，同步下拉框并更新预览图
	var current_map_idx = GlobalGameManager.get_selected_map_index()
	if map_option.selected != current_map_idx:
		map_option.select(current_map_idx) # 同步客机的下拉框显示
	
	update_map_preview(current_map_idx)

func populate_map_options():
	map_option.clear()
	var maps: Array = GlobalGameManager.get_available_maps()
	for i in range(maps.size()):
		var map_res = maps[i]
		# 使用资源路径的文件名作为地图名，或者从资源内部读取变量
		var map_name = map_res.resource_path.get_file().get_basename().trim_suffix("_data") 
		map_name = map_name.replace("_", " ").capitalize()
		map_option.add_item(map_name, i)

# ================= 3. 开始游戏 =================

func _on_Button_StartGame_pressed():
	if multiplayer.is_server():
		# C++ 中已封装好：整理数据并发送加载场景的 RPC
		GlobalGameManager.host_start_game()

# ================= 地图预览生成 =================

func update_map_preview(map_idx: int):
	# 防止重复加载同一张地图
	if map_idx == current_preview_map_idx:
		return
	current_preview_map_idx = map_idx
	
	# 1. 清理旧的预览地图
	for child in preview_root.get_children():
		child.queue_free()
		
	var maps: Array = GlobalGameManager.get_available_maps()
	if map_idx < 0 or map_idx >= maps.size():
		return
		
	var map_res = maps[map_idx]
	if not map_res or not map_res.map_scene:
		return
		
	# 2. 实例化地图放入子视口
	var map_instance = map_res.map_scene.instantiate()
	# 【关键】禁用预览地图的所有脚本逻辑和物理判定，使其纯粹作为静态画面
	map_instance.process_mode = Node.PROCESS_MODE_DISABLED 
	preview_root.add_child(map_instance)
	
	# 3. 寻找 TileMapLayer 和 SpawnPoints
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
