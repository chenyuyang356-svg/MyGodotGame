extends AudioManager

func _ready() -> void:
	#音乐
	preload_sound("res://asset/audio/music/thetreadofwarmix.ogg")
	preload_sound("res://asset/audio/music/02_-_sam_goor00_collard_-_reign_supreme.ogg")
	#游戏音效
	register_sound("Explosion", "res://asset/audio/sfx/explosion01_refined.wav")
	register_sound("GunShot", "res://asset/audio/sfx/lmg_fire01.mp3")
	#UI音效
	register_sound("MenuClick", "res://asset/audio/ui/sound_click.wav")
	register_sound("InGameMenuClick", "res://asset/audio/ui/Menu Selection Click.wav")
	
	get_tree().node_added.connect(_on_node_added)

func _on_node_added(node):
	if node is Button and node.is_in_group("menu_button"):
		node.pressed.connect(func(): play_ui_sfx("MenuClick"))
