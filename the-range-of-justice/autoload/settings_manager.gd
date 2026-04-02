# settings_manager.gd
extends Node

const SAVE_PATH = "user://settings.cfg"

var settings = {
	"video": {
		"fullscreen": false
	},
	"audio": {
		"master": 0.8,
		"music": 0.8,
		"sfx": 0.8,
		"ui": 0.8
	},
	"debug": {
		"is_dev_mode": false # 新增：持久化开发者模式开关
	}
}

func _ready():
	load_settings()

func save_settings():
	var config = ConfigFile.new()
	for section in settings.keys():
		for key in settings[section].keys():
			config.set_value(section, key, settings[section][key])
	config.save(SAVE_PATH)

func load_settings():
	var config = ConfigFile.new()
	var err = config.load(SAVE_PATH)
	if err != OK:
		apply_settings()
		return

	for section in settings.keys():
		for key in settings[section].keys():
			settings[section][key] = config.get_value(section, key, settings[section][key])
	apply_settings()

func apply_settings():
	# 1. 应用全屏
	var mode = Window.MODE_EXCLUSIVE_FULLSCREEN if settings.video.fullscreen else Window.MODE_WINDOWED
	DisplayServer.window_set_mode(mode)
	
	# 2. 应用音量
	_set_bus_volume("Master", settings.audio.master)
	_set_bus_volume("Music", settings.audio.music)
	_set_bus_volume("SFX", settings.audio.sfx)
	_set_bus_volume("UI", settings.audio.ui)
	
	# 3. 开发者模式通知 (可选，用于调试)
	print("Settings Applied. Dev Mode: ", settings.debug.is_dev_mode)

func _set_bus_volume(bus_name: String, value: float):
	var bus_index = AudioServer.get_bus_index(bus_name)
	if bus_index != -1:
		AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))
