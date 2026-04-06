extends Control

# 使用 @export 允许在检查器中直接拖入场景文件，增加灵活性
# 对应你目录中的 res://main/main.tscn
@export_file("*.tscn") var game_scene_path: String = "res://main/main.tscn"
@onready var menu_buttons = $MainLayout/ContentVBox

func _ready():
	GlobalAudioManager.play_bgm("res://asset/audio/music/thetreadofwarmix.ogg", 0.0, 0.0)


func _on_single_player_pressed():
	get_parent().change_ui("res://ui/single_player_menu/single_player_menu.tscn")

func _on_multiplayer_pressed():
	get_parent().change_ui("res://ui/network_ui/network_ui.tscn")

func _on_options_button_pressed():
	get_parent().change_ui("res://ui/option_menu/option_menu.tscn")
	

func _on_exit_button_pressed():
	# 退出游戏
	get_tree().quit()
	
	
func _on_options_closed():
	# 1. 重新显示主菜单按钮
	$MainLayout/ContentVBox.show()
