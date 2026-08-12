#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/ip.hpp>
#include <godot_cpp/classes/time.hpp>

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

	Dictionary game_over_config;
	game_over_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	game_over_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	game_over_config["call_local"] = true; // 主机本地也要弹出结束界面
	rpc_config("rpc_client_notify_game_over", game_over_config);

	// --- 主机迁移 RPC ---
	Dictionary mig_req; // 任意 peer 可调用（旧主机/继任者/回归客户端）
	mig_req["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	mig_req["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	mig_req["call_local"] = false;
	rpc_config("rpc_begin_migration", mig_req);
	rpc_config("rpc_report_address", mig_req);
	rpc_config("rpc_server_rejoin", mig_req);

	Dictionary mig_broadcast; // 旧主机广播新主机信息
	mig_broadcast["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	mig_broadcast["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	mig_broadcast["call_local"] = false;
	rpc_config("rpc_handover", mig_broadcast);
}

GameManager::~GameManager() {}

// 游戏主循环
void GameManager::_physics_process(double p_delta) {
	// 新主机在迁移完成前不跑任何游戏逻辑，只等待玩家回归
	if (migration_pending && i_am_new_host) {
		if (rejoined_count >= expected_rejoin_count || Time::get_singleton()->get_ticks_msec() > migration_deadline) {
			finalize_migration();
		}
		else {
			return;
		}
	}

	if (!is_setup || !unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager) { return; }

	if (is_server_authority()) {
		tick_accumulator += p_delta;

		// 服务端更新物理和寻路逻辑
		unit_manager->physics_update(p_delta);
		building_manager->physics_update(p_delta);

		int max_steps = 3;
		while (tick_accumulator >= logic_tick_rate && max_steps > 0) {

			building_manager->prepare_interpolation_snapshot();

			broadcast_network_snapshot();

			for (int team_id = 1; team_id < 10; ++team_id) {
				sync_resources_to_client(team_id);
			}

			tick_accumulator -= logic_tick_rate;
			max_steps--;
		}
		if (tick_accumulator > logic_tick_rate * 5) tick_accumulator = logic_tick_rate; // 丢弃过旧帧

		// 胜负判定计时器
		game_over_check_timer += p_delta;
		if (game_over_check_timer >= CHECK_INTERVAL) {
			game_over_check_timer = 0.0f;
			check_victory_conditions();
		}
	}
	else {
		tick_accumulator += p_delta;
	}
}

// 渲染主循环
void GameManager::_process(double p_delta) {
	// 客户端重连超时：放弃并退回
	if (reconnecting && Time::get_singleton()->get_ticks_msec() > reconnect_deadline) {
		reconnecting = false;
		leave_game();
		return;
	}

	if (migration_pending && i_am_new_host) {
		if (rejoined_count >= expected_rejoin_count || Time::get_singleton()->get_ticks_msec() > migration_deadline) {
			finalize_migration();
		}
		else {
			return;
		}
	}

	if (!is_setup || !unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager) { return; }

	unit_manager->update(p_delta);
	building_manager->update(p_delta);

	update_group(p_delta);
	
	// alpha 表示当前距离下一次网络数据包到达时间的百分比
	// 如果由于网络抖动导致丢包，允许短暂地超量推测 (上限 1.2 倍)，防止画面卡顿
	float alpha = UtilityFunctions::clamp(tick_accumulator / logic_tick_rate, 0.0, 1.2);

	// 更新迷雾
	fog_update_timer += p_delta;
	if (fog_update_timer >= FOG_UPDATE_INTERVAL) {
		fog_update_timer = 0.0f;

		std::vector<Vector2> positions;
		std::vector<float> radii;

		for (const auto& unit : unit_manager->units) {
			if (unit.team_id == selection_manager->get_team_id()) {
				// 记录 3D 世界的 X 和 Z 坐标
				Vector2 visual_pos = UtilityFunctions::lerp(unit.prev_position, unit.next_position, alpha);
				positions.push_back(visual_pos);
				radii.push_back(unit.stats->sight_range);
			}
		}

		for (auto& pair : building_manager->buildings) {
			BuildingData& building = pair.second;
			if (building.team_id == selection_manager->get_team_id() && building.state != BuildingState::BUILDING) {
				positions.push_back(Vector2(building.grid_pos * flow_field_manager->get_cell_size()) +
					Vector2(building.stats->get_footprint() * flow_field_manager->get_cell_size()) / 2);
				radii.push_back(building.stats->sight_range);
			}
		}

		fog_manager->update_vision(positions, radii);
	}

	// 驱动底层 MultiMesh 实例，让显卡去画出介于 prev 和 next 之间的平滑位置
	unit_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	building_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	weapon_manager->update_multimesh_buffer(p_delta, alpha, unit_manager, building_manager, selection_manager);

	effect_manager->update(p_delta);
}

void GameManager::update_group(double p_delta) {
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->get_group_cleanup_interval()) return;
	group_manager->cleanup_timer = 0.0;

	// 计算每个组所需的到达集成值界限
	group_manager->update_target_integrations(flow_field_manager);

	// 清理过期的编队
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

void GameManager::set_audio_manager(Node* p_node)
{
	audio_manager = Object::cast_to<AudioManager>(p_node);
}

void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_group_manager(group_manager);
	unit_manager->set_fog_manager(fog_manager);
	unit_manager->set_attack_manager(attack_manager);
	unit_manager->set_weapon_manager(weapon_manager);
	unit_manager->set_effect_manager(effect_manager);

	building_manager->set_attack_manager(attack_manager);
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
	projectile_manager->set_audio_manager(audio_manager);
	projectile_manager->set_building_manager(building_manager);

	Vector2 map_size = Vector2(p_width, p_height) * Vector2(p_cell_size);
	Vector2 map_pos = Vector2(p_origin) * Vector2(p_cell_size);
	fog_manager->setup_fog(map_pos, map_size);

	unit_manager->setup_system(p_width, p_height, p_cell_size, p_origin);
	building_manager->setup_system(p_width, p_height, p_cell_size);
	weapon_manager->setup_system(fog_manager);
	is_setup = true;
	game_over = false;
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
	local_peer_id = 1;
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

	// 记录目标地址/端口，供断线后自动重连使用
	server_address = p_address;
	server_port = p_port;

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

	// 安全校验：未注册的远端 peer 无权修改地图
	int sender_id = get_multiplayer()->get_remote_sender_id();
	if (sender_id > 1 && !players_settings.has(sender_id)) {
		UtilityFunctions::print("[Security] 拒绝未注册 peer ", sender_id, " 修改地图");
		return;
	}

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

	// 安全校验：远端必须已注册（主机本地调用 sender_id <= 1，天然信任）
	if (sender_id > 1 && !players_settings.has(sender_id)) {
		UtilityFunctions::print("[Security] 拒绝未注册 peer ", sender_id, " 修改设置");
		return;
	}

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
	emit_signal("start_game");
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

// 安全校验辅助：单位 ID 数组是否全部归属指定队伍
bool GameManager::_units_owned_by_team(PackedInt32Array p_ids, int p_team) {
	if (!unit_manager) return false;
	if (p_team < 0) return false;
	for (int i = 0; i < p_ids.size(); ++i) {
		int idx = unit_manager->get_unit_index_by_id(p_ids[i]);
		if (idx == -1) return false;                     // 单位不存在
		if (unit_manager->units[idx].team_id != p_team) return false; // 非本队单位
	}
	return true;
}

void GameManager::rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos) {
	if (!get_multiplayer()->is_server()) return;

	// 安全校验：远端必须已注册，且所有被指挥单位归属其队伍
	int sender_id = get_multiplayer()->get_remote_sender_id();
	if (sender_id > 1) {
		if (!players_settings.has(sender_id)) { return; }
		if (!_units_owned_by_team(p_ids, players_settings[sender_id].team_id)) {
			UtilityFunctions::print("[Security] 拒绝 peer ", sender_id, " 越权移动指令");
			return;
		}
	}

	if (unit_manager) {
		unit_manager->command_units_to_move(p_ids, p_pos);
	}
}

