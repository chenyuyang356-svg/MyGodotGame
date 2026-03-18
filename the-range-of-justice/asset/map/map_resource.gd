class_name MapResource
extends Resource

@export var map_name: String = "新地图"
@export var map_description: String = ""
# 核心：该地图对应的 2D 场景文件，场景根节点应包含一个 TileMapLayer
@export var map_scene: PackedScene 
@export var max_player_count: int = 10
@export var initial_gold: int = 5000
