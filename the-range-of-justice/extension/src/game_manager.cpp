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
}

GameManager::~GameManager() {}

void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager || !is_setup) { return; }
	if (!is_setup) return;

	if (is_server_authority()) {
		// --- A. 服务器逻辑 ---
		tick_accumulator += p_delta;

		// 1. 运行单位逻辑 (移动计算、流场应用、状态切换)
			// 注意：内部会将 position 存入 prev_position，新算的存入 next_position
		unit_manager->physics_update(p_delta);

		// 2. 运行建筑逻辑 (建造进度增加、生产队列逻辑)
		

		// 确保逻辑按固定频率运行 (逻辑 Tick)
		while (tick_accumulator >= logic_tick_rate) {

			// 3. 定期广播快照 (可以每 Tick 广播，也可以每 2 个 Tick 广播)
			broadcast_network_snapshot();

			// 以后改成根据队伍数量的循环
			for (int team_id = 1; team_id < 10; ++team_id) {
				sync_resources_to_client(team_id);
			}

			tick_accumulator -= logic_tick_rate;
		}
	}
	else {
		// --- B. 客户端逻辑 ---
		// 客户端在 _physics_process 里通常只负责接收包和维护本地计时器
		// 实际的坐标平滑放在 _process 里做
		tick_accumulator += p_delta;
	}
}

void GameManager::_process(double p_delta) {
	if (!is_setup) return;

	unit_manager->update(p_delta);
	building_manager->update(p_delta);

	// 1. 计算插值系数 alpha (0.0 到 1.0)
	// 它代表当前时间处于两个逻辑 Tick 之间的位置
	float alpha = UtilityFunctions::clamp(tick_accumulator / logic_tick_rate, 0.0, 1.2);

	// 2. 执行插值渲染 (MultiMesh 绘制)
	// 这个方法内部会使用 lerp(prev_pos, next_pos, alpha)
	unit_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	building_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);

	// 3. 运行本地特效 (如投射物飞行、粒子效果)
	// 这些通常不强求 Tick 同步，随帧率跑更流畅
	// projectile_manager->update_visuals(p_delta);
}

void GameManager::update_group(double p_delta) {
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->CLEANUP_INTERVAL) return;
	group_manager->cleanup_timer = 0.0;

	auto it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		// 如果没有单位在移动了，且 ID 列表也空了（或单位都到达了）
		// 这里可以根据需求决定：是计数器归零就删，还是单位完全清空才删
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
	}
}

void GameManager::set_flow_field_manager(Node* p_node) {
	flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void GameManager::set_selection_manager(Node* p_node) {
	selection_manager = Object::cast_to<SelectionManager>(p_node);

	if (selection_manager) {
		// 在 C++ 中连接信号
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

// --- 服务器接收到 RPC 后的处理 ---

void GameManager::rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos) {
	// 安全检查：只有服务器能跑这段代码
	if (!get_multiplayer()->is_server()) return;

	// 可以在这里根据 get_multiplayer()->get_remote_sender_id() 
	// 校验这些 unit_ids 是否真的属于发送者。
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
	if (!unit_manager || unit_manager->units.empty()) return;

	PackedByteArray data;
	int unit_count = (int)unit_manager->units.size();

	// 预分配空间：4字节(计数) + 每个单位字节
	data.resize(4 + unit_count * 25);

	// 写入单位数量
	data.encode_s32(0, unit_count);

	int offset = 4;
	for (const auto& unit : unit_manager->units) {
		data.encode_s32(offset, unit.id);           // 4 bytes
		data.encode_float(offset + 4, unit.position.x); // 4 bytes
		data.encode_float(offset + 8, unit.position.y); // 4 bytes
		data.encode_float(offset + 12, unit.rotation);  // 4 bytes
		data.encode_float(offset + 16, unit.height);  // 4 bytes
		data.encode_float(offset + 20, unit.current_health);  // 4 bytes
		data.set(offset + 24, (uint8_t)unit.state);     // 1 byte
		offset += 25;
	}

	// 广播二进制流
	rpc("rpc_client_receive_snapshot", data);
}

void GameManager::rpc_client_receive_snapshot(const PackedByteArray& p_raw_data) {
	if (!unit_manager || p_raw_data.size() < 4) return;

	int unit_count = p_raw_data.decode_s32(0);
	int offset = 4;

	for (int i = 0; i < unit_count; i++) {
		if (offset + 21 > p_raw_data.size()) break;

		int id = p_raw_data.decode_s32(offset);
		float px = p_raw_data.decode_float(offset + 4);
		float py = p_raw_data.decode_float(offset + 8);
		float rot = p_raw_data.decode_float(offset + 12);
		float height = p_raw_data.decode_float(offset + 16);
		float health = p_raw_data.decode_float(offset + 20);
		uint8_t state = p_raw_data.get(offset + 24);
		offset += 25;

		int idx = unit_manager->get_unit_index_by_id(id);
		if (idx != -1) {
			UnitData& unit = unit_manager->units[idx];

			// 更新插值历史
			unit.prev_position = unit.next_position;
			unit.prev_rotation = unit.next_rotation;
			unit.prev_height = unit.next_height;

			// 设置新的目标
			unit.position = Vector2(px, py);
			unit.next_position = Vector2(px, py);
			unit.next_rotation = rot;
			unit.next_height = height;

			unit.current_health = health;
			unit.state = (UnitState)state;
		}
	}

	// 重置插值时间轴
	tick_accumulator = 0.0;
}

// 1. 单位生成
void GameManager::rpc_server_request_spawn_unit(String p_type, Vector2 p_pos, int p_team) {
	if (!is_server_authority()) return;
	// 服务器生成单位，获取 ID
	int new_id = unit_manager->spawn_unit_by_type(p_type, p_pos, p_team);
	if (new_id != -1) {
		// 广播给所有人（包括发起者）
		rpc("rpc_client_spawn_unit", new_id, p_type, p_pos, p_team);
	}
}

void GameManager::rpc_client_spawn_unit(int p_id, String p_type, Vector2 p_pos, int p_team) {
	if (is_server_authority()) return; // 服务器已经在本地生成过了，跳过
	// 客户端使用服务器指定的 ID 生成单位
	unit_manager->spawn_unit_by_type(p_type, p_pos, p_team, p_id);
}

// 2. 建筑生成
void GameManager::rpc_server_request_place_building(String p_type, Vector2i p_grid_pos, int p_team) {
	if (!is_server_authority()) return;

	Ref<BuildingStats> stats = building_manager->get_building_stats_by_type(p_type);
	if (stats.is_null()) return;

	// 1. 先检查钱够不够并扣费
	if (economy_manager->try_spend(p_team, stats->get_cost())) {
		// 2. 扣费成功再执行放置
		int new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team);
		if (new_id != -1) {
			rpc("rpc_client_spawn_building", new_id, p_type, p_grid_pos, p_team);
			sync_resources_to_client(p_team); // 同步新余额
		}
	}
	else {
		// 3. (可选) 如果钱不够，可以给客户端发一个“金钱不足”的提示 RPC
	}

}

void GameManager::rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team) {
	if (is_server_authority()) return;
	// 客户端放置建筑，并强制使用 ID，同时内部会自动更新本地 FlowField
	building_manager->place_building_by_type(p_type, p_grid_pos, p_team, p_id);
}

