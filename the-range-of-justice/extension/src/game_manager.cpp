#pragma once

#include <godot_cpp/core/class_db.hpp>

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

	Dictionary snapshot_config;
	snapshot_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	snapshot_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_UNRELIABLE_ORDERED;
	snapshot_config["call_local"] = false;
	rpc_config("rpc_client_receive_snapshot", snapshot_config);

	rpc_config("rpc_server_request_produce_unit", req_config);

	rpc_config("rpc_client_sync_resources", sync_config);

	Dictionary start_config;
	start_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	start_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	start_config["call_local"] = true; // 主机也要执行场景切换
	rpc_config("rpc_client_load_game", start_config);

	// 客户端请求注册：ANY_PEER (任何人可叄1�7), RELIABLE
	Dictionary reg_config;
	reg_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	reg_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_config["call_local"] = false;
	rpc_config("rpc_server_request_registration", reg_config);

	// 服务器同步注册结果给扢�有客户端：AUTHORITY (仅服务器叄1�7), RELIABLE
	Dictionary reg_sync_config;
	reg_sync_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	reg_sync_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_sync_config["call_local"] = true; // 确保服务器本地也更新 map
	rpc_config("rpc_client_on_player_registered", reg_sync_config);
}

GameManager::~GameManager() {}

void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager || !is_setup) { return; }
	if (!is_setup) return;

	if (is_server_authority()) {
		// --- A. 服务器��辑 ---
		tick_accumulator += p_delta;

		// 1. 运行单位逻辑 (移动计算、流场应用��状态切捄1�7)
			// 注意：内部会射1�7 position 存入 prev_position，新算的存入 next_position
		unit_manager->physics_update(p_delta);

		// 2. 运行建筑逻辑 (建��进度增加��生产队列��辑)
		building_manager->update(p_delta);

		// 确保逻辑按固定频率运衄1�7 (逻辑 Tick)
		while (tick_accumulator >= logic_tick_rate) {

			// 3. 定期广播快照 (可以毄1�7 Tick 广播，也可以毄1�7 2 丄1�7 Tick 广播)
			broadcast_network_snapshot();

			// 以后改成根据队伍数量的循玄1�7
			for (int team_id = 1; team_id < 10; ++team_id) {
				sync_resources_to_client(team_id);
			}

			tick_accumulator -= logic_tick_rate;
		}
	}
	else {
		// --- B. 客户端��辑 ---
		// 客户端在 _physics_process 里��常只负责接收包和维护本地计时器
		// 实际的坐标平滑放圄1�7 _process 里做
		tick_accumulator += p_delta;
	}
}

void GameManager::_process(double p_delta) {
	if (!is_setup) return;

	unit_manager->update(p_delta);
	// 1. 计算插��系敄1�7 alpha (0.0 刄1�7 1.0)
	// 它代表当前时间处于两个��辑 Tick 之间的位罄1�7
	float alpha = UtilityFunctions::clamp(tick_accumulator / logic_tick_rate, 0.0, 1.2);

	// 2. 执行插��渲柄1�7 (MultiMesh 绘制)
	// 这个方法内部会使甄1�7 lerp(prev_pos, next_pos, alpha)
	unit_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	building_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);

	// 3. 运行本地特效 (如投射物飞行、粒子效构1�7)
	// 这些通常不强汄1�7 Tick 同步，随帧率跑更流畅
	// projectile_manager->update_visuals(p_delta);
}

void GameManager::update_group(double p_delta) {
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->CLEANUP_INTERVAL) return;
	group_manager->cleanup_timer = 0.0;

	auto it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		// 如果没有单位在移动了，且 ID 列表也空了（或单位都到达了）
		// 这里可以根据霢�求决定：是计数器归零就删，还是单位完全清空才刄1�7
		if (it->second.moving_units_count <= 0) {
			for (int unit_id : it->second.unit_ids) {
				int index = unit_manager->get_unit_index_by_id(unit_id);
				if (index >= 0) {
					unit_manager->units[index].temp_group_id = -1;
				}
			}
			it = group_manager->temp_groups.erase(it);
		}
		else {
			++it;
		}
	}
}

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
		// 圄1�7 C++ 中连接信叄1�7
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


