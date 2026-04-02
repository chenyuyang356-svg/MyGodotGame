extends Control

# 获取 UI 引用 (根据你提供的 .tscn 结构)
@onready var check_fullscreen: CheckButton = %CheckButton_Fullscreen
@onready var debug_mode: CheckButton = %CheckButton_DebugMode
@onready var slider_master: HSlider = %HSlider_Master
@onready var slider_music: HSlider = %HSlider_Music
@onready var slider_sfx: HSlider = %HSlider_SFX
@onready var slider_ui: HSlider = %HSlider_UI

func _ready():
	# 1. 初始化 UI 显示为当前保存的设置
	check_fullscreen.button_pressed = SettingsManager.settings.video.fullscreen
	debug_mode.button_pressed = SettingsManager.settings.debug.is_dev_mode
	slider_master.value = SettingsManager.settings.audio.master
	slider_music.value = SettingsManager.settings.audio.music
	slider_sfx.value = SettingsManager.settings.audio.sfx
	slider_ui.value = SettingsManager.settings.audio.ui
	
	# 2. 连接信号
	check_fullscreen.toggled.connect(_on_fullscreen_toggled)
	debug_mode.toggled.connect(_on_debug_mode_toggled)
	slider_master.value_changed.connect(_on_audio_value_changed.bind("master"))
	slider_music.value_changed.connect(_on_audio_value_changed.bind("music"))
	slider_sfx.value_changed.connect(_on_audio_value_changed.bind("sfx"))
	slider_ui.value_changed.connect(_on_audio_value_changed.bind("ui"))

func _on_fullscreen_toggled(is_pressed: bool):
	SettingsManager.settings.video.fullscreen = is_pressed
	SettingsManager.apply_settings()

func _on_debug_mode_toggled(is_pressed: bool):
	SettingsManager.settings.debug.is_dev_mode = is_pressed
	SettingsManager.apply_settings()

func _on_audio_value_changed(value: float, bus_key: String):
	SettingsManager.settings.audio[bus_key] = value
	SettingsManager.apply_settings()

func _on_back_button_pressed():
	# 退出菜单时统一保存到磁盘
	SettingsManager.save_settings()
	# 这里写返回上一级菜单的代码，例如:
	get_tree().change_scene_to_file("res://ui/main_menu/main_menu.tscn")
