#pragma once

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <unordered_map>

namespace godot {

    class EconomyManager : public Node2D {
        GDCLASS(EconomyManager, Node2D)

    private:
        // Key: TeamID (int), Value: Current Balance (double)
        std::unordered_map<int, double> team_balances;

    protected:
        static void _bind_methods();

    public:
        EconomyManager();
        ~EconomyManager();

        // --- 核心业务逻辑 ---

        // 为指定队伍初始化资源（通常在游戏开始时由服务器调用）
        void init_team(int p_team_id, double p_initial_amount);

        // 增加资源：用于采集、回收、奖励等
        void add_resources(int p_team_id, double p_amount);

        // 扣除资源：返回 true 表示扣费成功，false 表示余额不足
        // 这是一个原子操作（Atomic-like），检查和扣费在同一个逻辑步内完成
        bool try_spend(int p_team_id, double p_amount);

        // 获取当前余额（供服务器逻辑或本地 UI 查询）
        double get_balance(int p_team_id) const;

        // 强制设置余额（通常用于调试或特殊剧情事件）
        void set_balance(int p_team_id, double p_amount);

        // 检查队伍是否存在
        bool has_team(int p_team_id) const;
    };

} 