void GameManager::rpc_server_receive_attack_unit(PackedInt32Array p_ids, int p_target_id, bool p_target_is_building) {
	if (!get_multiplayer()->is_server()) return;

	// 安全校验：同上，禁止指挥他人单位
	int sender_id = get_multiplayer()->get_remote_sender_id();
	if (sender_id > 1) {
		if (!players_settings.has(sender_id)) { return; }
		if (!_units_owned_by_team(p_ids, players_settings[sender_id].team_id)) {
			UtilityFunctions::print("[Security] 拒绝 peer ", sender_id, " 越权攻击指令");
			return;
		}
	}

	if (unit_manager) {
		unit_manager->command_units_to_attack_target(p_ids, p_target_id, p_target_is_building, building_manager);
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
		// 基础29字节(含护盾) + 武器数量标记(1字节) + 每把武器的旋转角度(4字节 * 武器数)
		total_unit_bytes += 29 + 1 + unit.weapons.size() * 4;
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
		data.encode_float(offset + 24, unit.current_shield);
		data.set(offset + 28, (uint8_t)unit.state);
		offset += 29;

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
		if (offset + 29 > p_raw_data.size()) break; // 基础长度边界检查

		int id = p_raw_data.decode_s32(offset);
		float px = p_raw_data.decode_float(offset + 4);
		float py = p_raw_data.decode_float(offset + 8);
		float rot = p_raw_data.decode_float(offset + 12);
		float height = p_raw_data.decode_float(offset + 16);
		float health = p_raw_data.decode_float(offset + 20);
		float shield = p_raw_data.decode_float(offset + 24);
		uint8_t state = p_raw_data.get(offset + 28);
		offset += 29;

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
			unit.current_shield = shield;
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
			if (offset + 13 > p_raw_data.size()) break;

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

	// 安全校验：远端只能为自己队伍生成单位（主机本地调用不受限）
	int sender_id = get_multiplayer()->get_remote_sender_id();
	if (sender_id > 1) {
		if (!players_settings.has(sender_id)) { return; }
		if (players_settings[sender_id].team_id != p_team) {
			UtilityFunctions::print("[Security] 拒绝 peer ", sender_id, " 为其他队伍生成单位");
			return;
		}
	}

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
void GameManager::rpc_server_request_place_building(
	String p_type, Vector2i p_grid_pos, int p_team, int p_force_id, bool p_is_pre_placed, PackedInt32Array p_builder_ids) {
	if (!is_server_authority()) return;

	// 安全校验：远端只能为自己队伍放置建筑，且建造者单位必须归属自己
	int sender_id = get_multiplayer()->get_remote_sender_id();
	if (sender_id > 1) {
		if (!players_settings.has(sender_id)) { return; }
		if (players_settings[sender_id].team_id != p_team) {
			UtilityFunctions::print("[Security] 拒绝 peer ", sender_id, " 为其他队伍放置建筑");
			return;
		}
		if (!_units_owned_by_team(p_builder_ids, players_settings[sender_id].team_id)) {
			UtilityFunctions::print("[Security] 拒绝 peer ", sender_id, " 越权指挥建造单位");
			return;
		}
	}

	Ref<BuildingStats> stats = building_manager->get_building_stats_by_type(p_type);
	if (stats.is_null()) return;

	int new_id = -1;

	// 1. 处理预放置建筑或正常扣费逻辑
	if (p_is_pre_placed) {
		new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team, -1, true);
	}
	else {
		if (economy_manager->try_spend(p_team, stats->get_cost())) {
			new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team, -1, false);
			sync_resources_to_client(p_team);
		}
	}

	// 2. 如果建筑生成成功，且有传入的建造者 ID
	if (new_id != -1) {
		// 同步给所有客户端显示建筑
		rpc("rpc_client_spawn_building", new_id, p_type, p_grid_pos, p_team, p_is_pre_placed);

		// --- 新增功能：指挥单位建造 ---
		if (p_builder_ids.size() > 0 && unit_manager) {
			// 指挥这些单位去“攻击”这个新建筑（即执行建造逻辑）
			// 参数说明: 单位 ID 数组, 目标 ID, 目标是否为建筑 (true), 建筑管理器
			unit_manager->command_units_to_attack_target(p_builder_ids, new_id, true, building_manager);
		}
	}
}

