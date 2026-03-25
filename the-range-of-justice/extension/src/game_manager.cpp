#pragma once

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/dir_access.hpp>

#include "game_manager.h"

using namespace godot;

GameManager::GameManager() {
	// --- 通用 RPC 配置模板 ---
	Dictionary req_config; // 客户端发送请求给服务器：ANY_PEER, RELIABLE
	req_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	req_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	req_config["call_local"] = false;

	Dictionary sync_config; // 服务器同步给客户端：AUTHORITY, RELIABLE
	sync_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	sync_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	sync_config["call_local"] = false;

	// 绑定单位相关
	rpc_config("rpc_server_request_spawn_unit", req_config);
	rpc_config("rpc_client_spawn_unit", sync_config);
	rpc_config("rpc_client_despawn_unit", sync_config);

	// 绑定建筑相关
	rpc_config("rpc_server_request_place_building", req_config);
	rpc_config("rpc_client_spawn_building", sync_config);
	rpc_config("rpc_client_remove_building", sync_config);

	// 移动指令配置
	Dictionary move_config;
	move_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	move_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	move_config["call_local"] = false;
	rpc_config("rpc_server_receive_move", move_config);

	// 攻击指令配置
	Dictionary attack_config;
	attack_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	attack_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	attack_config["call_local"] = false;
	rpc_config("rpc_server_receive_attack_unit", attack_config);

	// 快照同步：服务器高频下发，允许丢包，保证最新顺序即可
	Dictionary snapshot_config;
	snapshot_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	snapshot_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED;
	snapshot_config["call_local"] = false;
	rpc_config("rpc_client_receive_snapshot", snapshot_config);

	// 生产与经济
	rpc_config("rpc_server_request_produce_unit", req_config);
	rpc_config("rpc_client_sync_resources", sync_config);

	// 场景加载
	Dictionary start_config;
	start_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	start_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	start_config["call_local"] = true; // 主机也要执行场景切换
	rpc_config("rpc_client_load_game", start_config);

	// 玩家注册
	Dictionary reg_config;
	reg_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	reg_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_config["call_local"] = false;
	rpc_config("rpc_server_request_registration", reg_config);

	Dictionary reg_sync_config;
	reg_sync_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	reg_sync_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_sync_config["call_local"] = true; // 确保服务器本地也更新 map
	rpc_config("rpc_client_on_player_registered", reg_sync_config);
	rpc_config("rpc_client_spawn_projectile", reg_sync_config);

	// 大厅设置同步 (any_peer -> server)
	Dictionary lobby_req;
	lobby_req["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	lobby_req["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	lobby_req["call_local"] = true;
	rpc_config("rpc_server_update_player_settings", lobby_req);
	rpc_config("rpc_server_set_map", lobby_req);

	// 大厅数据广播 (server -> all)
	Dictionary lobby_sync;
	lobby_sync["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	lobby_sync["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	lobby_sync["call_local"] = true;
	rpc_config("rpc_client_sync_lobby", lobby_sync);
}

GameManager::~GameManager() {}

// 游戏主循环
void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager || !is_setup) { return; }
	if (!is_setup) return;

	if (is_server_authority()) {
		tick_accumulator += p_delta;

		// 服务端更新物理和寻路逻辑
		unit_manager->physics_update(p_delta);

		building_manager->update(p_delta);

		while (tick_accumulator >= logic_tick_rate) {

			building_manager->prepare_interpolation_snapshot();

			broadcast_network_snapshot();

			for (int team_id = 1; team_id < 10; ++team_id) {
				sync_resources_to_client(team_id);
			}

			tick_accumulator -= logic_tick_rate;
		}
	}
	else {
		tick_accumulator += p_delta;
	}
}

// 渲染主循环
void GameManager::_process(double p_delta) {
	if (!is_setup) return;

	unit_manager->update(p_delta);

	update_group(p_delta);
	
	// alpha 表示当前距离下一次网络数据包到达时间的百分比
	// 如果由于网络抖动导致丢包，允许短暂地超量推测 (上限 1.2 倍)，防止画面卡顿
	float alpha = UtilityFunctions::clamp(tick_accumulator / logic_tick_rate, 0.0, 1.2);

	// 更新迷雾
	std::vector<Vector2> positions;
	std::vector<float> radii;

	for (const auto& unit : unit_manager->units) {
		if (unit.team_id == selection_manager->get_team_id()) {
			// 记录 3D 世界的 X 和 Z 坐标
			positions.push_back(Vector2(unit.position.x, unit.position.y));
			radii.push_back(unit.stats->sight_range);
		}
	}

	for (auto& pair : building_manager->buildings) {
		BuildingData& building = pair.second;
		if (building.team_id == selection_manager->get_team_id()) {
			positions.push_back(Vector2(building.grid_pos * flow_field_manager->get_cell_size()) +
				Vector2(building.stats->get_footprint() * flow_field_manager->get_cell_size()) / 2);
			radii.push_back(building.stats->sight_range);
		}
	}

	fog_manager->update_vision(positions, radii);

	// 驱动底层 MultiMesh 实例，让显卡去画出介于 prev 和 next 之间的平滑位置
	unit_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	building_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	weapon_manager->update_multimesh_buffer(p_delta, alpha, unit_manager, building_manager, selection_manager);

	effect_manager->update(p_delta);
}

