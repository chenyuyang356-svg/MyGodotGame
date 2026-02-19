extends Control
signal close_requested
@onready var fullscreen_check: CheckButton = $VBoxContainer/CheckButton
@onready var menu_buttons = $"../MainLayout/ContentVBox"
func _ready():
	
	# 初始化按钮状态：检查当前窗口是否已经是全屏
	var current_mode = DisplayServer.window_get_mode()
	if current_mode == DisplayServer.WINDOW_MODE_FULLSCREEN or current_mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN:
		fullscreen_check.button_pressed = true
	else:
		fullscreen_check.button_pressed = false
		
		
# 当点击“返回”时，只需要隐藏自己，不需要切换场景
func _on_back_button_pressed():
	hide()
	menu_buttons.show()
	

# 信号连接的函数
func _on_check_button_toggled(toggled_on: bool):
	if toggled_on:
		# 切换到全屏模式
		# WINDOW_MODE_FULLSCREEN 通常指“无边框全屏窗口”，Alt-Tab 切换更顺滑
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN)
		print("信号接收到了！正在尝试切换全屏...")

	else:
		# 切换回窗口模式
		DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_WINDOWED)
