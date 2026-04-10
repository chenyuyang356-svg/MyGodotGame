extends GameManager

var fog_mode = 3

func _on_start_game():
	get_tree().root.find_child("UI_Master", true, false).start_game()
