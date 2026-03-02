#include "economy_manager.h"

using namespace godot;

EconomyManager::EconomyManager() {}
EconomyManager::~EconomyManager() {}

void EconomyManager::init_team(int p_team_id, double p_initial_amount) {
    team_balances[p_team_id] = p_initial_amount;
}

void EconomyManager::add_resources(int p_team_id, double p_amount) {
    if (p_amount <= 0.0) return;

    // 如果队伍不存在，会自动创建一个默认值为 0 的条目
    team_balances[p_team_id] += p_amount;
}

bool EconomyManager::try_spend(int p_team_id, double p_amount) {
    if (p_amount < 0.0) return false; // 不允许扣除负数（那叫加钱）

    auto it = team_balances.find(p_team_id);
    if (it != team_balances.end()) {
        if (it->second >= p_amount) {
            it->second -= p_amount;
            return true;
        }
    }
    return false; // 队伍不存在或钱不够
}

double EconomyManager::get_balance(int p_team_id) const {
    auto it = team_balances.find(p_team_id);
    if (it != team_balances.end()) {
        return it->second;
    }
    return 0.0;
}

void EconomyManager::set_balance(int p_team_id, double p_amount) {
    team_balances[p_team_id] = p_amount;
}

bool EconomyManager::has_team(int p_team_id) const {
    return team_balances.find(p_team_id) != team_balances.end();
}

void EconomyManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("init_team", "team_id", "initial_amount"), &EconomyManager::init_team);
    ClassDB::bind_method(D_METHOD("add_resources", "team_id", "amount"), &EconomyManager::add_resources);
    ClassDB::bind_method(D_METHOD("try_spend", "team_id", "amount"), &EconomyManager::try_spend);
    ClassDB::bind_method(D_METHOD("get_balance", "team_id"), &EconomyManager::get_balance);
    ClassDB::bind_method(D_METHOD("set_balance", "team_id", "amount"), &EconomyManager::set_balance);
}