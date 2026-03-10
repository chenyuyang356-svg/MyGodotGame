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


void GameManager::setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin) {
	unit_manager->set_flow_field_manager(flow_field_manager);
	unit_manager->set_group_manager(group_manager);
	unit_manager->set_attack_manager(attack_manager);

	building_manager->set_flow_field_manager(flow_field_manager);
	building_manager->set_unit_manager(unit_manager);
	building_manager->set_economy_manager(economy_manager);

	selection_manager->set_team_id(peer_to_team_map[get_multiplayer()->get_unique_id()]);

	attack_manager->set_building_manager(building_manager);
	attack_manager->set_projectile_manager(projectile_manager);

	Vector2 map_size = Vector2(p_width, p_height) * Vector2(p_cell_size);
	Vector2 map_pos = Vector2(p_origin) * Vector2(p_cell_size);
	Ref<Texture2D> brush = ResourceLoader::get_singleton()->load("res://asset/fog_brush.tres");
	fog_manager->setup_fog(map_pos, map_size, brush);

	unit_manager->setup_system(p_width, p_height, p_cell_size, p_origin);
	is_setup = true;
}

// 房间/联机会话管理
void GameManager::host_game(int p_port) {
	Ref<ENetMultiplayerPeer> peer;
	peer.instantiate();
	Error err = peer->create_server(p_port);
	if (err != OK) {
		UtilityFunctions::print("Failed to host server.");
		return;
	}
	get_multiplayer()->set_multiplayer_peer(peer);

	//主机自己永远占据 Pear ID 1 和 Team 1
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
	// 建立网络连接 ID 和游戏内 ID 的映射
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Registered Peer: ", p_peer_id, " to Team: ", p_team_id);
}

void GameManager::rpc_client_load_game(const String& p_scene_path) {
	get_tree()->change_scene_to_file(p_scene_path);
}

void GameManager::rpc_server_request_registration(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	int sender_id = get_multiplayer()->get_remote_sender_id();
	int desired_team_id = p_team_id;

	// 如果没有指定队伍，则自动分配队伍
	if (desired_team_id == 0) {
		desired_team_id = peer_to_team_map.size() + 1;
	}
	register_player(sender_id, desired_team_id);

	// 通知玩家注册成功
	rpc("rpc_client_on_player_registered", sender_id, desired_team_id);

	// 增量同步：把已经存在的其他玩家信息发给新加入的玩家
	for (const auto& pair : peer_to_team_map) {
		if (pair.first != sender_id) {
			rpc_id(sender_id, "rpc_client_on_player_registered", pair.first, pair.second);
		}
	}
}

void GameManager::rpc_client_on_player_registered(int p_peer_id, int p_team_id) {
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Sync: Player ", p_peer_id, " is on Team ", p_team_id);

	// 客户端注册成功后初始化阵营 ID
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

	// 预分配内存：
	// 单位头(4) + (每个单位 25 字节) + 建筑头(4) + (每个建筑 9 字节)
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

// 客户端解包
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
		float health = p_raw_data.decode_float(offset + 20);
		uint8_t state = p_raw_data.get(offset + 24);
		offset += 25;

		int idx = unit_manager->get_unit_index_by_id(id);
		if (idx != -1) {
			UnitData& unit = unit_manager->units[idx];

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

	// 2. 解析建筑 
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
		if (peer_to_team_map.find(sender_id) != peer_to_team_map.end()) {
			sender_team = peer_to_team_map[sender_id];
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
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_pos, p_team_id);
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
	rpc_id(1, "rpc_server_request_registration", desired_team);
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

	ClassDB::bind_method(D_METHOD("get_logic_tick_rate"), &GameManager::get_logic_tick_rate);
	ClassDB::bind_method(D_METHOD("set_logic_tick_rate", "logic_tick_rate"), &GameManager::set_logic_tick_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "logic_tick_rate"), "set_logic_tick_rate", "get_logic_tick_rate");

}