void GameManager::rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team, bool p_is_pre_placed) {
	if (is_server_authority()) return;
	building_manager->place_building_by_type(p_type, p_grid_pos, p_team, p_id, p_is_pre_placed);
}

void GameManager::rpc_client_despawn_unit(int p_id) {
	if (unit_manager) {
		unit_manager->despawn_unit(p_id, selection_manager); // 真正删除本地实体（含选择清理）
	}
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

void GameManager::rpc_client_notify_game_over(int p_winner_team) {
	game_over = true;
	UtilityFunctions::print("Game Over! Winner Team: ", p_winner_team);

	// 停止本地部分模拟
	// 可以在这里通知 UI 层显示结算画面
	emit_signal("game_finished", p_winner_team);
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
		if (unit_manager) unit_manager->command_units_to_attack_target(p_ids, p_target_id, false, building_manager);
	}
	else {
		rpc_id(1, "rpc_server_receive_attack_unit", p_ids, p_target_id, false);
	}
}

void GameManager::_on_attack_building_requested(PackedInt32Array p_ids, int p_target_id) {
	if (get_multiplayer()->is_server()) {
		if (unit_manager) unit_manager->command_units_to_attack_target(p_ids, p_target_id, true, building_manager);
	}
	else {
		rpc_id(1, "rpc_server_receive_attack_unit", p_ids, p_target_id, true);
	}
}

