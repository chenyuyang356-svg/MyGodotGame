extends Control

# 使用 @export 允许在检查器中直接拖入场景文件，增加灵活性
# 对应你目录中的 res://main/main.tscn
@export_file("*.tscn") var game_scene_path: String = "res://main/main.tscn"
@onready var menu_buttons = $MainLayout/ContentVBox
# 获取 Options 节点的引用
@onready var options_menu = $OptionsMenu 

func _ready():
	# 这一行是保险：不管编辑器里眼睛是睁开还是闭上，游戏启动时强制隐藏它
	options_menu.hide()
	options_menu.close_requested.connect(_on_options_closed)
	GlobalAudioManager.play_bgm("res://asset/audio/music/thetreadofwarmix.ogg", 0.0, 0.0)


func _on_single_player_pressed():
	# 单机模式：直接开启本地服务器并进入游戏
	GlobalGameManager.host_game(7777)
	GlobalGameManager.host_start_game()

func _on_multiplayer_pressed():
	get_tree().change_scene_to_file("res://ui/network_ui/network_ui.tscn")

func _on_options_button_pressed():
	# 这里未来可以用来打开设置面板
	print("点击了设置按钮")
	options_menu.show()
	menu_buttons.hide()
	

func _on_exit_button_pressed():
	# 退出游戏
	get_tree().quit()
	
	
func _on_options_closed():
	# 1. 重新显示主菜单按钮
	$MainLayout/ContentVBox.show()