// 3. 销毁逻辑
void GameManager::rpc_client_despawn_unit(int p_id) {
	selection_manager->on_unit_despawned(p_id);
}

void GameManager::rpc_client_remove_building(int p_id) {
	building_manager->remove_building(p_id, selection_manager);
}

void GameManager::rpc_server_request_produce_unit(int p_building_id, String p_unit_type) {
	if (!get_multiplayer()->is_server()) return;

	// 1. 验证：该玩家是否拥有该建筑？(这里可以通过 get_remote_sender_id 校验)
	if (building_manager->get_building_team_id(p_building_id) != selection_manager->get_team_id()) {
		return;
	}

	int team_id = building_manager->get_building_team_id(p_building_id);
	Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(p_unit_type);

	// 验证钱是否足够
	if (economy_manager->try_spend(team_id, u_stats->get_cost())) {
		building_manager->add_unit_to_production_queue(p_building_id, p_unit_type);
		sync_resources_to_client(team_id);
	}
}

void GameManager::rpc_client_sync_resources(int p_team_id, double p_amount) {
	// 只有当同步的是玩家自己的队伍时才更新本地缓存
	if (p_team_id == selection_manager->get_team_id()) {
		// 更新本地 EconomyManager 副本（用于 UI 显示）
		economy_manager->set_balance(p_team_id, p_amount);

		// 发信号给 GDScript UI
		// emit_signal("resources_updated", p_amount);
	}
}

void GameManager::sync_resources_to_client(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	double current_gold = economy_manager->get_balance(p_team_id);

	// 假设你定义了 rpc_client_sync_resources (AUTHORITY, RELIABLE)
	// 在实际多玩家环境中，你需要根据 team_id 找到对应的 PeerID 发送
	// 这里简单演示：全员广播（客户端会根据自己的 team_id 过滤或服务器按需发送）
	rpc("rpc_client_sync_resources", p_team_id, current_gold);
}

// 信号的具体实现
void GameManager::_on_move_requested(PackedInt32Array p_ids, Vector2 p_pos) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器，直接调用 UnitManager 逻辑
		if (unit_manager) unit_manager->command_units_to_move(p_ids, p_pos);
	}
	else {
		// 如果是客户端，通过 RPC 发送给服务器 (ID 1 永远是服务器)
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
		// 如果是客户端，发送 RPC 给服务器 (ID 为 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_grid_pos, p_team_id);
	}
}

void godot::GameManager::_on_spawn_unit_requested(String p_type_name, Vector2 p_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器（比如单机测试或主机），直接进入处理逻辑
		rpc_server_request_spawn_unit(p_type_name, p_pos, p_team_id);
	}
	else {
		// 如果是客户端，发送 RPC 给服务器 (ID 为 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_pos, p_team_id);
	}
}

void godot::GameManager::_on_despawn_unit_requested(int p_unit_id) {
	unit_manager->despawn_unit(p_unit_id, selection_manager);
}

void GameManager::_on_unit_production_requested(int p_bid, String p_type) {
	if (get_multiplayer()->is_server()) {
		// 如果是服务器，直接进入逻辑
		rpc_server_request_produce_unit(p_bid, p_type);
	}
	else {
		// 如果是客户端，通过 RPC 发送给服务器
		rpc_id(1, "rpc_server_request_produce_unit", p_bid, p_type);
	}
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

	ClassDB::bind_method(D_METHOD("get_logic_tick_rate"), &GameManager::get_logic_tick_rate);
	ClassDB::bind_method(D_METHOD("set_logic_tick_rate", "logic_tick_rate"), &GameManager::set_logic_tick_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "logic_tick_rate"), "set_logic_tick_rate", "get_logic_tick_rate");

}