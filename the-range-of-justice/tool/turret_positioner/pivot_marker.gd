extends Node2D
## 枢轴十字标记（旋转中心参考）

func _draw() -> void:
	draw_line(Vector2(-10, 0), Vector2(10, 0), Color(1, 0.3, 0.3), 1.0)
	draw_line(Vector2(0, -10), Vector2(0, 10), Color(1, 0.3, 0.3), 1.0)
	draw_circle(Vector2.ZERO, 2.0, Color(1, 0.6, 0.6))
