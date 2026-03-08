#pragma once

#include <godot_cpp/core/class_db.hpp>

#include "game_manager.h"

using namespace godot;

GameManager::GameManager() {
	// --- 閫氱敤 RPC 閰嶇疆妯℃澘 ---
	Dictionary req_config; // 瀹㈡埛绔彂閫佽姹傜粰鏈嶅姟鍣細ANY_PEER, RELIABLE
	req_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	req_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	req_config["call_local"] = false;

	Dictionary sync_config; // 鏈嶅姟鍣ㄥ悓姝ョ粰瀹㈡埛绔細AUTHORITY, RELIABLE
	sync_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	sync_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	sync_config["call_local"] = false;

	// 缁戝畾鍗曚綅鐩稿叧
	rpc_config("rpc_server_request_spawn_unit", req_config);
	rpc_config("rpc_client_spawn_unit", sync_config);
	rpc_config("rpc_client_despawn_unit", sync_config);

	// 缁戝畾寤虹瓚鐩稿叧
	rpc_config("rpc_server_request_place_building", req_config);
	rpc_config("rpc_client_spawn_building", sync_config);
	rpc_config("rpc_client_remove_building", sync_config);

	// 绉诲姩鎸囦护閰嶇疆
	Dictionary move_config;
	move_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	move_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	move_config["call_local"] = false;
	rpc_config("rpc_server_receive_move", move_config);

	// 鏀诲嚮鎸囦护閰嶇疆
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
	start_config["call_local"] = true; // 涓绘満涔熻鎵ц鍦烘櫙鍒囨崲
	rpc_config("rpc_client_load_game", start_config);

	// 瀹㈡埛绔姹傛敞鍐岋細ANY_PEER (浠讳綍浜哄彲鍙�), RELIABLE
	Dictionary reg_config;
	reg_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_ANY_PEER;
	reg_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_config["call_local"] = false;
	rpc_config("rpc_server_request_registration", reg_config);

	// 鏈嶅姟鍣ㄥ悓姝ユ敞鍐岀粨鏋滅粰鎵€鏈夊鎴风锛欰UTHORITY (浠呮湇鍔″櫒鍙�), RELIABLE
	Dictionary reg_sync_config;
	reg_sync_config["rpc_mode"] = MultiplayerAPI::RPC_MODE_AUTHORITY;
	reg_sync_config["transfer_mode"] = MultiplayerPeer::TRANSFER_MODE_RELIABLE;
	reg_sync_config["call_local"] = true; // 纭繚鏈嶅姟鍣ㄦ湰鍦颁篃鏇存柊 map
	rpc_config("rpc_client_on_player_registered", reg_sync_config);
}

GameManager::~GameManager() {}

void GameManager::_physics_process(double p_delta) {
	if (!unit_manager || !building_manager || !flow_field_manager || !selection_manager || !group_manager || !is_setup) { return; }
	if (!is_setup) return;

	if (is_server_authority()) {
		// --- A. 鏈嶅姟鍣ㄩ€昏緫 ---
		tick_accumulator += p_delta;

		// 1. 杩愯鍗曚綅閫昏緫 (绉诲姩璁＄畻銆佹祦鍦哄簲鐢ㄣ€佺姸鎬佸垏鎹�)
			// 娉ㄦ剰锛氬唴閮ㄤ細灏� position 瀛樺叆 prev_position锛屾柊绠楃殑瀛樺叆 next_position
		unit_manager->update(p_delta);

		// 2. 杩愯寤虹瓚閫昏緫 (寤洪€犺繘搴﹀鍔犮€佺敓浜ч槦鍒楅€昏緫)
		building_manager->update(p_delta);

		// 纭繚閫昏緫鎸夊浐瀹氶鐜囪繍琛� (閫昏緫 Tick)
		while (tick_accumulator >= logic_tick_rate) {

			// 3. 瀹氭湡骞挎挱蹇収 (鍙互姣� Tick 骞挎挱锛屼篃鍙互姣� 2 涓� Tick 骞挎挱)
			broadcast_network_snapshot();

			// 浠ュ悗鏀规垚鏍规嵁闃熶紞鏁伴噺鐨勫惊鐜�
			for (int team_id = 1; team_id < 10; ++team_id) {
				sync_resources_to_client(team_id);
			}

			tick_accumulator -= logic_tick_rate;
		}
	}
	else {
		// --- B. 瀹㈡埛绔€昏緫 ---
		// 瀹㈡埛绔湪 _physics_process 閲岄€氬父鍙礋璐ｆ帴鏀跺寘鍜岀淮鎶ゆ湰鍦拌鏃跺櫒
		// 瀹為檯鐨勫潗鏍囧钩婊戞斁鍦� _process 閲屽仛
		tick_accumulator += p_delta;
	}
}