void GameManager::update_group(double p_delta) {
	// 定期清理过期的编队
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->CLEANUP_INTERVAL) return;
	group_manager->cleanup_timer = 0.0;

	auto it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		if (it->second.moving_units_count <= 0) {
			for (int unit_id : it->second.unit_ids) {
				int index = unit_manager->get_unit_index_by_id(unit_id);
				if (index >= 0) {
					if (unit_manager->units[index].temp_group_id == it->first) {
						unit_manager->units[index].temp_group_id = -1;
					}
				}
			}
			it = group_manager->temp_groups.erase(it);
		}
		else {
			it->second.average_integration = 0;
			++it;
		}
	}

	// 更新average_integration
	std::vector<UnitData>& units = unit_manager->units;
	for (int unit_idx = 0; unit_idx < units.size(); ++unit_idx) {
		UnitData& unit = units[unit_idx];
		if (unit.temp_group_id != -1) {
			UnitGroup* temp_group = group_manager->get_temp_group(unit.temp_group_id);
			if (temp_group) {
				temp_group->average_integration +=
					flow_field_manager->get_integration(unit.position, unit.target_pos, unit.get_nav_type());
			}
		}
	}

	it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		UnitGroup& temp_group = it->second;
		if (temp_group.moving_units_count > 0) {
			temp_group.average_integration /= temp_group.moving_units_count;
		}
		++it;
	}
}

// 初始化
void GameManager::set_unit_manager(Node* p_node) {
	unit_manager = Object::cast_to<UnitManager>(p_node);

	if (unit_manager) {
		unit_manager->connect("despawn_unit_requested", Callable(this, "_on_despawn_unit_requested"));
	}
}

void GameManager::set_building_manager(Node* p_node) {
	building_manager = Object::cast_to<BuildingManager>(p_node);

	if (building_manager) {
		building_manager->connect("placement_requested", Callable(this, "_on_placement_requested"));
		building_manager->connect("spawn_unit_requested", Callable(this, "_on_spawn_unit_requested"));
		building_manager->connect("despawn_building_requested", Callable(this, "_on_despawn_building_requested"));
	}
}

void GameManager::set_flow_field_manager(Node* p_node) {
	flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void GameManager::set_selection_manager(Node* p_node) {
	selection_manager = Object::cast_to<SelectionManager>(p_node);

	if (selection_manager) {
		selection_manager->connect("move_requested", Callable(this, "_on_move_requested"));
		selection_manager->connect("attack_unit_requested", Callable(this, "_on_attack_unit_requested"));
		selection_manager->connect("attack_building_requested", Callable(this, "_on_attack_building_requested"));
		selection_manager->connect("unit_production_requested", Callable(this, "_on_unit_production_requested"));
	}
}

void GameManager::set_group_manager(Node* p_node) {
	group_manager = Object::cast_to<GroupManager>(p_node);
}

void godot::GameManager::set_economy_manager(Node* p_node) {
	economy_manager = Object::cast_to<EconomyManager>(p_node);
}

void godot::GameManager::set_attack_manager(Node* p_node) {
	attack_manager = Object::cast_to<AttackManager>(p_node);

	if (attack_manager) {
		attack_manager->connect("spawn_projectile_requested", Callable(this, "_on_spawn_projectile_requested"));
	}
}

void godot::GameManager::set_projectile_manager(Node* p_node) {
	projectile_manager = Object::cast_to<ProjectileManager>(p_node);
}

void godot::GameManager::set_fog_manager(Node* p_node) {
	fog_manager = Object::cast_to<FogManager>(p_node);
}

void GameManager::set_weapon_manager(Node* p_node) {
	weapon_manager = Object::cast_to<WeaponManager>(p_node);
}

void GameManager::set_effect_manager(Node* p_node) {
	effect_manager = Object::cast_to<EffectManager>(p_node);
}

void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_group_manager(group_manager);
	unit_manager->set_fog_manager(fog_manager);
	unit_manager->set_attack_manager(attack_manager);
	unit_manager->set_weapon_manager(weapon_manager);
	unit_manager->set_effect_manager(effect_manager);

	building_manager->set_flow_field_manager(flow_field_manager);
	building_manager->set_unit_manager(unit_manager);
	building_manager->set_economy_manager(economy_manager);
	building_manager->set_fog_manager(fog_manager);
	building_manager->set_weapon_manager(weapon_manager);

	selection_manager->set_team_id(players_settings[get_multiplayer()->get_unique_id()].team_id);

	attack_manager->set_building_manager(building_manager);
	attack_manager->set_projectile_manager(projectile_manager);

	effect_manager->setup(fog_manager);

	projectile_manager->set_effect_manager(effect_manager);

	Vector2 map_size = Vector2(p_width, p_height) * Vector2(p_cell_size);
	Vector2 map_pos = Vector2(p_origin) * Vector2(p_cell_size);
	Ref<Texture2D> brush = ResourceLoader::get_singleton()->load("res://asset/fog_brush.tres");
	fog_manager->setup_fog(map_pos, map_size, brush);

	unit_manager->setup_system(p_width, p_height, p_cell_size, p_origin);
	weapon_manager->setup_system(fog_manager);
	is_setup = true;
}

