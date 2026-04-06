extends Control

@onready var title_label = $PanelContainer/VBoxContainer/Label
@onready var continue_button = $PanelContainer/VBoxContainer/Button_Continue
@onready var lobby_button = $PanelContainer/VBoxContainer/Button_BackToLobby
@onready var disconnect_button = $PanelContainer/VBoxContainer/Button_Disconnect
@export var selection_manager: SelectionManager

signal change_ui(scene_path: String)

func _ready():
	hide()
	# 连接 C++ 发出的信号
	if GlobalGameManager.has_signal("game_finished"):
		GlobalGameManager.game_finished.connect(_on_game_finished)
	
	continue_button.pressed.connect(_on_button_continue_pressed)
	lobby_button.pressed.connect(_on_button_back_to_lobby_pressed)
	disconnect_button.pressed.connect(_on_button_disconnect_pressed)

func _on_game_finished(winner_team_id: int):
	show()
	Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE) # 确保鼠标可见
	
	# 获取本地玩家队伍 ID
	var my_team = selection_manager.get_team_id()
	
	if winner_team_id == my_team:
		title_label.text = "战 役 胜 利"
		title_label.modulate = Color.GOLD
	else:
		title_label.text = "全 军 覆 没"
		title_label.modulate = Color.RED
	
	# 只有满足以下两个条件，才显示“返回大厅”按钮：
	# 1. 我是服务器 (multiplayer.is_server())
	# 2. 房间里还有其他玩家 (multiplayer.get_peers().size() > 0)
	var is_multiplayer_session = multiplayer.get_peers().size() > 0
	
	if multiplayer.is_server() and is_multiplayer_session:
		lobby_button.visible = true
	else:
		lobby_button.visible = false

# --- 选项 1：留在战场 ---
func _on_button_continue_pressed():
	hide()
	# 注意：此时 C++ 已经停止了物理 tick，玩家只能观察不能操作

# --- 选项 2：回到大厅 (同步所有玩家) ---
func _on_button_back_to_lobby_pressed():
	if multiplayer.is_server():
		# 通知所有客户端切换回大厅场景
		rpc_back_to_lobby.rpc()

@rpc("authority", "call_local", "reliable")
func rpc_back_to_lobby():
	# 每个客户端清理本地游戏数据并切回大厅
	GlobalGameManager.reset_game_state()
	GlobalAudioManager.stop_bgm()
	emit_signal("change_ui", "res://ui/network_ui/network_ui.tscn")

# --- 选项 3：彻底退出 (断开连接) ---
func _on_button_disconnect_pressed():
	GlobalGameManager.leave_game() # 调用 C++ 的网络清理逻辑
	emit_signal("change_ui", "res://ui/main_menu/main_menu.tscn")