void GameManager::_process(double p_delta) {
	if (!is_setup) return;

	// 1. 璁＄畻鎻掑€肩郴鏁� alpha (0.0 鍒� 1.0)
	// 瀹冧唬琛ㄥ綋鍓嶆椂闂村浜庝袱涓€昏緫 Tick 涔嬮棿鐨勪綅缃�
	float alpha = UtilityFunctions::clamp(tick_accumulator / logic_tick_rate, 0.0, 1.2);

	// 2. 鎵ц鎻掑€兼覆鏌� (MultiMesh 缁樺埗)
	// 杩欎釜鏂规硶鍐呴儴浼氫娇鐢� lerp(prev_pos, next_pos, alpha)
	unit_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);
	building_manager->update_multimesh_buffer(p_delta, alpha, selection_manager);

	// 3. 杩愯鏈湴鐗规晥 (濡傛姇灏勭墿椋炶銆佺矑瀛愭晥鏋�)
	// 杩欎簺閫氬父涓嶅己姹� Tick 鍚屾锛岄殢甯х巼璺戞洿娴佺晠
	// projectile_manager->update_visuals(p_delta);
}

void GameManager::update_group(double p_delta) {
	group_manager->cleanup_timer += p_delta;
	if (group_manager->cleanup_timer < group_manager->CLEANUP_INTERVAL) return;
	group_manager->cleanup_timer = 0.0;

	auto it = group_manager->temp_groups.begin();
	while (it != group_manager->temp_groups.end()) {
		// 濡傛灉娌℃湁鍗曚綅鍦ㄧЩ鍔ㄤ簡锛屼笖 ID 鍒楄〃涔熺┖浜嗭紙鎴栧崟浣嶉兘鍒拌揪浜嗭級
		// 杩欓噷鍙互鏍规嵁闇€姹傚喅瀹氾細鏄鏁板櫒褰掗浂灏卞垹锛岃繕鏄崟浣嶅畬鍏ㄦ竻绌烘墠鍒�
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
		// 鍦� C++ 涓繛鎺ヤ俊鍙�
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
	// 骞挎挱缁欐墍鏈夊鎴风鍔犺浇娓告垙鍦烘櫙
	rpc("rpc_client_load_game", "res://main/main.tscn");
}

void GameManager::register_player(int p_peer_id, int p_team_id) {
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Registered Peer: ", p_peer_id, " to Team: ", p_team_id);
}

void GameManager::rpc_client_load_game(const String& p_scene_path) {
	// 浣跨敤 Godot 鐨勫満鏅垏鎹㈠姛鑳�
	get_tree()->change_scene_to_file(p_scene_path);
}

// [RPC] 杩愯鍦ㄦ湇鍔″櫒涓�
void GameManager::rpc_server_request_registration(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	int sender_id = get_multiplayer()->get_remote_sender_id();

	// 1. 鏈嶅姟鍣ㄩ€昏緫鏍￠獙 (渚嬪锛氳闃熶紞鏄惁宸叉弧锛�)
	// if (team_is_full(p_team_id)) p_team_id = find_next_available_team();

	// 2. 鍦ㄦ湇鍔″櫒鏈湴娉ㄥ唽
	int desired_team_id = p_team_id;
	if (desired_team_id == 0) {
		desired_team_id = peer_to_team_map.size() + 1;
	}
	register_player(sender_id, desired_team_id);

	// 3. 骞挎挱缁欐墍鏈変汉锛氭柊鐜╁鍔犲叆浜嗘煇闃�
	// 杩欐牱姣忎釜浜虹殑 peer_to_team_map 閮芥槸鍚屾鐨�
	rpc("rpc_client_on_player_registered", sender_id, desired_team_id);

	// 4. 銆愰澶栨楠ゃ€戝皢鈥滃綋鍓嶅凡瀛樺湪鐨勭帺瀹跺垪琛ㄢ€濆悓姝ョ粰杩欎釜鏂板姞鍏ョ殑鐜╁
	for (const auto& pair : peer_to_team_map) {
		if (pair.first != sender_id) {
			rpc_id(sender_id, "rpc_client_on_player_registered", pair.first, pair.second);
		}
	}
}

// [RPC] 杩愯鍦ㄦ墍鏈夊鎴风涓�
void GameManager::rpc_client_on_player_registered(int p_peer_id, int p_team_id) {
	peer_to_team_map[p_peer_id] = p_team_id;
	UtilityFunctions::print("Sync: Player ", p_peer_id, " is on Team ", p_team_id);

	// 濡傛灉娉ㄥ唽鐨勬槸鏈湴鐜╁鑷繁锛屾洿鏂� SelectionManager 鐨� team_id
	if (p_peer_id == get_multiplayer()->get_unique_id()) {
		if (selection_manager) {
			selection_manager->set_team_id(p_team_id);
		}
	}
}

// --- 鏈嶅姟鍣ㄦ帴鏀跺埌 RPC 鍚庣殑澶勭悊 ---

void GameManager::rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos) {
	// 瀹夊叏妫€鏌ワ細鍙湁鏈嶅姟鍣ㄨ兘璺戣繖娈典唬鐮�
	if (!get_multiplayer()->is_server()) return;

	// 鍙互鍦ㄨ繖閲屾牴鎹� get_multiplayer()->get_remote_sender_id() 
	// 鏍￠獙杩欎簺 unit_ids 鏄惁鐪熺殑灞炰簬鍙戦€佽€呫€�
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

	// 閲嶆柊璁＄畻缂撳啿鍖哄ぇ灏�: 4(unit_count) + unit_data + 4(bld_count) + bld_data
	// 寤虹瓚鏁版嵁鍖呯粨鏋勶細ID(4), HP(4), State(1) = 9 bytes
	data.resize(4 + unit_count * 25 + 4 + bld_count * 9);

	int offset = 0;
	// 1. 鍐欏叆鍗曚綅
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

	// 2. 鍐欏叆寤虹瓚 (鏂板)
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

	// 1. 瑙ｆ瀽鍗曚綅
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

			// 鏇存柊鎻掑€煎巻鍙�
			unit.prev_position = unit.next_position;
			unit.prev_rotation = unit.next_rotation;
			unit.prev_height = unit.next_height;

			// 璁剧疆鏂扮殑鐩爣
			unit.position = Vector2(px, py);
			unit.next_position = Vector2(px, py);
			unit.next_rotation = rot;
			unit.next_height = height;
			unit.state = (UnitState)state;
		}
	}

	// 2. 瑙ｆ瀽寤虹瓚 (鏂板)
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

	// 閲嶇疆鎻掑€兼椂闂磋酱
	tick_accumulator = 0.0;
}