void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_group_manager(group_manager);
	building_manager->set_flow_field_manager(flow_field_manager);
	building_manager->set_unit_manager(unit_manager);
	building_manager->set_economy_manager(economy_manager);
	selection_manager->set_team_id(peer_to_team_map[get_multiplayer()->get_unique_id()]);
	unit_manager->setup_system(p_width, p_height, p_cell_size, p_origin);
	is_setup = true;
}

void GameManager::host_game(int p_port) {
	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_server(p_port);
	if (err != OK) {
		UtilityFunctions::print("Failed to host server.");
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);

	register_player(1, 1);
	UtilityFunctions::print("Server started on port: ", p_port);
}

void GameManager::join_game(String p_address, int p_port) {
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
	// 广播给所有客户端加载游戏场景
	rpc("rpc_client_load_game", "res://main/main.tscn");
}

void GameManager::register_player(int p_peer_id, int p_team_id) {
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Registered Peer: ", p_peer_id, " to Team: ", p_team_id);
}

void GameManager::rpc_client_load_game(const String& p_scene_path) {
	// 使用 Godot 的场景切换功胄1�7
	get_tree()->change_scene_to_file(p_scene_path);
}

// [RPC] 运行在服务器丄1�7
void GameManager::rpc_server_request_registration(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	int sender_id = get_multiplayer()->get_remote_sender_id();

	// 1. 服务器��辑校验 (例如：该队伍是否已满＄1�7)
	// if (team_is_full(p_team_id)) p_team_id = find_next_available_team();

	// 2. 在服务器本地注册
	int desired_team_id = p_team_id;
	if (desired_team_id == 0) {
		desired_team_id = peer_to_team_map.size() + 1;
	}
	register_player(sender_id, desired_team_id);

	// 3. 广播给所有人：新玩家加入了某阄1�7
	// 这样每个人的 peer_to_team_map 都是同步的1�7
	rpc("rpc_client_on_player_registered", sender_id, desired_team_id);

	// 4. 【额外步骤��将“当前已存在的玩家列表��同步给这个新加入的玩家
	for (const auto& pair : peer_to_team_map) {
		if (pair.first != sender_id) {
			rpc_id(sender_id, "rpc_client_on_player_registered", pair.first, pair.second);
		}
	}
}

// [RPC] 运行在所有客户端丄1�7
void GameManager::rpc_client_on_player_registered(int p_peer_id, int p_team_id) {
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Sync: Player ", p_peer_id, " is on Team ", p_team_id);

	// 如果注册的是本地玩家自己，更斄1�7 SelectionManager 的1�7 team_id
	if (p_peer_id == get_multiplayer()->get_unique_id()) {
		if (selection_manager) {
			selection_manager->set_team_id(p_team_id);
		}
	}
}

// --- 服务器接收到 RPC 后的处理 ---

void GameManager::rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos) {
	// 安全棢�查：只有服务器能跑这段代砄1�7
	if (!get_multiplayer()->is_server()) return;

	// 可以在这里根捄1�7 get_multiplayer()->get_remote_sender_id() 
	// 校验这些 unit_ids 是否真的属于发������1�7
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

void GameManager::broadcast_network_snapshot() {
	if (!unit_manager || !building_manager) return;

	PackedByteArray data;
	int unit_count = (int)unit_manager->units.size();
	int bld_count = (int)building_manager->buildings.size();

	// 重新计算缓冲区大射1�7: 4(unit_count) + unit_data + 4(bld_count) + bld_data
	// 建筑数据包结构：ID(4), HP(4), State(1) = 9 bytes
	data.resize(4 + unit_count * 25 + 4 + bld_count * 9);

	int offset = 0;
	// 1. 写入单位
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
	}

	// 2. 写入建筑 (新增)
	data.encode_s32(offset, bld_count);
	offset += 4;
	for (const auto& pair : building_manager->buildings) {
		const BuildingData& b = pair.second;
		data.encode_s32(offset, b.id);              // 4 bytes
		data.encode_float(offset + 4, b.current_health); // 4 bytes
		data.set(offset + 8, (uint8_t)b.state);      // 1 byte
		offset += 9;
	}

	rpc("rpc_client_receive_snapshot", data);
}

