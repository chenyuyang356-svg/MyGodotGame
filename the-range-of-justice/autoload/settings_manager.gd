extends Node

const SAVE_PATH = "user://settings.cfg" # user:// 路径在各平台都是可写的

# 默认设置
var settings = {
	"video": {
		"fullscreen": false
	},
	"audio": {
		"master": 0.8,
		"music": 0.8,
		"sfx": 0.8,
		"ui": 0.8
	}
}

func _ready():
	load_settings()

# 保存到文件
func save_settings():
	var config = ConfigFile.new()
	
	for section in settings.keys():
		for key in settings[section].keys():
			config.set_value(section, key, settings[section][key])
			
	config.save(SAVE_PATH)

# 从文件加载
func load_settings():
	var config = ConfigFile.new()
	var err = config.load(SAVE_PATH)
	
	if err != OK:
		apply_settings() # 如果没有文件，应用默认设置
		return

	for section in settings.keys():
		for key in settings[section].keys():
			settings[section][key] = config.get_value(section, key, settings[section][key])
	
	apply_settings()

# 将设置应用到引擎引擎（画面、声音）
func apply_settings():
	# 应用全屏
	var mode = Window.MODE_EXCLUSIVE_FULLSCREEN if settings.video.fullscreen else Window.MODE_WINDOWED
	DisplayServer.window_set_mode(mode)
	
	# 应用音量 (假设你的音频总线名称对应 master, music, sfx, ui)
	_set_bus_volume("Master", settings.audio.master)
	_set_bus_volume("Music", settings.audio.music)
	_set_bus_volume("SFX", settings.audio.sfx)
	_set_bus_volume("UI", settings.audio.ui)

func _set_bus_volume(bus_name: String, value: float):
	var bus_index = AudioServer.get_bus_index(bus_name)
	if bus_index != -1:
		# 将 0.0-1.0 的线性值转换为分贝
		AudioServer.set_bus_volume_db(bus_index, linear_to_db(value))