// 1. 鍗曚綅鐢熸垚
void GameManager::rpc_server_request_spawn_unit(String p_type, Vector2 p_pos, int p_team) {
	if (!is_server_authority()) return;
	// 鏈嶅姟鍣ㄧ敓鎴愬崟浣嶏紝鑾峰彇 ID
	int new_id = unit_manager->spawn_unit_by_type(p_type, p_pos, p_team);
	if (new_id != -1) {
		// 骞挎挱缁欐墍鏈変汉锛堝寘鎷彂璧疯€咃級
		rpc("rpc_client_spawn_unit", new_id, p_type, p_pos, p_team);
	}
}

void GameManager::rpc_client_spawn_unit(int p_id, String p_type, Vector2 p_pos, int p_team) {
	if (is_server_authority()) return; // 鏈嶅姟鍣ㄥ凡缁忓湪鏈湴鐢熸垚杩囦簡锛岃烦杩�
	// 瀹㈡埛绔娇鐢ㄦ湇鍔″櫒鎸囧畾鐨� ID 鐢熸垚鍗曚綅
	unit_manager->spawn_unit_by_type(p_type, p_pos, p_team, p_id);
}

// 2. 寤虹瓚鐢熸垚
void GameManager::rpc_server_request_place_building(String p_type, Vector2i p_grid_pos, int p_team) {
	if (!is_server_authority()) return;

	Ref<BuildingStats> stats = building_manager->get_building_stats_by_type(p_type);
	if (stats.is_null()) return;

	// 1. 鍏堟鏌ラ挶澶熶笉澶熷苟鎵ｈ垂
	if (economy_manager->try_spend(p_team, stats->get_cost())) {
		// 2. 鎵ｈ垂鎴愬姛鍐嶆墽琛屾斁缃�
		int new_id = building_manager->place_building_by_type(p_type, p_grid_pos, p_team);
		if (new_id != -1) {
			rpc("rpc_client_spawn_building", new_id, p_type, p_grid_pos, p_team);
			sync_resources_to_client(p_team); // 鍚屾鏂颁綑棰�
		}
	}
	else {
		// 3. (鍙€�) 濡傛灉閽变笉澶燂紝鍙互缁欏鎴风鍙戜竴涓€滈噾閽变笉瓒斥€濈殑鎻愮ず RPC
	}

}