void GameManager::rpc_client_receive_snapshot(const PackedByteArray& p_raw_data) {
	if (!unit_manager || p_raw_data.size() < 4) return;

	int offset = 0;

	// 1. 解析单位
	int unit_count = p_raw_data.decode_s32(offset);
	offset += 4;

	for (int i = 0; i < unit_count; i++) {
		if (offset + 21 > p_raw_data.size()) break;

		int id = p_raw_data.decode_s32(offset);
		float px = p_raw_data.decode_float(offset + 4);
		float py = p_raw_data.decode_float(offset + 8);
		float rot = p_raw_data.decode_float(offset + 12);
		float height = p_raw_data.decode_float(offset + 16);
		uint8_t state = p_raw_data.get(offset + 20);
		offset += 21;

		int idx = unit_manager->get_unit_index_by_id(id);
		if (idx != -1) {
			UnitData& unit = unit_manager->units[idx];

			// 更新插��历叄1�7
			unit.prev_position = unit.next_position;
			unit.prev_rotation = unit.next_rotation;
			unit.prev_height = unit.next_height;

			// 设置新的目标
			unit.position = Vector2(px, py);
			unit.next_position = Vector2(px, py);
			unit.next_rotation = rot;
			unit.next_height = height;
			unit.state = (UnitState)state;
		}
	}

	// 2. 解析建筑 (新增)
	if (offset + 4 <= p_raw_data.size()) {
		int bld_count = p_raw_data.decode_s32(offset);
		offset += 4;

		for (int i = 0; i < bld_count; i++) {
			if (offset + 9 > p_raw_data.size()) break;

			int id = p_raw_data.decode_s32(offset);
			float health = p_raw_data.decode_float(offset + 4);
			uint8_t state = p_raw_data.get(offset + 8);
			offset += 9;

			if (building_manager->buildings.count(id)) {
				BuildingData& b = building_manager->buildings[id];
				b.current_health = health;
				b.state = (BuildingState)state;
			}
		}
	}

	// 重置插��时间轴
	tick_accumulator = 0.0;
}

// 1. 单位生成
void GameManager::rpc_server_request_spawn_unit(String p_type, Vector2 p_pos, int p_team) {
	if (!is_server_authority()) return;
	// 服务器生成单位，获取 ID
	int new_id = unit_manager->spawn_unit_by_type(p_type, p_pos, p_team);
	if (new_id != -1) {
		// 广播给所有人（包括发起��）
		rpc("rpc_client_spawn_unit", new_id, p_type, p_pos, p_team);
	}
}

void GameManager::rpc_client_spawn_unit(int p_id, String p_type, Vector2 p_pos, int p_team) {
	selection_manager->on_unit_despawned(p_id);
}

// 2. 建筑生成
void GameManager::rpc_server_request_place_building(String p_type, Vector2i p_grid_pos, int p_team) {
	if (!is_server_authority()) return;

	Ref<BuildingStats> stats = building_manager->get_building_stats_by_type(p_type);
	if (stats.is_null()) return;

	// 1. 先检查钱够不够并扣费
	if (economy_manager->try_spend(p_team, stats->get_cost())) {
		// 2. 扣费成功再执行放罄1�7
		int new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team);
		if (new_id != -1) {
			rpc("rpc_client_spawn_building", new_id, p_type, p_grid_pos, p_team);
			sync_resources_to_client(p_team); // 同步新余预1�7
		}
	}
	else {
		// 3. (可��1�7) 如果钱不够，可以给客户端发一个��金钱不足��的提示 RPC
	}

}

