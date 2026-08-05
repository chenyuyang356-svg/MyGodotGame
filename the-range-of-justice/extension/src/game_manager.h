#pragma once

#include <set>
#include <unordered_set>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/templates/hash_map.hpp>


#include "unit_manager.h"
#include "building_manager.h"
#include "flow_field_manager.h"
#include "selection_manager.h"
#include "group_manager.h"
#include "unit_loader.h"
#include "economy_manager.h"
#include "attack_manager.h"
#include "projectile_manager.h"
#include "fog_manager.h"
#include "weapon_manager.h"
#include "effect_manager.h"
#include "audio_manager.h"

namespace godot {

	struct PlayerSettings {
		int team_id = 1;
		int spawn_id = 1;
		String name = "Player";
	};

	class GameManager : public Node2D {
		GDCLASS(GameManager, Node2D)

	private:
		UnitManager* unit_manager = nullptr;
		BuildingManager* building_manager = nullptr;
		FlowFieldManager* flow_field_manager = nullptr;
		SelectionManager* selection_manager = nullptr;
		GroupManager* group_manager = nullptr;
		EconomyManager* economy_manager = nullptr;
		AttackManager* attack_manager = nullptr;
		ProjectileManager* projectile_manager = nullptr;
		FogManager* fog_manager = nullptr;
		WeaponManager* weapon_manager = nullptr;
		EffectManager* effect_manager = nullptr;
		AudioManager* audio_manager = nullptr;

		bool is_setup = false;
		bool game_in_progress = false;

		// 逻辑 Tick 频率 (例如 20Hz)
		double logic_tick_rate = 0.05;
		double tick_accumulator = 0.0;

		// 网络角色标识 (通常利用 Godot 的 MultiplayerAPI 获取)
		bool is_server_authority() {
			return get_multiplayer()->is_server(); // 或者是你定义的角色变量
		}

		int32_t server_port = 7777;
		String server_address = "127.0.0.1";


		// --- 新增地图与大厅数据 ---
		TypedArray<Resource> available_maps;
		int selected_map_index = 0;
		int local_spawn = 0;
		String local_player_name = "Player";

		// 使用 Godot 的 HashMap 方便存储 peer_id -> settings
		HashMap<int, PlayerSettings> players_settings;

		bool game_over = true;
		float game_over_check_timer = 0.0f;
		const float CHECK_INTERVAL = 1.0f; // 每秒检查一次胜负
		void check_victory_conditions();

		float fog_update_timer = 0.0f;
		const float FOG_UPDATE_INTERVAL = 0.0f;

	protected:
		static void _bind_methods();

	public:
		GameManager();
		~GameManager();

		virtual void _physics_process(double p_delta) override;
		virtual void _process(double p_delta) override;

		void update_group(double p_delta);

		void set_unit_manager(Node* p_node);
		void set_building_manager(Node* p_node);
		void set_flow_field_manager(Node* p_node);
		void set_selection_manager(Node* p_node);
		void set_group_manager(Node* p_node);
		void set_economy_manager(Node* p_node);
		void set_attack_manager(Node* p_node);
		void set_projectile_manager(Node* p_node);
		void set_fog_manager(Node* p_node);
		void set_weapon_manager(Node* p_node);
		void set_effect_manager(Node* p_node);
		void set_audio_manager(Node* p_node);
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);

		// --- 网络管理接口 ---
		void host_game(int p_port);
		void join_game(String p_address, int p_port);
		void leave_game();
		void reset_game_state();

		void rpc_client_load_game(int p_map_idx, Dictionary p_player_configs);
		void rpc_server_request_registration(int p_team_id, String p_name);
		void rpc_client_on_player_registered(int p_peer_id, int p_team_id);
		void host_start_game(); // 主机点击“开始游戏”时调用

		void register_player(int p_peer_id, int p_team_id, String p_name);
		void set_available_maps(TypedArray<Resource> p_maps) { available_maps = p_maps; }
		TypedArray<Resource> get_available_maps() { return available_maps; }
		void load_available_maps();

		void rpc_server_set_map(int p_index);
		void rpc_server_update_player_settings(int p_team, int p_spawn);
		void rpc_client_sync_lobby(int p_map_idx, Dictionary p_all_settings);