// 房间/联机会话管理
void GameManager::host_game(int p_port) {
	// 如果已有连接，先将其关闭释放
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}
	game_in_progress = false;
	players_settings.clear();

	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_server(p_port);
	if (err != OK) {
		UtilityFunctions::print("Failed to host server.");
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);

	//主机自己永远占据 Pear ID 1 和 Team 1
	register_player(1, 1, local_player_name);
	UtilityFunctions::print("Server started on port: ", p_port);
}

void GameManager::join_game(String p_address, int p_port) {
	// 如果已有连接，先将其关闭释放
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}
	game_in_progress = false;
	players_settings.clear();

	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_client(p_address, p_port);
	if (err != OK) {
		UtilityFunctions::print("Failed to connect to server.");
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);
	UtilityFunctions::print("Connecting to: ", p_address);
}

void GameManager::host_start_game() {
	if (!is_server_authority()) return;

	game_in_progress = true; // 标记游戏已经开始

	// 告诉底层网络：不再接受新连接！
	Ref<MultiplayerPeer> peer = get_multiplayer()->get_multiplayer_peer();
	if (peer.is_valid()) {
		peer->set_refuse_new_connections(true);
	}

	// 准备最终的配置数据包
	Dictionary final_configs;
	for (const auto& E : players_settings) {
		Dictionary d;
		d["team"] = E.value.team_id;
		d["spawn"] = E.value.spawn_id;
		final_configs[E.key] = d;
	}

	// 广播：加载地图索引为 X 的地图，并传入配置
	rpc("rpc_client_load_game", selected_map_index, final_configs);
}

void GameManager::register_player(int p_peer_id, int p_team_id, String p_name) {
	PlayerSettings& s = players_settings[p_peer_id];
	s.team_id = p_team_id;
	s.spawn_id = p_team_id; // 默认出生点跟队伍一致
	s.name = p_name;
	UtilityFunctions::print("Registered Peer: ", p_peer_id, " Name: ", p_name, " to Team: ", p_team_id);
}

void GameManager::load_available_maps() {
	available_maps.clear();
	Ref<DirAccess> dir = DirAccess::open("res://asset/map");
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String folder_name = dir->get_next();
		while (!folder_name.is_empty()) {
			// 忽略 "." 和 ".." 以及非文件夹
			if (dir->current_is_dir() && !folder_name.begins_with(".")) {
				// 拼接路径，例如: res://asset/map/map_01/map_01_data.tres
				String map_path = "res://asset/map/" + folder_name + "/" + folder_name + "_data.tres";

				if (ResourceLoader::get_singleton()->exists(map_path)) {
					Ref<Resource> map_res = ResourceLoader::get_singleton()->load(map_path);
					if (map_res.is_valid()) {
						available_maps.push_back(map_res);
						UtilityFunctions::print("Loaded map: ", map_path);
					}
				}
			}
			folder_name = dir->get_next();
		}
	}
	else {
		UtilityFunctions::print("Failed to open map directory.");
	}
}

void GameManager::rpc_server_set_map(int p_index) {
	if (!get_multiplayer()->is_server()) return;
	selected_map_index = p_index;

	// 广播当前状态
	Dictionary all_configs;
	// 将现有的 players_settings 转为 Dictionary 方便传输
	for (const auto& E : players_settings) {
		Dictionary d;
		d["team"] = E.value.team_id;
		d["spawn"] = E.value.spawn_id;
		d["name"] = E.value.name;
		all_configs[E.key] = d;
	}
	rpc("rpc_client_sync_lobby", selected_map_index, all_configs);
}

void GameManager::rpc_server_update_player_settings(int p_team, int p_spawn) {
	int sender_id = get_multiplayer()->get_remote_sender_id();

	PlayerSettings& s = players_settings[sender_id];
	s.team_id = p_team;
	s.spawn_id = p_spawn;

	// 更新后广播
	rpc_server_set_map(selected_map_index);
}

void GameManager::rpc_client_sync_lobby(int p_map_idx, Dictionary p_all_settings) {
	selected_map_index = p_map_idx;

	// 更新本地缓存
	Array keys = p_all_settings.keys();
	for (int i = 0; i < keys.size(); i++) {
		int peer_id = keys[i];
		Dictionary d = p_all_settings[peer_id];
		players_settings[peer_id].team_id = d["team"];
		players_settings[peer_id].spawn_id = d["spawn"];
		players_settings[peer_id].name = d["name"];
	}

	// 发出信号给 GDScript UI 刷新显示
	emit_signal("lobby_updated");
}

void GameManager::rpc_client_load_game(int p_map_idx, Dictionary p_player_configs) {
	selected_map_index = p_map_idx;
	// 更新本地配置
	Array keys = p_player_configs.keys();
	for (int i = 0; i < keys.size(); i++) {
		int peer_id = keys[i];
		Dictionary d = p_player_configs[peer_id];
		players_settings[peer_id].team_id = d["team"];
		players_settings[peer_id].spawn_id = d["spawn"];
	}
	local_spawn = players_settings[get_multiplayer()->get_unique_id()].spawn_id;

	// 获取地图资源路径
	if (selected_map_index >= available_maps.size()) return;
	Ref<Resource> map_res = available_maps[selected_map_index];

	String scene_path = "res://main/main.tscn"; // 你的主游戏主循环场景
	get_tree()->change_scene_to_file(scene_path);
}

