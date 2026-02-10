#目前只是为了方便测试而添加的
extends StaticBody2D

@export var SquareNumber: int

func _ready() -> void:
	var SquareScene: PackedScene = load("res://game_object/unit/square.tscn")
	for i in range(SquareNumber):
		var Square: Node2D = SquareScene.instantiate()
		Square.global_position = global_position + (16 + (i / 20) * 14) * Vector2.DOWN + (i % 20 - 14) * 12 * Vector2.RIGHT
		get_parent().add_child.call_deferred(Square)
