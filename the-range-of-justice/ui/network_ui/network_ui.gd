extends CanvasLayer

# 获取 UI 节点的引用
@onready var ip_input = $PanelContainer/VBoxContainer/IPInput
@onready var port_input = $PanelContainer/VBoxContainer/PortInput
@onready var status_label = $PanelContainer/VBoxContainer/StatusLabel
@onready var start_game_button = $PanelContainer/VBoxContainer/StartGameButton
@onready var menu_buttons = $"../MainLayout/ContentVBox"

func _ready():
	# 按钮点击事件连接
	$PanelContainer/VBoxContainer/HostButton.pressed.connect(_on_host_pressed)
	$PanelContainer/VBoxContainer/JoinButton.pressed.connect(_on_join_pressed)
	
	# 连接 Godot 内置的网络信号，用于显示连接状态
	multiplayer.connected_to_server.connect(_on_connected_ok)
	multiplayer.connection_failed.connect(_on_connected_fail)
	multiplayer.server_disconnected.connect(_on_server_offline)
	
	start_game_button.hide()
	start_game_button.pressed.connect(_on_start_game_pressed)
	
	multiplayer.peer_connected.connect(_on_peer_changed)
	multiplayer.peer_disconnected.connect(_on_peer_changed)

func _on_host_pressed():
	var port = int(port_input.text)
	GlobalGameManager.host_game(port)
	status_label.text = "大厅已创建，等待玩家..."
	start_game_button.show() # 只有房主能看到开始按钮

func _on_join_pressed():
	var ip = ip_input.text
	var port = int(port_input.text)
	status_label.text = "状态: 正在尝试连接 %s..." % ip
	GlobalGameManager.join_game(ip, port)
	

func _on_peer_changed(_id):
	var peers = multiplayer.get_peers()
	status_label.text = "当前房间人数: %d" % (peers.size() + 1)

func _on_back_pressed():
	hide()
	menu_buttons.show()

func _on_start_game_pressed():
	# 主机点击开始
	GlobalGameManager.host_start_game()

# --- 网络状态回调 ---

func _on_connected_ok():
	status_label.text = "已加入大厅，等待房主开始游戏..."

func _on_connected_fail():
	status_label.text = "状态: 连接失败，请检查 IP/端口"

func _on_server_offline():
	status_label.text = "状态: 服务器已断开"
	self.show() # 服务器断开后重新显示菜单
