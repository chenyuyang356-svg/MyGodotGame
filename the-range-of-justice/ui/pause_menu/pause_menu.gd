extends Control

@onready var resume_button = $PanelContainer/VBoxContainer/Button_Continue
@onready var settings_button = $PanelContainer/VBoxContainer/Button_Settings
@onready var exit_button = $PanelContainer/VBoxContainer/Button_Disconnect

func _ready():
	hide()
	# 按钮连接
	resume_button.pressed.connect(_on_resume_pressed)
	settings_button.pressed.connect(_on_settings_pressed)
	exit_button.pressed.connect(_on_exit_pressed)

func _input(event):
	# 检测按下 Esc 键 (系统默认动作 ui_cancel)
	if event.is_action_pressed("ui_cancel"):
		if visible:
			_on_resume_pressed()
		else:
			_show_pause_menu()

func _show_pause_menu():
	show()
	Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	
	# 如果是单人游戏或服务器，可以尝试暂停逻辑
	# 注意：在多人游戏中暂停通常会导致同步问题，这里逻辑主要针对单人
	if multiplayer.get_peers().size() == 0:
		get_tree().paused = true

func _on_resume_pressed():
	hide()
	if multiplayer.get_peers().size() == 0:
		get_tree().paused = false
	# 如果你的游戏有特殊的鼠标锁定逻辑，可以在这里恢复
	# Input.set_mouse_mode(Input.MOUSE_MODE_CONFINED) 

func _on_settings_pressed():
	print("打开设置界面...")
	# 这里后续可以实例化你的设置面板

func _on_exit_pressed():
	# 确保退出前解除暂停状态，防止切回主界面后系统依然处于暂停中
	get_tree().paused = false
	
	# 复用你 GameOverUI 里的退出逻辑
	if GlobalGameManager.has_method("leave_game"):
		GlobalGameManager.leave_game()
	
	get_tree().change_scene_to_file("res://ui/main_menu/main_menu.tscn")