void GameManager::rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team) {
	if (is_server_authority()) return;
	// 客户端放置建筑，并强制使甄1�7 ID，同时内部会自动更新本地 FlowField
	building_manager->place_building_by_type(p_type, p_grid_pos, p_team, p_id);
}

// 3. 锢�毁��辑
void GameManager::rpc_client_despawn_unit(int p_id) {
	unit_manager->despawn_unit(p_id, selection_manager);
}

void GameManager::rpc_client_remove_building(int p_id) {
	building_manager->remove_building(p_id, selection_manager);
}

void GameManager::rpc_server_request_produce_unit(int p_building_id, String p_unit_type) {
	if (!get_multiplayer()->is_server()) return;

	// 1. 获取发����的 Peer ID
	int sender_id = get_multiplayer()->get_remote_sender_id();

	// 2. 找到评1�7 Peer 对应的1�7 Team ID
	int sender_team;
	if (sender_id <= 1) {
		// 说明是主机（Server）自己在操作
		sender_team = selection_manager->get_team_id();
	}
	else {
		// 说明是远程客户端在操作，去我们维护的映射表里柄1�7
		if (peer_to_team_map.find(sender_id) != peer_to_team_map.end()) {
			sender_team = peer_to_team_map[sender_id];
		}
		else {
			return; // 未识别的玩家
		}
	}

	// 3. 校验：该建筑的归属权是否属于发����所在的队伍
	if (building_manager->get_building_team_id(p_building_id) != sender_team) {
		UtilityFunctions::print("Peer ", sender_id, " (Team ", sender_team,
			") tried to use building ", p_building_id,
			" belonging to Team ", building_manager->get_building_team_id(p_building_id));
		return;
	}

	// 4. 执行逻辑（扣费��加入生产队列）
	Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(p_unit_type);
	if (economy_manager->try_spend(sender_team, u_stats->get_cost())) {
		building_manager->add_unit_to_production_queue(p_building_id, p_unit_type);
		sync_resources_to_client(sender_team);
	}
}

void GameManager::rpc_client_sync_resources(int p_team_id, double p_amount) {
	// 只有当同步的是玩家自己的队伍时才更新本地缓存
	if (p_team_id == selection_manager->get_team_id()) {
		// 更新本地 EconomyManager 副本（用亄1�7 UI 显示＄1�7
		economy_manager->set_balance(p_team_id, p_amount);

		// 发信号给 GDScript UI
		// emit_signal("resources_updated", p_amount);
	}
}

void GameManager::sync_resources_to_client(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	double current_gold = economy_manager->get_balance(p_team_id);

	// 假设你定义了 rpc_client_sync_resources (AUTHORITY, RELIABLE)
	// 在实际多玩家环境中，你需要根捄1�7 team_id 找到对应的1�7 PeerID 发��1�7
	// 这里箢�单演示：全员广播（客户端会根据自己的 team_id 过滤或服务器按需发��）
	rpc("rpc_client_sync_resources", p_team_id, current_gold);
}

// 信号的具体实玄1�7
void GameManager::_on_move_requested(PackedInt32Array p_ids, Vector2 p_pos) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器，直接调甄1�7 UnitManager 逻辑
		if (unit_manager) unit_manager->command_units_to_move(p_ids, p_pos);
	}
	else {
		// 如果是客户端，��过 RPC 发��给服务噄1�7 (ID 1 永远是服务器)
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
		// 如果是客户端，发逄1�7 RPC 给服务器 (ID 丄1�7 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_grid_pos, p_team_id);
	}
}

void godot::GameManager::_on_spawn_unit_requested(String p_type_name, Vector2 p_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器（比如单机测试或主机），直接进入处理逻辑
		rpc_server_request_spawn_unit(p_type_name, p_pos, p_team_id);
	}
	else {
		// 如果是客户端，发逄1�7 RPC 给服务器 (ID 丄1�7 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_pos, p_team_id);
	}
}