void GameManager::_on_placement_requested(String p_type_name, Vector2i p_grid_pos, int p_team_id, int p_forced_id, bool p_is_pre_placed) {
	// 获取当前选中的单位 ID 列表
	PackedInt32Array builder_ids;
	if (selection_manager) {
		builder_ids = selection_manager->get_selected_unit_ids(); // 假设你的 SelectionManager 有这个方法
	}
	
	if (get_multiplayer()->is_server()) {
		// 如果是服务器（比如单机测试或主机），直接进入处理逻辑
		rpc_server_request_place_building(p_type_name, p_grid_pos, p_team_id, p_forced_id, p_is_pre_placed, builder_ids);
	}
	else {
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_grid_pos, p_team_id, p_forced_id, p_is_pre_placed, builder_ids);
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
	// 广播删除，保证客户端本地实体同步移除（服务器权威）
	if (get_multiplayer()->is_server()) {
		rpc("rpc_client_despawn_unit", p_unit_id);
	}
}

void GameManager::_on_despawn_building_requested(int p_bid) {
	building_manager->remove_building(p_bid, selection_manager);
	if (get_multiplayer()->is_server()) {
		rpc("rpc_client_remove_building", p_bid);
	}
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
	local_peer_id = my_id;

	// 重连（普通掉线 或 迁移后）统一走 rejoin 自报旧身份
	if (reconnecting || migration_pending) {
		reconnecting = false;
		rpc_id(1, "rpc_server_rejoin", my_old_peer_id);
		return;
	}

	int desired_team = 0;
	rpc_id(1, "rpc_server_request_registration", desired_team, local_player_name);
}

void GameManager::leave_game() {
	// 释放网络层
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}

	players_settings.clear();
	reset_game_state();

	UtilityFunctions::print("Left game and disconnected network.");

	// 发出自定义断开信号，供 GDScript 侧响应（如清理场景或切回主菜单）
	emit_signal("game_left");
}

void GameManager::reset_game_state() {
	is_setup = false;
	game_in_progress = false;
}

void GameManager::_on_peer_disconnected(int p_id) {
	if (get_multiplayer()->is_server()) {
		UtilityFunctions::print("Server: Peer disconnected: ", p_id);

		// 大厅：直接移除并刷新
		if (!game_in_progress) {
			players_settings.erase(p_id);
			rpc_server_set_map(selected_map_index);
		}
		else {
			// 局内掉线：保留该玩家全部实体与身份，暂存其队伍信息，直到其重连
			if (players_settings.has(p_id)) {
				disconnected_players[p_id] = players_settings[p_id];
				players_settings.erase(p_id);
			}
		}
	}
	else {
		UtilityFunctions::print("Client: Peer disconnected: ", p_id);
	}
}

void GameManager::_on_server_disconnected() {
	UtilityFunctions::print("Client: Server disconnected.");

	if (migration_pending) {
		// 主机迁移中：继任者接管为服务器，普通客户端重连
		if (i_am_new_host) {
			_become_new_host();
		}
		else {
			_reconnect_to_new_host();
		}
		return;
	}

	if (game_in_progress && !is_server_authority()) {
		// 普通掉线：保留场上行为，自动重连同一主机
		_reconnect_to_host();
		return;
	}

	// 大厅或非游戏场景断线，按原逻辑退出
	leave_game();
}

void GameManager::_on_connection_failed() {
	UtilityFunctions::print("Client: Connection failed.");

	// 重连流程中：失败则重试，超过次数或超时则退出
	if (reconnecting) {
		if (reconnect_attempts < MAX_RECONNECT_ATTEMPTS) {
			_reconnect_to_host();
			return;
		}
		reconnecting = false;
	}
	leave_game();
}

// ========== 主机迁移（阶段1：优雅交接） ==========

// 旧主机发起迁移：序列化状态发给继任者，收到继任者地址后广播交接信息，随后旧主机自动退出
// 返回 true 表示已发起（旧主机稍后会自动 leave_game）；false 表示无法交接，调用方应直接退出
bool GameManager::migrate_host() {
	if (!is_server_authority()) { return false; }
	if (!game_in_progress) { return false; } // 大厅阶段暂不做迁移

	elected_successor = pick_successor();
	if (elected_successor == -1) {
		UtilityFunctions::print("没有可交接的玩家，直接退出。");
		return false;
	}

	Dictionary state = serialize_game_state();
	Dictionary mapping = get_all_player_settings();
	UtilityFunctions::print("开始迁移：向继任者 peer ", elected_successor, " 发送状态 (units=",
		((Array)state["units"]).size(), ")");
	rpc_id(elected_successor, "rpc_begin_migration", state, mapping, server_port);

	// 旧主机不再参与游戏逻辑：立即退出本地模拟，避免切场景后 manager 悬垂
	reset_game_state();
	return true;
}

