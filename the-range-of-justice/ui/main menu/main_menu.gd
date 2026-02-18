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


func _on_start_button_pressed():
	# 切换到你的 RTS 游戏主关卡
	# 这里使用了你之前尝试过的 get_tree()，但在函数内部是安全的
	if game_scene_path != "":
		get_tree().change_scene_to_file(game_scene_path)
	else:
		print("警告：未设置游戏主场景路径")

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
