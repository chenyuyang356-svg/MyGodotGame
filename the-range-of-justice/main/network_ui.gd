extends CanvasLayer

# 获取 UI 节点的引用
@onready var ip_input = $PanelContainer/VBoxContainer/IPInput
@onready var port_input = $PanelContainer/VBoxContainer/PortInput
@onready var status_label = $PanelContainer/VBoxContainer/StatusLabel

# 假设你的 GameManager 在主场景的这个路径下
# 如果你的层级结构不同，请修改这个路径
@onready var game_manager = get_node("/root/main/GameManager") 

func _ready():
	# 按钮点击事件连接
	$PanelContainer/VBoxContainer/HostButton.pressed.connect(_on_host_pressed)
	$PanelContainer/VBoxContainer/JoinButton.pressed.connect(_on_join_pressed)
	
	# 连接 Godot 内置的网络信号，用于显示连接状态
	multiplayer.connected_to_server.connect(_on_connected_ok)
	multiplayer.connection_failed.connect(_on_connected_fail)
	multiplayer.server_disconnected.connect(_on_server_offline)

func _on_host_pressed():
	var port = int(port_input.text)
	game_manager.host_game(port)
	status_label.text = "状态: 正在作为服务器运行..."
	# 创建房间后，UI 就可以隐藏了（或者等玩家满了再隐藏）
	# self.hide() 

func _on_join_pressed():
	var ip = ip_input.text
	var port = int(port_input.text)
	status_label.text = "状态: 正在尝试连接 %s..." % ip
	game_manager.join_game(ip, port)

# --- 网络状态回调 ---

func _on_connected_ok():
	status_label.text = "状态: 成功连接到服务器！"
	await get_tree().create_timer(1.0).timeout
	self.hide() # 连接成功后隐藏菜单

func _on_connected_fail():
	status_label.text = "状态: 连接失败，请检查 IP/端口"

func _on_server_offline():
	status_label.text = "状态: 服务器已断开"
	self.show() # 服务器断开后重新显示菜单