void GameManager::rpc_server_request_registration(int p_team_id, String p_name) {
	if (!get_multiplayer()->is_server()) return;

	int sender_id = get_multiplayer()->get_remote_sender_id();
	int desired_team_id = p_team_id;

	// 如果没有指定队伍，则自动分配队伍
	if (desired_team_id == 0) {
		desired_team_id = players_settings.size() + 1;
	}
	register_player(sender_id, desired_team_id, p_name);

	rpc_server_set_map(selected_map_index);
}

void GameManager::rpc_client_on_player_registered(int p_peer_id, int p_team_id) {
	if (p_peer_id == get_multiplayer()->get_unique_id()) {
		if (selection_manager) {
			selection_manager->set_team_id(p_team_id);
		}
	}
}

// --- 服务器接收到 RPC 后的处理 ---

void GameManager::rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos) {
	if (!get_multiplayer()->is_server()) return;

	if (unit_manager) {
		unit_manager->command_units_to_move(p_ids, p_pos);
	}
}

void GameManager::rpc_server_receive_attack_unit(PackedInt32Array p_ids, int p_target_id) {
	if (!get_multiplayer()->is_server()) return;
	if (unit_manager) {
		unit_manager->command_units_to_attack_target(p_ids, p_target_id);
	}
}

// 核心状态同步
void GameManager::broadcast_network_snapshot() {
	if (!unit_manager || !building_manager) return;

	PackedByteArray data;
	int unit_count = (int)unit_manager->units.size();
	int bld_count = (int)building_manager->buildings.size();

	// 1. 预先计算单位部分需要的总内存大小
	int total_unit_bytes = 0;
	for (const auto& unit : unit_manager->units) {
		// 基础25字节 + 武器数量标记(1字节) + 每把武器的旋转角度(4字节 * 武器数)
		total_unit_bytes += 25 + 1 + unit.weapons.size() * 4;
	}

	// 2. 计算建筑所需的序列化内存
	int total_bld_bytes = 0;
	for (const auto& pair : building_manager->buildings) {
		// 建筑基础 9 字节 + 武器数 1 字节 + (每个武器 4 字节)
		total_bld_bytes += 13 + 1 + pair.second.weapons.size() * 4;
	}

	// 重新调整缓冲区大小
	data.resize(4 + total_unit_bytes + 4 + total_bld_bytes);

	int offset = 0;
	// 2. 写入单位
	data.encode_s32(offset, unit_count);
	offset += 4;
	for (const auto& unit : unit_manager->units) {
		data.encode_s32(offset, unit.id);
		data.encode_float(offset + 4, unit.position.x);
		data.encode_float(offset + 8, unit.position.y);
		data.encode_float(offset + 12, unit.rotation);
		data.encode_float(offset + 16, unit.height);
		data.encode_float(offset + 20, unit.current_health);
		data.set(offset + 24, (uint8_t)unit.state);
		offset += 25;

		// --- 写入武器的朝向同步数据 ---
		uint8_t weapon_count = (uint8_t)unit.weapons.size();
		data.set(offset, weapon_count);
		offset += 1;

		for (const auto& weapon : unit.weapons) {
			data.encode_float(offset, weapon.rotation);
			offset += 4;
		}
	}

	// 3. 写入建筑
	data.encode_s32(offset, bld_count);
	offset += 4;
	for (const auto& pair : building_manager->buildings) {
		const BuildingData& b = pair.second;
		data.encode_s32(offset, b.id);              // 4 bytes
		data.encode_float(offset + 4, b.current_health); // 4 bytes
		data.set(offset + 8, (uint8_t)b.state);      // 1 byte
		data.encode_float(offset + 9, b.next_progress_percent); // 占 4 字节
		offset += 13;

		// 【新增】写入该建筑挂载的武器角度
		uint8_t weapon_count = (uint8_t)b.weapons.size();
		data.set(offset, weapon_count);
		offset += 1;

		for (const auto& weapon : b.weapons) {
			data.encode_float(offset, weapon.rotation);
			offset += 4;
		}
	}

	rpc("rpc_client_receive_snapshot", data);
}