void godot::GameManager::_on_despawn_unit_requested(int p_unit_id) {
	unit_manager->despawn_unit(p_unit_id, selection_manager);
}

void GameManager::_on_despawn_building_requested(int p_bid) {
	// 无论是服务器还是客户端，丢�旄1�7 DYING 结束，就从内存中移除
	building_manager->remove_building(p_bid, selection_manager);
}

void GameManager::_on_unit_production_requested(int p_bid, String p_type) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器，直接进入��辑
		rpc_server_request_produce_unit(p_bid, p_type);
	}
	else {
		// 如果是客户端，��过 RPC 发��给服务噄1�7
		rpc_id(1, "rpc_server_request_produce_unit", p_bid, p_type);
	}
}

void GameManager::_enter_tree() {
	// 连接信号
	get_multiplayer()->connect("peer_connected", Callable(this, "_on_peer_connected"));
	get_multiplayer()->connect("connected_to_server", Callable(this, "_on_connected_to_server"));
}

// --- 服务器端触发 ---
void GameManager::_on_peer_connected(int p_id) {
	if (!get_multiplayer()->is_server()) return;
	UtilityFunctions::print("Server: Peer connected: ", p_id);
	// 等待客户端主动发 RPC 过来告知兄1�7 TeamID，或者由服务器在这里分配
}

// --- 客户端端触发 ---
void GameManager::_on_connected_to_server() {
	int my_id = get_multiplayer()->get_unique_id();
	UtilityFunctions::print("Client: Connected to server. My ID: ", my_id);

	// 【关键��：主动向服务器请求注册。这里假设客户端知道自己想去哪个队（比如仄1�7 UI 选的＄1�7
	// 如果是自动分配，可以先发丢�丄1�7 0，让服务器决定��1�7
	int desired_team = 0;
	rpc_id(1, "rpc_server_request_registration", desired_team);
}

void GameManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &GameManager::set_unit_manager);
	ClassDB::bind_method(D_METHOD("set_building_manager", "node"), &GameManager::set_building_manager);
	ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &GameManager::set_flow_field_manager);
	ClassDB::bind_method(D_METHOD("set_selection_manager", "node"), &GameManager::set_selection_manager);
	ClassDB::bind_method(D_METHOD("set_group_manager", "node"), &GameManager::set_group_manager);
	ClassDB::bind_method(D_METHOD("set_economy_manager", "node"), &GameManager::set_economy_manager);
	ClassDB::bind_method(D_METHOD("setup_system", "width", "height", "cell_size", "grid_origin"), &GameManager::setup_system);

	ClassDB::bind_method(D_METHOD("host_game", "port"), &GameManager::host_game);
	ClassDB::bind_method(D_METHOD("join_game", "address", "port"), &GameManager::join_game);

	ClassDB::bind_method(D_METHOD("host_start_game"), &GameManager::host_start_game);
	ClassDB::bind_method(D_METHOD("rpc_client_load_game", "scene_path"), &GameManager::rpc_client_load_game);
	ClassDB::bind_method(D_METHOD("rpc_server_request_registration", "team_id"), &GameManager::rpc_server_request_registration);
	ClassDB::bind_method(D_METHOD("rpc_client_on_player_registered", "peer_id", "team_id"), &GameManager::rpc_client_on_player_registered);

	ClassDB::bind_method(D_METHOD("register_player", "peer_id", "team_id"), &GameManager::register_player);

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
	ClassDB::bind_method(D_METHOD("_on_peer_connected", "id"), &GameManager::_on_peer_connected);
	ClassDB::bind_method(D_METHOD("_on_connected_to_server"), &GameManager::_on_connected_to_server);

	ClassDB::bind_method(D_METHOD("get_logic_tick_rate"), &GameManager::get_logic_tick_rate);
	ClassDB::bind_method(D_METHOD("set_logic_tick_rate", "logic_tick_rate"), &GameManager::set_logic_tick_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "logic_tick_rate"), "set_logic_tick_rate", "get_logic_tick_rate");

}