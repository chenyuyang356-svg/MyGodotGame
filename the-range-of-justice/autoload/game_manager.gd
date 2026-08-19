extends GameManager

var fog_mode = 2

func _on_start_game():
	print("game start")
	get_tree().root.find_child("UI_Master", true, false).start_game()
