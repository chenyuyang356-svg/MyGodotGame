extends Node2D

@onready var SelectingTimer: Timer = $Timer
var IsBoxSelecting: bool = false
var BoxSelectingStartPoint: Vector2
var BoxSelectingEndPoint: Vector2
var BoxSelectingRectangle: Rect2
var TypeGroups: Array = ["Square"]


func _ready() -> void:
	SelectingTimer.timeout.connect(_on_timeout)


func _input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_LEFT and event.double_click:
			var MousePosition: Vector2 = get_global_mouse_position()
			var SelectionAreas: Array = get_tree().get_nodes_in_group("SelectionArea")
			var OverlappingAreas: Array = GetOverlappingCircleAreas(MousePosition, SelectionAreas)
			var OverlappingIndexes: Array = GetIndexes(OverlappingAreas)
			#选择同类单位
			if !OverlappingIndexes.is_empty():
				var GroupName: StringName = GetTypeGroup(OverlappingAreas[0].get_groups())
				GameEvent.emit_signal("double_click_selecting", GroupName)
				print("Double click selecting!")
				return
		elif event.button_index == MOUSE_BUTTON_LEFT and event.pressed:
			BoxSelectingStartPoint = get_global_mouse_position()
			SelectingTimer.start()
		elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed:
			var MousePosition: Vector2 = get_global_mouse_position()
			var SelectionAreas: Array = get_tree().get_nodes_in_group("SelectionArea")
			var OverlappingAreas: Array = GetOverlappingCircleAreas(MousePosition, SelectionAreas)
			var OverlappingIndexes: Array = GetIndexes(OverlappingAreas)
			#选择新目标位置
			if OverlappingIndexes.is_empty():
				GameEvent.emit_signal("choosing_target_position", MousePosition)
				print("Choosing target position!")


func _process(delta: float) -> void:
	if IsBoxSelecting:
		BoxSelectingEndPoint = get_global_mouse_position()
		BoxSelectingRectangle = GetRect2(BoxSelectingStartPoint, BoxSelectingEndPoint)
		queue_redraw()
		var Indexes: Array = GetBoxSelectingIndexes(BoxSelectingRectangle)
		if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
			GameEvent.emit_signal("is_box_selecting", Indexes)
		#结束框选
		else:
			IsBoxSelecting = false
			GameEvent.emit_signal("end_box_selecting", Indexes)
			queue_redraw()
			print("End box selecting!")


func _draw() -> void:
	if IsBoxSelecting:
		draw_rect(BoxSelectingRectangle, Color.GREEN, false, 1, false)


func _on_timeout():
	#开始框选
	if Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT):
		BoxSelectingEndPoint = get_global_mouse_position()
		IsBoxSelecting = true
		print("Start box selecting!")
	#选择单个单位
	else:
		var MousePosition: Vector2 = get_global_mouse_position()
		var SelectionAreas: Array = get_tree().get_nodes_in_group("SelectionArea")
		var OverlappingAreas: Array = GetOverlappingCircleAreas(MousePosition, SelectionAreas)
		var OverlappingIndexes: Array = GetIndexes(OverlappingAreas)
		if !OverlappingAreas.is_empty():
			GameEvent.emit_signal("single_selecting", OverlappingIndexes[0])
			print("Single selecting!")


func GetOverlappingCircleAreas(Point: Vector2, CircleAreas: Array):
	var Result: Array
	for CircleArea: Area2D in CircleAreas:
		var shape: Shape2D = CircleArea.get_child(0).shape
		if shape is CircleShape2D:
			var Radius: float = shape.radius
			var CirclePosition: Vector2 = CircleArea.global_position
			if Point.distance_squared_to(CirclePosition) < Radius ** 2:
				Result.append(CircleArea.get_parent())
	return Result


func GetIndexes(Nodes: Array):
	var Result: Array
	for Element: Node in Nodes:
		Result.append(Element.get_index())
	
	return Result 


func GetTypeGroup(Groups: Array):
	for TypeGroup in TypeGroups:
		if TypeGroup in Groups:
			return TypeGroup


func GetAllIndexInGroup(GroupName: StringName):
	var Result: Array
	var GroupElements = get_tree().get_nodes_in_group(GroupName)
	for element: Node in GroupElements:
		Result.append(element.get_index())
	
	return Result


func GetRect2(FirstPoint: Vector2, SecondPoint: Vector2):
	var Position: Vector2 = FirstPoint.min(SecondPoint)
	var Size: Vector2 = (SecondPoint - FirstPoint).abs()
	return Rect2(Position, Size)


func GetBoxSelectingIndexes(Rectangle: Rect2):
	var Result: Array
	var SelectionAreas: Array = get_tree().get_nodes_in_group("SelectionArea")
	for Area: Area2D in SelectionAreas:
		if Rectangle.has_point(Area.global_position):
			Result.append(Area.get_parent().get_index())
	
	return Result
