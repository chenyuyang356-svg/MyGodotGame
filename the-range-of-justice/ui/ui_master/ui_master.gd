extends Node

@onready var ui_container = $UI_Container
@onready var menu_background = $MenuBackground

func _ready():
	# 初始加载主菜单
	change_ui("res://ui/main_menu/main_menu.tscn")

func start_game():
	menu_background.hide()
	# 清除旧 UI
	for child in ui_container.get_children():
		child.queue_free()
	
	# 加载新 UI 并作为子节点添加
	var new_ui = load("res://main/main.tscn").instantiate()
	ui_container.add_child(new_ui)

func change_ui(scene_path: String):
	menu_background.show()
	# 清除旧 UI
	for child in ui_container.get_children():
		child.queue_free()
	
	# 加载新 UI 并作为子节点添加
	var new_ui = load(scene_path).instantiate()
	ui_container.add_child(new_ui)