void GameManager::rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team) {
	if (is_server_authority()) return;
	// 瀹㈡埛绔斁缃缓绛戯紝骞跺己鍒朵娇鐢� ID锛屽悓鏃跺唴閮ㄤ細鑷姩鏇存柊鏈湴 FlowField
	building_manager->place_building_by_type(p_type, p_grid_pos, p_team, p_id);
}

// 3. 閿€姣侀€昏緫
void GameManager::rpc_client_despawn_unit(int p_id) {
	unit_manager->despawn_unit(p_id, selection_manager);
}

void GameManager::rpc_client_remove_building(int p_id) {
	building_manager->remove_building(p_id, selection_manager);
}

void GameManager::rpc_server_request_produce_unit(int p_building_id, String p_unit_type) {
	if (!get_multiplayer()->is_server()) return;

	// 1. 鑾峰彇鍙戦€佽€呯殑 Peer ID
	int sender_id = get_multiplayer()->get_remote_sender_id();

	// 2. 鎵惧埌璇� Peer 瀵瑰簲鐨� Team ID
	int sender_team;
	if (sender_id <= 1) {
		// 璇存槑鏄富鏈猴紙Server锛夎嚜宸卞湪鎿嶄綔
		sender_team = selection_manager->get_team_id();
	}
	else {
		// 璇存槑鏄繙绋嬪鎴风鍦ㄦ搷浣滐紝鍘绘垜浠淮鎶ょ殑鏄犲皠琛ㄩ噷鏌�
		if (peer_to_team_map.find(sender_id) != peer_to_team_map.end()) {
			sender_team = peer_to_team_map[sender_id];
		}
		else {
			return; // 鏈瘑鍒殑鐜╁
		}
	}

	// 3. 鏍￠獙锛氳寤虹瓚鐨勫綊灞炴潈鏄惁灞炰簬鍙戦€佽€呮墍鍦ㄧ殑闃熶紞
	if (building_manager->get_building_team_id(p_building_id) != sender_team) {
		UtilityFunctions::print("Peer ", sender_id, " (Team ", sender_team,
			") tried to use building ", p_building_id,
			" belonging to Team ", building_manager->get_building_team_id(p_building_id));
		return;
	}

	// 4. 鎵ц閫昏緫锛堟墸璐广€佸姞鍏ョ敓浜ч槦鍒楋級
	Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(p_unit_type);
	if (economy_manager->try_spend(sender_team, u_stats->get_cost())) {
		building_manager->add_unit_to_production_queue(p_building_id, p_unit_type);
		sync_resources_to_client(sender_team);
	}
}

void GameManager::rpc_client_sync_resources(int p_team_id, double p_amount) {
	// 鍙湁褰撳悓姝ョ殑鏄帺瀹惰嚜宸辩殑闃熶紞鏃舵墠鏇存柊鏈湴缂撳瓨
	if (p_team_id == selection_manager->get_team_id()) {
		// 鏇存柊鏈湴 EconomyManager 鍓湰锛堢敤浜� UI 鏄剧ず锛�
		economy_manager->set_balance(p_team_id, p_amount);

		// 鍙戜俊鍙风粰 GDScript UI
		// emit_signal("resources_updated", p_amount);
	}
}

void GameManager::sync_resources_to_client(int p_team_id) {
	if (!get_multiplayer()->is_server()) return;

	double current_gold = economy_manager->get_balance(p_team_id);

	// 鍋囪浣犲畾涔変簡 rpc_client_sync_resources (AUTHORITY, RELIABLE)
	// 鍦ㄥ疄闄呭鐜╁鐜涓紝浣犻渶瑕佹牴鎹� team_id 鎵惧埌瀵瑰簲鐨� PeerID 鍙戦€�
	// 杩欓噷绠€鍗曟紨绀猴細鍏ㄥ憳骞挎挱锛堝鎴风浼氭牴鎹嚜宸辩殑 team_id 杩囨护鎴栨湇鍔″櫒鎸夐渶鍙戦€侊級
	rpc("rpc_client_sync_resources", p_team_id, current_gold);
}

