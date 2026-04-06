extends Control

@onready var button_sandbox = %Button_Sandbox
@onready var button_back = %Button_Back

func _ready():
	button_back.pressed.connect(_on_btn_back_pressed)
	button_sandbox.pressed.connect(_on_btn_sandbox_pressed)

func _on_btn_sandbox_pressed():
	# 跳转到单人地图选择界面
	get_tree().change_scene_to_file("res://ui/single_player_lobby/single_player_lobby.tscn")

func _on_btn_back_pressed():
	get_tree().change_scene_to_file("res://ui/main_menu/main_menu.tscn")