// 客户端解包
void GameManager::rpc_client_receive_snapshot(const PackedByteArray& p_raw_data) {
	if (!unit_manager || p_raw_data.size() < 4) return;

	int offset = 0;

	// 1. 解析单位
	int unit_count = p_raw_data.decode_s32(offset);
	offset += 4;

	for (int i = 0; i < unit_count; i++) {
		if (offset + 25 > p_raw_data.size()) break; // 基础长度边界检查

		int id = p_raw_data.decode_s32(offset);
		float px = p_raw_data.decode_float(offset + 4);
		float py = p_raw_data.decode_float(offset + 8);
		float rot = p_raw_data.decode_float(offset + 12);
		float height = p_raw_data.decode_float(offset + 16);
		float health = p_raw_data.decode_float(offset + 20);
		uint8_t state = p_raw_data.get(offset + 24);
		offset += 25;

		// --- 读取武器的朝向同步数据 ---
		if (offset + 1 > p_raw_data.size()) break;
		uint8_t weapon_count = p_raw_data.get(offset);
		offset += 1;

		std::vector<float> weapon_rotations;
		weapon_rotations.reserve(weapon_count);
		for (int w = 0; w < weapon_count; ++w) {
			if (offset + 4 > p_raw_data.size()) break;
			weapon_rotations.push_back(p_raw_data.decode_float(offset));
			offset += 4;
		}

		// --- 应用数据 ---
		int idx = unit_manager->get_unit_index_by_id(id);
		if (idx != -1) {
			UnitData& unit = unit_manager->units[idx];

			unit.prev_position = unit.next_position;
			unit.prev_rotation = unit.next_rotation;
			unit.prev_height = unit.next_height;

			// 设置新的单位目标
			unit.position = Vector2(px, py);
			unit.next_position = Vector2(px, py);
			unit.next_rotation = rot;
			unit.next_height = height;
			unit.current_health = health;
			unit.state = (UnitState)state;

			// 设置新的武器目标
			int sync_count = Math::min((int)weapon_rotations.size(), (int)unit.weapons.size());
			for (int w = 0; w < sync_count; ++w) {
				WeaponData& weapon = unit.weapons[w];

				// 同步物理帧和逻辑帧差值逻辑
				weapon.prev_rotation = weapon.next_rotation;
				weapon.rotation = weapon_rotations[w];
				weapon.next_rotation = weapon_rotations[w];
			}
		}
	}

	// 2. 解析建筑 
	if (offset + 4 <= p_raw_data.size()) {
		int bld_count = p_raw_data.decode_s32(offset);
		offset += 4;

		for (int i = 0; i < bld_count; i++) {
			if (offset + 9 > p_raw_data.size()) break;

			int id = p_raw_data.decode_s32(offset);
			float health = p_raw_data.decode_float(offset + 4);
			uint8_t state = p_raw_data.get(offset + 8);
			float server_prog = p_raw_data.decode_float(offset + 9); // 获取百分比
			offset += 13;

			// 【新增】解析发过来的武器同步角度
			if (offset + 1 > p_raw_data.size()) break;
			uint8_t weapon_count = p_raw_data.get(offset);
			offset += 1;

			std::vector<float> weapon_rotations;
			weapon_rotations.reserve(weapon_count);
			for (int w = 0; w < weapon_count; ++w) {
				if (offset + 4 > p_raw_data.size()) break;
				weapon_rotations.push_back(p_raw_data.decode_float(offset));
				offset += 4;
			}

			if (building_manager->buildings.count(id)) {
				BuildingData& b = building_manager->buildings[id];
				b.current_health = health;
				b.state = (BuildingState)state;
				b.prev_progress_percent = b.next_progress_percent;
				b.next_progress_percent = server_prog;

				// 【新增】应用武器插值数据
				int sync_count = Math::min((int)weapon_rotations.size(), (int)b.weapons.size());
				for (int w = 0; w < sync_count; ++w) {
					WeaponData& weapon = b.weapons[w];
					weapon.prev_rotation = weapon.next_rotation;
					weapon.rotation = weapon_rotations[w];
					weapon.next_rotation = weapon_rotations[w];
				}
			}
		}
	}

	tick_accumulator = 0.0;
}

// 实体生成与管理
// 1. 单位生成与销毁
void GameManager::rpc_server_request_spawn_unit(String p_type, Vector2 p_pos, int p_team) {
	if (!is_server_authority()) return;
	// 服务器生成单位，获取 ID
	int new_id = unit_manager->spawn_unit_by_type(p_type, p_pos, p_team);
	if (new_id != -1) {
		rpc("rpc_client_spawn_unit", new_id, p_type, p_pos, p_team);
	}
}

void GameManager::rpc_client_spawn_unit(int p_id, String p_type, Vector2 p_pos, int p_team) {
	unit_manager->spawn_unit_by_type(p_type, p_pos, p_team, p_id);
}

// 2. 建筑生成与销毁
void GameManager::rpc_server_request_place_building(String p_type, Vector2i p_grid_pos, int p_team) {
	if (!is_server_authority()) return;

	Ref<BuildingStats> stats = building_manager->get_building_stats_by_type(p_type);
	if (stats.is_null()) return;

	// 1. 先检查钱够不够并扣费
	if (economy_manager->try_spend(p_team, stats->get_cost())) {
		int new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team);
		if (new_id != -1) {
			rpc("rpc_client_spawn_building", new_id, p_type, p_grid_pos, p_team);
			sync_resources_to_client(p_team); // 同步新余预1�7
		}
	}
	else {
	}

}

void GameManager::rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team) {
	if (is_server_authority()) return;
	building_manager->place_building_by_type(p_type, p_grid_pos, p_team, p_id);
}

void GameManager::rpc_client_despawn_unit(int p_id) {
	selection_manager->on_unit_despawned(p_id);
}

void GameManager::rpc_client_remove_building(int p_id) {
	building_manager->remove_building(p_id, selection_manager);
}