		Dictionary get_all_player_settings() {
			Dictionary res;
			for (const auto& E : players_settings) {
				Dictionary d;
				d["team"] = E.value.team_id;
				d["spawn"] = E.value.spawn_id;
				d["name"] = E.value.name;
				res[E.key] = d;
			}
			return res;
		}

		// --- RPC 指令接口 (客户端发送，服务器执行) ---
		// 在 Godot 4 C++ 中，RPC 函数名通常和普通函数一样，但在绑定时指定权限
		void rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos);
		void rpc_server_receive_attack_unit(PackedInt32Array p_ids, int p_target_id, bool p_target_is_building);

		// 服务器调用：广播所有单位状态
		void broadcast_network_snapshot();

		// 客户端接收：更新本地单位的插值目标
		void rpc_client_receive_snapshot(const PackedByteArray& p_raw_data);

		// 客户端调用：请求产生单位
		void rpc_server_request_spawn_unit(String p_type, Vector2 p_pos, int p_team);
		// 服务器调用：通知所有客户端产生单位（包含服务器分配的唯一ID）
		void rpc_client_spawn_unit(int p_id, String p_type, Vector2 p_pos, int p_team);
		// 服务器调用：通知所有客户端销毁单位
		void rpc_client_despawn_unit(int p_id);

		// --- 建筑同步 ---
		// 客户端调用：请求放置建筑
		void rpc_server_request_place_building(String p_type, Vector2i p_grid_pos, int p_team, int p_force_id,
			bool p_is_pre_placed, PackedInt32Array p_builder_ids = PackedInt32Array());
		// 服务器调用：同步建筑生成及 ID
		void rpc_client_spawn_building(int p_id, String p_type, Vector2i p_grid_pos, int p_team, bool p_is_pre_placed);
		// 服务器调用：同步移除建筑
		void rpc_client_remove_building(int p_id);

		void rpc_server_request_produce_unit(int p_building_id, String p_unit_type);

		void rpc_client_spawn_projectile(
			const String& p_type_name,
			Vector2 p_start_pos, float p_start_height,
			int p_target_id, bool p_target_is_building, float p_target_height,
			int p_source_id, bool p_source_is_building,
			float p_weapon_damage);

		void rpc_client_sync_resources(int p_team_id, double p_amount); // 仅同步给对应的客户端
		void sync_resources_to_client(int p_team_id);

		// RPC：服务端通知客户端游戏结束
		void rpc_client_notify_game_over(int p_winner_team);

		// 信号回调函数
		void _on_move_requested(PackedInt32Array p_ids, Vector2 p_pos);
		void _on_attack_unit_requested(PackedInt32Array p_ids, int p_target_id);
		void _on_attack_building_requested(PackedInt32Array p_ids, int p_target_id);

		void _on_placement_requested(String p_type_name, Vector2i p_grid_pos, int p_team_id, int p_forced_id, bool p_is_pre_placed);
		void _on_spawn_unit_requested(String p_type_name, Vector2 p_pos, int p_team_id);

		void _on_despawn_unit_requested(int p_unit_id);

		void _on_despawn_building_requested(int p_bid);

		void _on_spawn_projectile_requested(const String& p_type_name,
			Vector2 p_start_pos, float p_start_height,
			int p_target_id, bool p_target_is_building, float p_target_height,
			int p_source_id, bool p_source_is_building,
			float p_weapon_damage);

		void _on_unit_production_requested(int p_bid, String p_type);

		void _enter_tree() override;
		void _on_peer_connected(int p_id);
		void _on_connected_to_server();

		void _on_peer_disconnected(int p_id); // 新增：玩家断开连接回调
		void _on_server_disconnected(); // 新增：服务端关闭回调
		void _on_connection_failed(); // 新增：连接被拒绝/失败回调

		double get_logic_tick_rate() { return logic_tick_rate; }
		void set_logic_tick_rate(double p_value) { logic_tick_rate = p_value; }

		int get_selected_map_index() { return selected_map_index; }
		void set_selected_map_index(int p_value) { selected_map_index = p_value; }

		int get_local_spawn() const { return local_spawn; }
		void set_local_spawn(int p_value) { local_spawn = p_value; }

		String get_local_player_name() const { return local_player_name; }
		void set_local_player_name(const String& p_name) { local_player_name = p_name; }

		void start_game() { game_over = false; }
	};

}