// 淇″彿鐨勫叿浣撳疄鐜�
void GameManager::_on_move_requested(PackedInt32Array p_ids, Vector2 p_pos) {
	if (get_multiplayer()->is_server()) {
		// 濡傛灉鏄湇鍔″櫒锛岀洿鎺ヨ皟鐢� UnitManager 閫昏緫
		if (unit_manager) unit_manager->command_units_to_move(p_ids, p_pos);
	}
	else {
		// 濡傛灉鏄鎴风锛岄€氳繃 RPC 鍙戦€佺粰鏈嶅姟鍣� (ID 1 姘歌繙鏄湇鍔″櫒)
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
		// 鍚屾牱璋冪敤鏀诲嚮鎸囦护
		//unit_manager->command_units_to_attack_target(p_ids, p_target_id);
	}
}

void GameManager::_on_placement_requested(String p_type_name, Vector2i p_grid_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 濡傛灉鏄湇鍔″櫒锛堟瘮濡傚崟鏈烘祴璇曟垨涓绘満锛夛紝鐩存帴杩涘叆澶勭悊閫昏緫
		rpc_server_request_place_building(p_type_name, p_grid_pos, p_team_id);
	}
	else {
		// 濡傛灉鏄鎴风锛屽彂閫� RPC 缁欐湇鍔″櫒 (ID 涓� 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_grid_pos, p_team_id);
	}
}

void godot::GameManager::_on_spawn_unit_requested(String p_type_name, Vector2 p_pos, int p_team_id) {
	if (get_multiplayer()->is_server()) {
		// 濡傛灉鏄湇鍔″櫒锛堟瘮濡傚崟鏈烘祴璇曟垨涓绘満锛夛紝鐩存帴杩涘叆澶勭悊閫昏緫
		rpc_server_request_spawn_unit(p_type_name, p_pos, p_team_id);
	}
	else {
		// 濡傛灉鏄鎴风锛屽彂閫� RPC 缁欐湇鍔″櫒 (ID 涓� 1)
		rpc_id(1, "rpc_server_request_place_building", p_type_name, p_pos, p_team_id);
	}
}

void godot::GameManager::_on_despawn_unit_requested(int p_unit_id) {
	unit_manager->despawn_unit(p_unit_id, selection_manager);
}

void GameManager::_on_despawn_building_requested(int p_bid) {
	// 鏃犺鏄湇鍔″櫒杩樻槸瀹㈡埛绔紝涓€鏃� DYING 缁撴潫锛屽氨浠庡唴瀛樹腑绉婚櫎
	building_manager->remove_building(p_bid, selection_manager);
}

void GameManager::_on_unit_production_requested(int p_bid, String p_type) {
	if (get_multiplayer()->is_server()) {
		// 濡傛灉鏄湇鍔″櫒锛岀洿鎺ヨ繘鍏ラ€昏緫
		rpc_server_request_produce_unit(p_bid, p_type);
	}
	else {
		// 濡傛灉鏄鎴风锛岄€氳繃 RPC 鍙戦€佺粰鏈嶅姟鍣�
		rpc_id(1, "rpc_server_request_produce_unit", p_bid, p_type);
	}
}

void GameManager::_enter_tree() {
	// 杩炴帴淇″彿
	get_multiplayer()->connect("peer_connected", Callable(this, "_on_peer_connected"));
	get_multiplayer()->connect("connected_to_server", Callable(this, "_on_connected_to_server"));
}

// --- 鏈嶅姟鍣ㄧ瑙﹀彂 ---
void GameManager::_on_peer_connected(int p_id) {
	if (!get_multiplayer()->is_server()) return;
	UtilityFunctions::print("Server: Peer connected: ", p_id);
	// 绛夊緟瀹㈡埛绔富鍔ㄥ彂 RPC 杩囨潵鍛婄煡鍏� TeamID锛屾垨鑰呯敱鏈嶅姟鍣ㄥ湪杩欓噷鍒嗛厤
}

// --- 瀹㈡埛绔瑙﹀彂 ---
void GameManager::_on_connected_to_server() {
	int my_id = get_multiplayer()->get_unique_id();
	UtilityFunctions::print("Client: Connected to server. My ID: ", my_id);

	// 銆愬叧閿€戯細涓诲姩鍚戞湇鍔″櫒璇锋眰娉ㄥ唽銆傝繖閲屽亣璁惧鎴风鐭ラ亾鑷繁鎯冲幓鍝釜闃燂紙姣斿浠� UI 閫夌殑锛�
	// 濡傛灉鏄嚜鍔ㄥ垎閰嶏紝鍙互鍏堝彂涓€涓� 0锛岃鏈嶅姟鍣ㄥ喅瀹氥€�
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