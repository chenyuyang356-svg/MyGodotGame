extends Node

var is_dev_mode: bool:
	get:
		return SettingsManager.settings.debug.is_dev_mode