void GameManager::rpc_begin_migration(Dictionary p_state, Dictionary p_mapping, int p_port) {
	// 仅继任者接收
	migration_pending = true;
	i_am_new_host = true;
	my_old_peer_id = get_multiplayer()->get_unique_id();
	pending_game_state = p_state;
	migration_mapping = p_mapping;
	migration_port = p_port;
	UtilityFunctions::print("收到迁移状态，我是新主机（旧 peer ", my_old_peer_id, "）。上报地址...");
	rpc_id(1, "rpc_report_address", _get_local_ip(), migration_port);
}

void GameManager::rpc_report_address(String p_addr, int p_port) {
	if (!is_server_authority()) { return; }
	successor_address = p_addr;
	successor_port = p_port;
	UtilityFunctions::print("继任者地址: ", p_addr, ":", p_port, "。广播交接信息。");
	rpc("rpc_handover", elected_successor, successor_address, successor_port);
	// 广播完成后旧主机退出，触发客户端 server_disconnected 走迁移流程
	leave_game();
}

void GameManager::rpc_handover(int p_successor, String p_addr, int p_port) {
	// 继任者已在 rpc_begin_migration 中就绪，忽略这条广播
	if (i_am_new_host) { return; }
	// 所有普通客户端记录新主机信息
	migration_pending = true;
	my_old_peer_id = get_multiplayer()->get_unique_id();
	successor_address = p_addr;
	successor_port = p_port;
	UtilityFunctions::print("收到交接信息：新主机 peer ", p_successor, " @ ", p_addr, ":", p_port);
}

void GameManager::rpc_server_rejoin(int p_old_peer_id) {
	if (!get_multiplayer()->is_server()) { return; }
	int new_id = get_multiplayer()->get_remote_sender_id();

	// 优先从"掉线暂存"恢复（普通重连），其次从迁移映射恢复
	PlayerSettings restored;
	bool found = false;
	if (disconnected_players.has(p_old_peer_id)) {
		restored = disconnected_players[p_old_peer_id];
		disconnected_players.erase(p_old_peer_id);
		found = true;
	}
	else if (migration_mapping.has(p_old_peer_id)) {
		Dictionary info = migration_mapping[p_old_peer_id];
		restored.team_id = info["team"];
		restored.spawn_id = info["spawn"];
		restored.name = info["name"];
		found = true;
	}

	if (!found) {
		UtilityFunctions::print("忽略未知的回归玩家 old=", p_old_peer_id);
		return;
	}

	PlayerSettings& s = players_settings[new_id];
	s = restored;
	if (migration_pending && i_am_new_host) {
		rejoined_count++;
	}
	UtilityFunctions::print("玩家回归 old=", p_old_peer_id, " -> new=", new_id, " (team ", s.team_id, ")");
}

// 新主机：断掉旧连接后创建服务器，自己成为 peer 1
void GameManager::_become_new_host() {
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}
	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_server(migration_port);
	if (err != OK) {
		UtilityFunctions::printerr("迁移失败：无法创建服务器 (port ", migration_port, ")");
		leave_game();
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);

	players_settings.clear();
	// 新主机自己：new_id = 1，用旧映射找回自己的队伍
	if (migration_mapping.has(my_old_peer_id)) {
		Dictionary info = migration_mapping[my_old_peer_id];
		PlayerSettings& s = players_settings[1];
		s.team_id = info["team"];
		s.spawn_id = info["spawn"];
		s.name = info["name"];
	}
	rejoined_count = 1;
	expected_rejoin_count = migration_mapping.size() - 1; // 除旧主机外的所有玩家
	migration_deadline = Time::get_singleton()->get_ticks_msec() + 5000;
	UtilityFunctions::print("迁移：新主机上线 (peer 1)，等待其余玩家回归 (", expected_rejoin_count - 1, " 个)。");
}

// 普通客户端：重连到新主机
void GameManager::_reconnect_to_new_host() {
	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}
	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_client(successor_address, successor_port);
	if (err != OK) {
		UtilityFunctions::printerr("迁移重连失败：无法连接 ", successor_address, ":", successor_port);
		leave_game();
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);
	UtilityFunctions::print("迁移：重连新主机 ", successor_address, ":", successor_port, " ...");
}

// 新主机：所有玩家回归（或超时）后恢复完整游戏状态
void GameManager::finalize_migration() {
	if (!migration_pending || !i_am_new_host) { return; }

	migration_pending = false;
	restore_game_state(pending_game_state);
	pending_game_state = Dictionary();
	migration_mapping = Dictionary();

	game_in_progress = true;
	game_over = false;
	tick_accumulator = 0.0;
	UtilityFunctions::print("主机迁移完成，游戏继续。");
}

