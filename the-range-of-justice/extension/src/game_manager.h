#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/multiplayer_api.hpp>
#include <godot_cpp/classes/multiplayer_peer.hpp>
#include <godot_cpp/classes/e_net_multiplayer_peer.hpp>
#include <godot_cpp/classes/scene_tree.hpp>

#include "unit_manager.h"
#include "building_manager.h"
#include "flow_field_manager.h"
#include "selection_manager.h"
#include "group_manager.h"
#include "unit_loader.h"

namespace godot {

	class GameManager : public Node2D {
		GDCLASS(GameManager, Node2D)

	private:
		UnitManager* unit_manager = nullptr;
		BuildingManager* building_manager = nullptr;
		FlowFieldManager* flow_field_manager = nullptr;
		SelectionManager* selection_manager = nullptr;
		GroupManager* group_manager = nullptr;

		bool is_setup = false;

		// 逻辑 Tick 频率 (例如 20Hz)
		double logic_tick_rate = 0.05;
		double tick_accumulator = 0.0;

		// 网络角色标识 (通常利用 Godot 的 MultiplayerAPI 获取)
		bool is_server_authority() {
			return get_multiplayer()->is_server(); // 或者是你定义的角色变量
		}

		int32_t server_port = 7777;
		String server_address = "127.0.0.1";

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
		void setup_system(int p_width, int p_height, Vector2i p_cell_size, Vector2i p_origin);

		// --- 网络管理接口 ---
		void host_game(int p_port);
		void join_game(String p_address, int p_port);

		// --- RPC 指令接口 (客户端发送，服务器执行) ---
		// 在 Godot 4 C++ 中，RPC 函数名通常和普通函数一样，但在绑定时指定权限
		void rpc_server_receive_move(PackedInt32Array p_ids, Vector2 p_pos);
		void rpc_server_receive_attack_unit(PackedInt32Array p_ids, int p_target_id);

		// 服务器调用：广播所有单位状态
		void broadcast_network_snapshot();

		// 客户端接收：更新本地单位的插值目标
		void rpc_client_receive_snapshot(const PackedByteArray& p_raw_data);

		// 信号回调函数
		void _on_move_requested(PackedInt32Array p_ids, Vector2 p_pos);
		void _on_attack_unit_requested(PackedInt32Array p_ids, int p_target_id);
		void _on_attack_building_requested(PackedInt32Array p_ids, int p_target_id);

		double get_logic_tick_rate() { return logic_tick_rate; }
		void set_logic_tick_rate(double p_value) { logic_tick_rate = p_value; }
	};

}