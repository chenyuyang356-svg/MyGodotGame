extends FlowFieldManager

func init_cost(coords: Vector2i, cost: int, nav_type: int):
	set_init_cost(coords, cost, nav_type)
	set_cost(coords, cost, nav_type)