// 普通客户端：断线后自动重连同一主机（玩家实体/行为保留在场上）
void GameManager::_reconnect_to_host() {
	my_old_peer_id = local_peer_id;
	reconnecting = true;
	reconnect_attempts++;
	reconnect_deadline = Time::get_singleton()->get_ticks_msec() + RECONNECT_TIMEOUT_MS;

	if (get_multiplayer()->has_multiplayer_peer()) {
		get_multiplayer()->set_multiplayer_peer(Ref<MultiplayerPeer>());
	}
	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_client(server_address, server_port);
	if (err != OK) {
		UtilityFunctions::printerr("重连失败：无法连接 ", server_address, ":", server_port);
		reconnecting = false;
		leave_game();
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);
	UtilityFunctions::print("重连中... ", server_address, ":", server_port, " (第 ", reconnect_attempts, " 次)");
}

int GameManager::pick_successor() {
	int best = -1;
	for (const auto& E : players_settings) {
		if (E.key == 1) continue; // 自己
		if (best == -1 || E.key < best) best = E.key;
	}
	return best;
}

String GameManager::_get_local_ip() {
	PackedStringArray addrs = IP::get_singleton()->get_local_addresses();
	for (int i = 0; i < addrs.size(); ++i) {
		if (addrs[i].begins_with("127.")) continue; // 跳过回环
		if (addrs[i].contains(":")) continue;       // 跳过 IPv6
		return addrs[i];
	}
	return "127.0.0.1";
}

// 序列化完整游戏状态（单位/建筑/经济），供主机迁移使用
Dictionary GameManager::serialize_game_state() {
	Dictionary state;

	Array units;
	for (const auto& u : unit_manager->units) {
		Dictionary d;
		d["id"] = u.id;
		d["type"] = u.stats->get_unit_name();
		d["pos"] = u.position;
		d["vel"] = u.velocity;
		d["rot"] = u.rotation;
		d["ang_vel"] = u.angular_velocity;
		d["target_pos"] = u.target_pos;
		d["target_off"] = u.target_pos_offset;
		d["target_grid"] = u.target_grid;
		d["target_id"] = u.target_id;
		d["target_is_b"] = u.target_is_building;
		d["manual"] = u.is_manual_target;
		d["state"] = (int)u.state;
		d["health"] = u.current_health;
		d["height"] = u.height;
		d["team"] = u.team_id;
		Array cd;
		for (float c : u.weapon_cooldowns) cd.append(c);
		d["weapon_cd"] = cd;
		Array ws;
		for (const auto& w : u.weapons) {
			Dictionary wd;
			wd["name"] = w.stats->get_weapon_name();
			wd["rot"] = w.rotation;
			wd["cd"] = w.current_cooldown;
			wd["target"] = w.target_id;
			wd["state"] = (int)w.state;
			ws.append(wd);
		}
		d["weapons"] = ws;
		units.append(d);
	}
	state["units"] = units;

	Array blds;
	for (const auto& pair : building_manager->buildings) {
		const BuildingData& b = pair.second;
		Dictionary d;
		d["id"] = b.id;
		d["grid"] = b.grid_pos;
		d["type"] = b.stats->get_building_name();
		d["team"] = b.team_id;
		d["health"] = b.current_health;
		d["state"] = (int)b.state;
		d["build_timer"] = b.build_timer;
		d["prod_timer"] = b.unit_production_timer;
		Array pq;
		for (const String& s : b.production_queue) pq.append(s);
		d["prod_queue"] = pq;
		Array ws;
		for (const auto& w : b.weapons) {
			Dictionary wd;
			wd["name"] = w.stats->get_weapon_name();
			wd["rot"] = w.rotation;
			wd["cd"] = w.current_cooldown;
			wd["target"] = w.target_id;
			wd["state"] = (int)w.state;
			ws.append(wd);
		}
		d["weapons"] = ws;
		blds.append(d);
	}
	state["buildings"] = blds;

	Dictionary econ;
	for (int team = 1; team < 10; ++team) {
		if (economy_manager->has_team(team)) {
			econ[String::num(team)] = economy_manager->get_balance(team);
		}
	}
	state["economy"] = econ;

	return state;
}

