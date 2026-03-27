extends Control
@onready var fullscreen_check: CheckButton = $PanelContainer/MarginContainer/VBoxContainer/VBoxContainer2/HBoxContainer/CheckButton_Fullscreen
@onready var master_slider: HSlider = $PanelContainer/MarginContainer/VBoxContainer/VBoxContainer3/HBoxContainer/HSlider_Master
@onready var music_slider: HSlider = $PanelContainer/MarginContainer/VBoxContainer/VBoxContainer3/HBoxContainer2/HSlider_Music
@onready var sfx_slider: HSlider = $PanelContainer/MarginContainer/VBoxContainer/VBoxContainer3/HBoxContainer3/HSlider_SFX
@onready var ui_slider: HSlider = $PanelContainer/MarginContainer/VBoxContainer/VBoxContainer3/HBoxContainer4/HSlider_UI

func _ready():
	# 1. 初始化全屏按钮状态
	var current_mode = DisplayServer.window_get_mode()
	fullscreen_check.button_pressed = (current_mode == DisplayServer.WINDOW_MODE_FULLSCREEN or current_mode == DisplayServer.WINDOW_MODE_EXCLUSIVE_FULLSCREEN)
	
	# 2. 初始化音量滑条位置
	# 我们把滑条的 0.0-1.0 映射到 AudioServer 的分贝值
	_init_slider_volume("Master", master_slider)
	_init_slider_volume("Music", music_slider)
	_init_slider_volume("SFX", sfx_slider)
	_init_slider_volume("UI", ui_slider)
	
	# 3. 连接信号
	fullscreen_check.toggled.connect(_on_check_button_toggled)
	master_slider.value_changed.connect(_on_volume_slider_changed.bind("Master"))
	music_slider.value_changed.connect(_on_volume_slider_changed.bind("Music"))
	sfx_slider.value_changed.connect(_on_volume_slider_changed.bind("SFX"))
	ui_slider.value_changed.connect(_on_volume_slider_changed.bind("UI"))
		
		
# 当点击“返回”时，只需要隐藏自己，不需要切换场景
func _on_back_button_pressed():
	get_tree().change_scene_to_file("res://ui/main_menu/main_menu.tscn")
	

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

# 初始化滑条位置的辅助函数
func _init_slider_volume(bus_name: String, slider: HSlider):
	var bus_index = AudioServer.get_bus_index(bus_name)
	if bus_index != -1:
		# 将分贝(db)转回 0.0-1.0 的线性值
		var db_val = AudioServer.get_bus_volume_db(bus_index)
		slider.value = db_to_linear(db_val)

# 当滑条拖动时调用的函数
func _on_volume_slider_changed(value: float, bus_name: String):
	var bus_index = AudioServer.get_bus_index(bus_name)
	if bus_index != -1:
		# 将 0.0-1.0 的线性值转为分贝(db)
		# linear_to_db(0) 会变成 -80 (静音)
		AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))
		
		# 如果音量极低，直接静音该总线以节省资源
		AudioServer.set_bus_mute(bus_index, value < 0.01)
