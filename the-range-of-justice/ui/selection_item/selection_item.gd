extends PanelContainer

@onready var icon_rect = %Icon
@onready var name_label = %NameLabel
@onready var count_label = %CountLabel

func setup(p_name: String, p_count: int, p_icon: Texture2D):
	name_label.text = p_name
	count_label.text = "数量: %d" % p_count
	if p_icon:
		icon_rect.texture = p_icon
	else:
		# 如果没图标，可以给个默认图或者隐藏图标框
		icon_rect.visible = false