void GameManager::rpc_client_spawn_projectile(
	const String& p_type_name,
	Vector2 p_start_pos, float p_start_height,
	int p_target_id, bool p_target_is_building, float p_target_height,
	int p_source_id, bool p_source_is_building,
	float p_weapon_damage)
{
	if (projectile_manager) {
		projectile_manager->spawn_projectile(
			p_type_name, p_start_pos, p_start_height,
			p_target_id, p_target_is_building, p_target_height,
			p_source_id, p_source_is_building, p_weapon_damage
		);
	}
}

void GameManager::rpc_server_request_produce_unit(int p_building_id, String p_unit_type) {
	if (!get_multiplayer()->is_server()) return;
	int sender_id = get_multiplayer()->get_remote_sender_id();
	int sender_team;
	if (sender_id <= 1) {
		// 说明是主机（Server）自己在操作
		sender_team = selection_manager->get_team_id();
	}
	else {
		// 使用 players_settings 检查
		if (players_settings.has(sender_id)) {
			sender_team = players_settings[sender_id].team_id;
		}
		else {
			return; // 未识别的玩家
		}
	}
	// 防止玩家 A 发送 RPC 操控玩家 B 的兵营生产
	if (building_manager->get_building_team_id(p_building_id) != sender_team) {
		UtilityFunctions::print("Peer ", sender_id, " (Team ", sender_team,
			") tried to use building ", p_building_id,
			" belonging to Team ", building_manager->get_building_team_id(p_building_id));
		return;
	}
	// 经济校验
	Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(p_unit_type);
	if (economy_manager->try_spend(sender_team, u_stats->get_cost())) {
		building_manager->add_unit_to_production_queue(p_building_id, p_unit_type);
		sync_resources_to_client(sender_team);
	}
}

// 经济系统同步
void GameManager::rpc_client_sync_resources(int p_team_id, double p_amount) {
	// 只有当同步的是玩家自己的队伍时才更新本地缓存
	if (p_team_id == selection_manager->get_team_id()) {
		economy_manager->set_balance(p_team_id, p_amount);

		// 发信号给 GDScript UI
		// emit_signal("resources_updated", p_amount);
	}
}

void GameManager::sync_resources_to_client(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	double current_gold = economy_manager->get_balance(p_team_id);

	rpc("rpc_client_sync_resources", p_team_id, current_gold);
}

// 本地事件监听
void GameManager::_on_move_requested(PackedInt32Array p_ids, Vector2 p_pos) {
	if (get_multiplayer()->is_server()) {
		if (unit_manager) unit_manager->command_units_to_move(p_ids, p_pos);
	}
	else {
		rpc_id(1, "rpc_server_receive_move", p_ids, p_pos);
	}
}

void GameManager::_on_attack_unit_requested(PackedInt32Array p_ids, int p_target_id) {
	if (get_multiplayer()->is_server()) {
		if (unit_manager) unit_manager->command_units_to_attack_target(p_ids, p_target_id);
	}
	else {
		rpc_id(1, "rpc_server_receive_attack_unit", p_ids, p_target_id);
	}
}

void GameManager::_on_attack_building_requested(PackedInt32Array p_ids, int p_target_id) {
	if (unit_manager) {
		// 同样调用攻击指令
		//unit_manager->command_units_to_attack_target(p_ids, p_target_id);
	}
}

void GameManager::_on_placement_requested(String p_type_name, Vector2i p_grid_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器（比如单机测试或主机），直接进入处理逻辑
		rpc_server_request_place_building(p_type_name, p_grid_pos, p_team_id);
	}
	else {
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_grid_pos, p_team_id);
	}
}

void godot::GameManager::_on_spawn_unit_requested(String p_type_name, Vector2 p_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器（比如单机测试或主机），直接进入处理逻辑
		rpc_server_request_spawn_unit(p_type_name, p_pos, p_team_id);
	}
	else {
		rpc_id(1, "rpc_server_request_spawn_unit", p_type_name, p_pos, p_team_id);
	}
}

void godot::GameManager::_on_despawn_unit_requested(int p_unit_id) {
	unit_manager->despawn_unit(p_unit_id, selection_manager);
}

void GameManager::_on_despawn_building_requested(int p_bid) {
	building_manager->remove_building(p_bid, selection_manager);
}

void GameManager::_on_spawn_projectile_requested(const String& p_type_name, Vector2 p_start_pos, float p_start_height, int p_target_id, bool p_target_is_building, float p_target_height, int p_source_id, bool p_source_is_building, float p_weapon_damage) {
	if (!get_multiplayer()->is_server()) { return; }
	rpc("rpc_client_spawn_projectile",
		p_type_name, p_start_pos, p_start_height,
		p_target_id, p_target_is_building, p_target_height,
		p_source_id, p_source_is_building, p_weapon_damage);
}

void GameManager::_on_unit_production_requested(int p_bid, String p_type) {
	if (get_multiplayer()->is_server()) {
		rpc_server_request_produce_unit(p_bid, p_type);
	}
	else {
		rpc_id(1, "rpc_server_request_produce_unit", p_bid, p_type);
	}
}

