extends Node
class_name HealthComponent

signal died
signal health_changed(current_health, max_health)

@export var max_health: float = 100.0
var current_health: float

func _ready() -> void:
	current_health = max_health

func take_damage(amount: float) -> void:
	if current_health <= 0:
		return
		
	current_health -= amount
	health_changed.emit(current_health, max_health)
	print("Unit took damage! Remaining: ", current_health)
	
	if current_health <= 0:
		die()

func die() -> void:
	died.emit()
	print("Unit died!")
