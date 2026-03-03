extends EconomyManager

@export var selection_manager: SelectionManager
@export var resource_label: Label

func _process(delta: float) -> void:
	resource_label.text = str(int(get_balance(selection_manager.team_id)))