void GameManager::_enter_tree() {
	// 连接信号
	get_multiplayer()->connect("peer_connected", Callable(this, "_on_peer_connected"));
	get_multiplayer()->connect("connected_to_server", Callable(this, "_on_connected_to_server"));

	get_multiplayer()->connect("peer_disconnected", Callable(this, "_on_peer_disconnected"));
	get_multiplayer()->connect("server_disconnected", Callable(this, "_on_server_disconnected"));
	get_multiplayer()->connect("connection_failed", Callable(this, "_on_connection_failed"));
}

// --- 服务器端触发 ---
void GameManager::_on_peer_connected(int p_id) {
	if (!get_multiplayer()->is_server()) return;
	UtilityFunctions::print("Server: Peer connected: ", p_id);
}

// --- 客户端触发 ---
void GameManager::_on_connected_to_server() {
	int my_id = get_multiplayer()->get_unique_id();
	UtilityFunctions::print("Client: Connected to server. My ID: ", my_id);

	int desired_team = 0;
	rpc_id(1, "rpc_server_request_registration", desired_team, local_player_name);
}

void GameManager::leave_game() {
	// 释放网络层
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}

	players_settings.clear();
	game_in_progress = false;
	is_setup = false;

	UtilityFunctions::print("Left game and disconnected network.");

	// 发出自定义断开信号，供 GDScript 侧响应（如清理场景或切回主菜单）
	emit_signal("game_left");
}

void GameManager::_on_peer_disconnected(int p_id) {
	if (get_multiplayer()->is_server()) {
		UtilityFunctions::print("Server: Peer disconnected: ", p_id);
		players_settings.erase(p_id);

		// 如果还没开始游戏，则刷新大厅信息广播给其它玩家
		if (!game_in_progress) {
			rpc_server_set_map(selected_map_index);
		}
	}
	else {
		UtilityFunctions::print("Client: Peer disconnected: ", p_id);
	}
}

void GameManager::_on_server_disconnected() {
	UtilityFunctions::print("Client: Server disconnected. Leaving game...");
	// 房主断线，客户端自动退出清理
	leave_game();
}

void GameManager::_on_connection_failed() {
	UtilityFunctions::print("Client: Connection failed or rejected (Game already in progress).");
	// 如果连接由于在游戏中被拒绝（或超时），则清理退出
	leave_game();
}