// 新主机恢复状态（在清空本地实体后调用）
void GameManager::restore_game_state(const Dictionary& p_state) {
	unit_manager->clear_all_units();
	building_manager->clear_all_buildings();
	group_manager->temp_groups.clear();

	// --- 单位 ---
	Array units = p_state["units"];
	for (int i = 0; i < units.size(); ++i) {
		Dictionary d = units[i];
		int new_id = unit_manager->spawn_unit_by_type(d["type"], d["pos"], d["team"], d["id"]);
		if (new_id == -1) continue;
		int idx = unit_manager->get_unit_index_by_id(new_id);
		if (idx == -1) continue;
		UnitData& u = unit_manager->units[idx];
		u.velocity = d["vel"];
		u.rotation = d["rot"];
		u.angular_velocity = d["ang_vel"];
		u.target_pos = d["target_pos"];
		u.target_pos_offset = d["target_off"];
		u.target_grid = d["target_grid"];
		u.target_id = d["target_id"];
		u.target_is_building = d["target_is_b"];
		u.is_manual_target = d["manual"];
		u.state = (UnitState)(int)d["state"];
		u.current_health = d["health"];
		u.height = d["height"];
		Array cd = d["weapon_cd"];
		u.weapon_cooldowns.resize(cd.size());
		for (int k = 0; k < cd.size(); ++k) u.weapon_cooldowns[k] = cd[k];
		Array ws = d["weapons"];
		for (int k = 0; k < ws.size() && k < (int)u.weapons.size(); ++k) {
			Dictionary wd = ws[k];
			u.weapons[k].rotation = wd["rot"];
			u.weapons[k].current_cooldown = wd["cd"];
			u.weapons[k].target_id = wd["target"];
			u.weapons[k].state = (WeaponStateEnum)(int)wd["state"];
		}
		// 重置瞬态字段（编队/粒子/卡死检测等跨进程无意义的状态）
		u.temp_group_id = -1;
		u.prev_position = u.position; u.next_position = u.position;
		u.prev_rotation = u.rotation; u.next_rotation = u.rotation;
		u.prev_height = u.height; u.next_height = u.height;
		u.last_visual_pos = u.position;
		u.anim_time = 0.0f;
		u.stuck_timer = 0.0f;
	}

	// --- 建筑 ---
	Array blds = p_state["buildings"];
	for (int i = 0; i < blds.size(); ++i) {
		Dictionary d = blds[i];
		int id = building_manager->place_building_by_type(d["type"], d["grid"], d["team"], d["id"], true, true); // force
		if (id == -1) continue;
		BuildingData& b = building_manager->buildings[id];
		b.current_health = d["health"];
		b.state = (BuildingState)(int)d["state"];
		b.build_timer = d["build_timer"];
		b.unit_production_timer = d["prod_timer"];
		b.production_queue.clear();
		Array pq = d["prod_queue"];
		for (int k = 0; k < pq.size(); ++k) b.production_queue.push_back(pq[k]);
		Array ws = d["weapons"];
		for (int k = 0; k < ws.size() && k < (int)b.weapons.size(); ++k) {
			Dictionary wd = ws[k];
			b.weapons[k].rotation = wd["rot"];
			b.weapons[k].current_cooldown = wd["cd"];
			b.weapons[k].target_id = wd["target"];
			b.weapons[k].state = (WeaponStateEnum)(int)wd["state"];
		}
	}

	// --- 经济 ---
	Dictionary econ = p_state["economy"];
	Array ekeys = econ.keys();
	for (int i = 0; i < ekeys.size(); ++i) {
		economy_manager->set_balance((int)ekeys[i], econ[ekeys[i]]);
	}

	flow_field_manager->make_all_dirty();
}

