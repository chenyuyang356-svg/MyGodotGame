class_name ModManager
extends RefCounted

## 模组配置目录管理器。
##
## 游戏本体内容位于 res://config/<type>/（打进 PCK，优先级最高，最先注册）；
## 玩家 mod 位于 user://mods/<mod名>/config/<type>/（可写，后注册，可覆盖本体）。
## main.gd 通过 get_config_dirs() 拿到"本体 + 所有 mod"的配置目录列表，
## 按依赖顺序（武器 -> 单位/建筑 -> 投射物）依次注册。

static var _mod_dirs: PackedStringArray = PackedStringArray()
static var _scanned: bool = false

## 扫描 user://mods，缓存有效 mod 目录并加载其翻译。只执行一次。
static func _ensure_scanned() -> void:
	if _scanned:
		return
	_scanned = true

	var user_mods := "user://mods"
	if not DirAccess.dir_exists_absolute(user_mods):
		return

	var dir := DirAccess.open(user_mods)
	if dir == null:
		return

	dir.list_dir_begin()
	var name := dir.get_next()
	while name != "":
		if dir.current_is_dir() and not name.begins_with("."):
			var mod_dir := user_mods.path_join(name)
			# 只有包含 config 子目录的文件夹才视为有效 mod
			if DirAccess.dir_exists_absolute(mod_dir.path_join("config")):
				_mod_dirs.append(mod_dir)
				_load_mod_translations(mod_dir)
		name = dir.get_next()
	dir.list_dir_end()

## 返回某个类型的配置目录列表：本体 res://config/<type> 在前，各 mod 的 config/<type> 在后。
static func get_config_dirs(type: String) -> PackedStringArray:
	_ensure_scanned()
	var dirs := PackedStringArray(["res://config/%s" % type])
	for mod_dir in _mod_dirs:
		var d := mod_dir.path_join("config").path_join(type)
		if DirAccess.dir_exists_absolute(d):
			dirs.append(d)
	return dirs

## 已扫描到的 mod 目录列表（仅调试用）。
static func get_mod_dirs() -> PackedStringArray:
	_ensure_scanned()
	return _mod_dirs

## 加载 mod 自带翻译（仅支持预编译的 .translation 文件）。
static func _load_mod_translations(mod_dir: String) -> void:
	var t_dir := mod_dir.path_join("translation")
	if not DirAccess.dir_exists_absolute(t_dir):
		return

	var dir := DirAccess.open(t_dir)
	if dir == null:
		return

	dir.list_dir_begin()
	var f := dir.get_next()
	while f != "":
		if not dir.current_is_dir() and f.ends_with(".translation"):
			var res = load(t_dir.path_join(f))
			if res is Translation:
				TranslationServer.add_translation(res)
		f = dir.get_next()
	dir.list_dir_end()