// godot绑定
void GameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &GameManager::set_unit_manager);
	ClassDB::bind_method(D_METHOD("set_building_manager", "node"), &GameManager::set_building_manager);
	ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &GameManager::set_flow_field_manager);
	ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &GameManager::set_selection_manager);
	ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &GameManager::set_group_manager);
	ClassDB::bind_method(D_METHOD("set_economy_manager", "node"), &GameManager::set_economy_manager);
	ClassDB::bind_method(D_METHOD("set_attack_manager", "node"), &GameManager::set_attack_manager);
	ClassDB::bind_method(D_METHOD("set_projectile_manager", "node"), &GameManager::set_projectile_manager);
	ClassDB::bind_method(D_METHOD("set_fog_manager", "node"), &GameManager::set_fog_manager);
	ClassDB::bind_method(D_METHOD("set_weapon_manager", "node"), &GameManager::set_weapon_manager);
	ClassDB::bind_method(D_METHOD("set_effect_manager", "node"), &GameManager::set_effect_manager);
	ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &GameManager::setup_system);

	ClassDB::bind_method(D_METHOD("host_game", "port"), &GameManager::host_game);
	ClassDB::bind_method(D_METHOD("join_game", "address", "port"), &GameManager::join_game);

	ClassDB::bind_method(D_METHOD("host_start_game"), &GameManager::host_start_game);
	ClassDB::bind_method(D_METHOD("rpc_server_set_map", "index"), &GameManager::rpc_server_set_map);
	ClassDB::bind_method(D_METHOD("rpc_server_update_player_settings", "team", "spawn"), &GameManager::rpc_server_update_player_settings);
	ClassDB::bind_method(D_METHOD("rpc_client_sync_lobby", "map_idx", "settings"), &GameManager::rpc_client_sync_lobby);
	ClassDB::bind_method(D_METHOD("rpc_client_load_game", "map_idx", "configs"), &GameManager::rpc_client_load_game);
	ClassDB::bind_method(D_METHOD("rpc_server_request_registration", "team_id", "player_name"), &GameManager::rpc_server_request_registration);
	ClassDB::bind_method(D_METHOD("rpc_client_on_player_registered", "peer_id", "team_id"), &GameManager::rpc_client_on_player_registered);

	ClassDB::bind_method(D_METHOD("register_player", "peer_id", "team_id", "player_name"), &GameManager::register_player);
	ClassDB::bind_method(D_METHOD("load_available_maps"), &GameManager::load_available_maps);
	ClassDB::bind_method(D_METHOD("set_available_maps", "maps"), &GameManager::set_available_maps);
	ClassDB::bind_method(D_METHOD("get_available_maps"), &GameManager::get_available_maps);

	ClassDB::bind_method(D_METHOD("get_all_player_settings"), &GameManager::get_all_player_settings);

	ClassDB::bind_method(D_METHOD("rpc_server_receive_move", "ids", "pos"), &GameManager::rpc_server_receive_move);
	ClassDB::bind_method(D_METHOD("rpc_server_receive_attack_unit", "ids", "target_id"), &GameManager::rpc_server_receive_attack_unit);

	ClassDB::bind_method(D_METHOD("rpc_client_receive_snapshot", "data"), &GameManager::rpc_client_receive_snapshot);

	ClassDB::bind_method(D_METHOD("rpc_server_request_spawn_unit", "type", "pos", "team"),
		&GameManager::rpc_server_request_spawn_unit);
	ClassDB::bind_method(D_METHOD("rpc_client_spawn_unit", "id", "type", "pos", "team"),
		&GameManager::rpc_client_spawn_unit);
	ClassDB::bind_method(D_METHOD("rpc_client_despawn_unit", "id"),
		&GameManager::rpc_client_despawn_unit);


	ClassDB::bind_method(D_METHOD("rpc_server_request_place_building", "type", "grid_pos", "team"),
		&GameManager::rpc_server_request_place_building);
	ClassDB::bind_method(D_METHOD("rpc_client_spawn_building", "id", "type", "grid_pos", "team"),
		&GameManager::rpc_client_spawn_building);
	ClassDB::bind_method(D_METHOD("rpc_client_remove_building", "id"),
		&GameManager::rpc_client_remove_building);

	ClassDB::bind_method(D_METHOD("rpc_client_spawn_projectile", "type", "start_pos", "start_h", "t_id", "t_is_bld", "t_h", "s_id", "s_is_bld", "dmg"), &GameManager::rpc_client_spawn_projectile);

	ClassDB::bind_method(D_METHOD("rpc_server_request_produce_unit", "id", "unit_type"),
		&GameManager::rpc_server_request_produce_unit);

	ClassDB::bind_method(D_METHOD("rpc_client_sync_resources", "team_id", "amount"),
		&GameManager::rpc_client_sync_resources);

	ClassDB::bind_method(D_METHOD("_on_move_requested", "ids", "pos"), &GameManager::_on_move_requested);
	ClassDB::bind_method(D_METHOD("_on_attack_unit_requested", "ids", "target_id"), &GameManager::_on_attack_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_attack_building_requested", "ids", "target_id"), &GameManager::_on_attack_building_requested);
	ClassDB::bind_method(D_METHOD("_on_unit_production_requested", "id", "type"), &GameManager::_on_unit_production_requested);
	ClassDB::bind_method(D_METHOD("_on_placement_requested", "ids", "grid_pos", "team_id"), &GameManager::_on_placement_requested);
	ClassDB::bind_method(D_METHOD("_on_spawn_unit_requested", "ids", "pos", "team_id"), &GameManager::_on_spawn_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_despawn_unit_requested", "id"), &GameManager::_on_despawn_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_despawn_building_requested", "id"), &GameManager::_on_despawn_building_requested);
	ClassDB::bind_method(D_METHOD("_on_spawn_projectile_requested", "type_name",
		"start_pos", "start_height", "target_id", "target_is_building", "target_height",
		"source_id", "source_is_building", "weapon_damage"), &GameManager::_on_spawn_projectile_requested);
	ClassDB::bind_method(D_METHOD("_on_peer_connected", "id"), &GameManager::_on_peer_connected);
	ClassDB::bind_method(D_METHOD("_on_connected_to_server"), &GameManager::_on_connected_to_server);

	ClassDB::bind_method(D_METHOD("leave_game"), &GameManager::leave_game);

	ClassDB::bind_method(D_METHOD("_on_peer_disconnected", "id"), &GameManager::_on_peer_disconnected);
	ClassDB::bind_method(D_METHOD("_on_server_disconnected"), &GameManager::_on_server_disconnected);
	ClassDB::bind_method(D_METHOD("_on_connection_failed"), &GameManager::_on_connection_failed);

	ClassDB::bind_method(D_METHOD("get_logic_tick_rate"), &GameManager::get_logic_tick_rate);
	ClassDB::bind_method(D_METHOD("set_logic_tick_rate", "logic_tick_rate"), &GameManager::set_logic_tick_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "logic_tick_rate"), "set_logic_tick_rate", "get_logic_tick_rate");

	ClassDB::bind_method(D_METHOD("get_selected_map_index"), &GameManager::get_selected_map_index);
	ClassDB::bind_method(D_METHOD("set_selected_map_index", "selected_map_index"), &GameManager::set_selected_map_index);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "selected_map_index"), "set_selected_map_index", "get_selected_map_index");

	ClassDB::bind_method(D_METHOD("get_local_spawn"), &GameManager::get_local_spawn);
	ClassDB::bind_method(D_METHOD("set_local_spawn", "local_spawn"), &GameManager::set_local_spawn);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "local_spawn"), "set_local_spawn", "get_local_spawn");

	ClassDB::bind_method(D_METHOD("set_local_player_name", "name"), &GameManager::set_local_player_name);
	ClassDB::bind_method(D_METHOD("get_local_player_name"), &GameManager::get_local_player_name);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "local_player_name"), "set_local_player_name", "get_local_player_name");

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "available_maps", PROPERTY_HINT_NONE, "24/17:Resource"), "set_available_maps", "get_available_maps");

	ADD_SIGNAL(MethodInfo("lobby_updated"));
	ADD_SIGNAL(MethodInfo("game_left"));
}