void GameManager::check_victory_conditions() {
	if (!is_server_authority()) { return; }
	if (game_over) { return; }

	std::set<int> active_teams;

	// 1. 检查谁还有建筑
	for (const auto& pair : building_manager->buildings) {
		active_teams.insert(pair.second.team_id);
	}

	// 2. (可选) 检查谁还有单位，如果只有建筑没有单位也算输可以不加这步
	/*
	for (const auto& unit : unit_manager->units) {
		active_teams.insert(unit.team_id);
	}
	*/

	// 3. 判定结果
	if (active_teams.empty()) {
		// 极端情况：同归于尽或地图无玩家
		return;
	}

	if (active_teams.size() == 1) {
		// 只剩一支队伍，获胜
		int winner = *active_teams.begin();
		rpc("rpc_client_notify_game_over", winner);
	}
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
	ClassDB::bind_method(D_METHOD("set_audio_manager", "node"), &GameManager::set_audio_manager);
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
	ClassDB::bind_method(D_METHOD("rpc_server_receive_attack_unit", "ids", "target_id", "target_is_building"), &GameManager::rpc_server_receive_attack_unit);

	ClassDB::bind_method(D_METHOD("rpc_client_receive_snapshot", "data"), &GameManager::rpc_client_receive_snapshot);

	ClassDB::bind_method(D_METHOD("rpc_server_request_spawn_unit", "type", "pos", "team"),
		&GameManager::rpc_server_request_spawn_unit);
	ClassDB::bind_method(D_METHOD("rpc_client_spawn_unit", "id", "type", "pos", "team"),
		&GameManager::rpc_client_spawn_unit);
	ClassDB::bind_method(D_METHOD("rpc_client_despawn_unit", "id"),
		&GameManager::rpc_client_despawn_unit);


	ClassDB::bind_method(
		D_METHOD("rpc_server_request_place_building", "type", "grid_pos", "team", "forced_id", "is_pre_placed", "builder_ids"),
		&GameManager::rpc_server_request_place_building,
		DEFVAL(PackedInt32Array()) // 设置默认值
	);
	ClassDB::bind_method(D_METHOD("rpc_client_spawn_building", "id", "type", "grid_pos", "team", "is_pre_placed"),
		&GameManager::rpc_client_spawn_building);
	ClassDB::bind_method(D_METHOD("rpc_client_remove_building", "id"),
		&GameManager::rpc_client_remove_building);

	ClassDB::bind_method(D_METHOD("rpc_client_spawn_projectile", "type", "start_pos", "start_h", "t_id", "t_is_bld", "t_h", "s_id", "s_is_bld", "dmg"), &GameManager::rpc_client_spawn_projectile);

	ClassDB::bind_method(D_METHOD("rpc_server_request_produce_unit", "id", "unit_type"),
		&GameManager::rpc_server_request_produce_unit);

	ClassDB::bind_method(D_METHOD("rpc_client_sync_resources", "team_id", "amount"),
		&GameManager::rpc_client_sync_resources);

	ClassDB::bind_method(D_METHOD("rpc_client_notify_game_over", "winner_team"), &GameManager::rpc_client_notify_game_over);

	// --- 主机迁移绑定 ---
	ClassDB::bind_method(D_METHOD("migrate_host"), &GameManager::migrate_host);
	ClassDB::bind_method(D_METHOD("rpc_begin_migration", "state", "mapping", "port"), &GameManager::rpc_begin_migration);
	ClassDB::bind_method(D_METHOD("rpc_report_address", "addr", "port"), &GameManager::rpc_report_address);
	ClassDB::bind_method(D_METHOD("rpc_handover", "successor", "addr", "port"), &GameManager::rpc_handover);
	ClassDB::bind_method(D_METHOD("rpc_server_rejoin", "old_peer_id"), &GameManager::rpc_server_rejoin);
	ClassDB::bind_method(D_METHOD("serialize_game_state"), &GameManager::serialize_game_state);
	ClassDB::bind_method(D_METHOD("restore_game_state", "state"), &GameManager::restore_game_state);
	ClassDB::bind_method(D_METHOD("finalize_migration"), &GameManager::finalize_migration);

	ClassDB::bind_method(D_METHOD("_on_move_requested", "ids", "pos"), &GameManager::_on_move_requested);
	ClassDB::bind_method(D_METHOD("_on_attack_unit_requested", "ids", "target_id"), &GameManager::_on_attack_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_attack_building_requested", "ids", "target_id"), &GameManager::_on_attack_building_requested);
	ClassDB::bind_method(D_METHOD("_on_unit_production_requested", "id", "type"), &GameManager::_on_unit_production_requested);
	ClassDB::bind_method(D_METHOD("_on_placement_requested", "ids", "grid_pos", "team_id", "forced_id", "is_pre_placed"), &GameManager::_on_placement_requested);
	ClassDB::bind_method(D_METHOD("_on_spawn_unit_requested", "ids", "pos", "team_id"), &GameManager::_on_spawn_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_despawn_unit_requested", "id"), &GameManager::_on_despawn_unit_requested);
	ClassDB::bind_method(D_METHOD("_on_despawn_building_requested", "id"), &GameManager::_on_despawn_building_requested);
	ClassDB::bind_method(D_METHOD("_on_spawn_projectile_requested", "type_name",
		"start_pos", "start_height", "target_id", "target_is_building", "target_height",
		"source_id", "source_is_building", "weapon_damage"), &GameManager::_on_spawn_projectile_requested);
	ClassDB::bind_method(D_METHOD("_on_peer_connected", "id"), &GameManager::_on_peer_connected);
	ClassDB::bind_method(D_METHOD("_on_connected_to_server"), &GameManager::_on_connected_to_server);

	ClassDB::bind_method(D_METHOD("leave_game"), &GameManager::leave_game);
	ClassDB::bind_method(D_METHOD("reset_game_state"), &GameManager::reset_game_state);

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

	ClassDB::bind_method(D_METHOD("start_game"), &GameManager::start_game);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "available_maps", PROPERTY_HINT_NONE, "24/17:Resource"), "set_available_maps", "get_available_maps");

	ADD_SIGNAL(MethodInfo("lobby_updated"));
	ADD_SIGNAL(MethodInfo("game_left"));
	ADD_SIGNAL(MethodInfo("game_finished", PropertyInfo(Variant::INT, "winner_team_id")));
	ADD_SIGNAL(MethodInfo("start_game"));
}