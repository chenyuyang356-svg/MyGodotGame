# 代码审查报告与修改方案

> 审查对象：`the-range-of-justice/`（Godot 4.6 + godot-cpp 的 2D 视角 3D 渲染 RTS）
> 审查日期：2026-08-05

---

## 一、项目概览

- **架构**：`the-range-of-justice/` 是主游戏。C++ 扩展（`extension/`）负责核心逻辑（单位/建筑/流场寻路/战斗/网络快照/渲染批次），GDScript 负责输入、UI、场景装配。配置用 `.txt` 键值对加载到 `Resource`。
- **数据流**：`main.gd` → `GameManager::setup_system()` 串联所有 Manager → `_physics_process` 跑服务器逻辑 tick，`_process` 做渲染插值。
- **其他目录**：`vector-field-pathfinding/`、`extension/example/`、`flow_field_manager_gdextension/` 是早期原型，与主游戏无关但混在工作区里。

---

## 二、已确认的 BUG（按优先级）

### P0 — 逻辑错误

| # | 位置 | 问题 | 修复方案 |
|---|------|------|----------|
| 1 | `main/main.gd:121` | `init_cost(coords, 1 if is_near_obstacle else 1, 0)` 恒为 `1`。注释明确要求靠近障碍物（Wall/Sea）的格子代价为 `30`，此处分支写死成 `1`，"避让边缘"功能完全失效 | 改为 `30 if is_near_obstacle else 1` |
| 2 | `autoload/game_manager.gd:3` | `var fog_mode = 3` 越界。`FogMode` 枚举只有 `0/1/2`（None/Light/Heavy），`set_fog_mode(3)` 走到空分支，迷雾状态不可预期 | 默认值改为 `2` |
| 3 | `extension/src/unit_manager.cpp:1692-1725` `set_control_group` | `units[index]` 在 `index == -1`（单位已销毁）时直接访问 `units[-1]`，未定义行为/崩溃点；且函数从未被调用、未被绑定 | 整段删除，或加 `if (index == -1) continue;` 保护 |
| 4 | `extension/src/attack_manager.cpp:916,929,956,685,830` | `apply_damage`/`_execute_attack` 每条子弹命中都 `print` 刷屏，多单位交火时拖垮性能 | 删除或包进 `DebugManager.is_dev_mode` 判断 |

### P1 — 死代码/失效代码

| # | 位置 | 问题 | 处理 |
|---|------|------|------|
| 5 | `main/main.gd:230-269` `spawn_test_units()` | 内部全部 `continue`，还每局调用一次 | 整段删除 |
| 6 | `main/main.gd:167` | `economy_manager.set_balance(2, 5000)` 与下方按 `map_res.initial_gold` 的循环重复 | 删除硬编码行 |
| 7 | `extension/src/game_manager.cpp:499-517` `rpc_client_load_game` | `map_res`、`scene_path` 是死变量，真正切场景靠 `start_game` 信号 + UI_Master | 清理无用代码 |
| 8 | `extension/src/attack_manager.cpp:762` `_execute_building_attack`、`:835` `_process_weapons_logic` | 从未被调用（炮塔实际走 `weapon_mount` + `_execute_weapon_data_attack`） | 删除，连同 `BuildingStats` 中 `attack_damage/attack_range` 等炮塔直连字段 |
| 9 | 控制组功能整体未接线 | `GroupManager::control_groups`、`set_control_group`、`get_control_group_units` 无输入绑定、无 GDScript 调用；`command_units_to_patrol` 同理 | 要么补快捷键接线，要么删除 |
| 10 | `load_available_maps()` 从未被调用 | 地图列表硬编码在 `autoload/game_manager.tscn` 的 `available_maps`；`rpc_client_on_player_registered` 也无人调用 | 二选一：删除或真正接线 |
| 11 | `UnitManager::get_force`（`unit_manager.cpp:784`） | 无调用方 | 删除 |

### P2 — 代码卫生

| # | 位置 | 问题 | 处理 |
|---|------|------|------|
| 12 | `game_manager.cpp:1`、`unit_manager.cpp:1`、`building_manager.cpp:1`、`attack_manager.cpp:1`、`audio_manager.cpp:1` | `.cpp` 里写 `#pragma once` | 删除 |
| 13 | `game_manager.cpp:104-105` | `!is_setup` 判断重复两次 | 合并 |
| 14 | `selection_manager.cpp:157,169` | `if (1 || ...)` 恒真，调试残留 | 清理 |
| 15 | `fog_manager.h:77-78` | 类内成员用 `FogManager::` 限定多余 | 清理 |
| 16 | `unit_manager.cpp:181-186` 中文注释乱码；`building_data.h:2` 用 `#pragma warning(disable : 4828)` 掩盖编码问题 | 文件编码不一致 | 全仓统一 UTF-8 (BOM) |

---

## 三、架构与设计问题

1. **硬编码 `cell_size` 不统一**：真实格子是 `256×256`（`tile_set.tres`），但 `building_manager.cpp:262`、`projectile_manager.cpp:65,264` 写死 `Vector2(32,32)` 兜底，`attack_manager.cpp:46,994` 又写死 `Vector2(256,256)`。
   - 修复：统一用 `flow_field_manager->get_cell_size()`，必要时在 `game_definitions.h` 定义全局常量。

2. **魔法数字分散**：`AIR_HEIGHT_THRESHOLD = 20.0f` 在 `unit_manager.h` 定义，但 `unit_manager.cpp:690,825`、`group_manager.cpp:109`、`attack_manager.cpp:481,562,891` 又各自写死 `20.0f`。
   - 修复：统一引用常量。

3. **配置系统脆弱**：`.txt` 键值加载靠 `stats->set(key, ...)` 反射，拼写错误静默失败（如 `builder.txt` 里 `weapon` 与 `weapon_mount` 两套并存）。
   - 修复：迁移到 `.tres` 资源，或至少加"未识别 key"告警日志。

4. **GDScript 子类与 C++ 职责重叠**：`selection_manager.gd`/`building_manager.gd` 继承 C++ 类再叠加 UI 逻辑，模式合理但多处注释与实现不符（如 `building_manager.gd:48` 注释说 `get_mouse_world_pos` 返回 Vector3，实际返回 Vector2）。
   - 修复：统一注释或类型。

5. **`FOG_UPDATE_INTERVAL = 0.0f`**（`game_manager.h:84`）导致 `update_vision` 每帧全量重绘 512×512 视野贴图。
   - 修复：设为 `0.1s` 左右节流。

---

## 四、网络与安全性（联机关键短板）

1. **RPC 无权限校验**（最严重）：
   - `rpc_server_request_spawn_unit`（`game_manager.cpp:750`）：任意客户端可为任意队伍刷任意单位。
   - `rpc_server_receive_move`（`:544`）：不校验发送者是否拥有这些单位 ID。
   - `rpc_server_request_place_building`（`:764`）：不校验 `p_team` 是否属于发送者。
   - `rpc_server_set_map` / `rpc_server_update_player_settings`：未注册 peer 也可改。
   - 修复：在每个 `rpc_server_*` 开头用 `get_remote_sender_id()` 对照 `players_settings` 做归属校验。

2. **快照全量广播**：`broadcast_network_snapshot` 每 tick（20Hz）把全部单位/建筑无差别发给所有人，无兴趣管理/差分。
   - 修复：加"仅本队可见范围"裁剪，或降低快照频率。

3. **无主机迁移**：房主断线全员踢出（`_on_server_disconnected` 直接 `leave_game`）。
   - 若要做，需选票/交接逻辑。

4. **中途掉线残留**：`_on_peer_disconnected` 只删大厅配置，局内该玩家单位/建筑不清理；且胜利判定只看建筑（`check_victory_conditions` 注释掉了单位检查），一个只剩单位没建筑的队伍会被判负。
   - 修复：明确胜负规则并处理掉线队伍。

---

## 五、性能优化建议

| 位置 | 问题 | 建议 |
|---|---|---|
| `attack_manager.cpp` 命中打印 | 每发子弹 print | 删除/进 debug 分支 |
| `game_manager.h:84` | 迷雾每帧更新 | `FOG_UPDATE_INTERVAL=0.1` |
| `flow_field_manager` `make_all_dirty` | 每 0.5s 全部流场重算 | 只标记受影响格子附近的流场，或按 nav_type 分桶 |
| Dijkstra 全图遍历 | 每目标点全图 8 邻居扩散 | 缓存集成场；对 `use_direct_path` 直行单位跳过 |
| `broadcast_network_snapshot` | 全量快照 | 差分编码 + 裁剪 |
| `update_multimesh_buffer` | 每帧全量重建分组缓存 | 已用分组缓存，可保持；仅建议避免每帧哈希访问 |

---

## 六、工程与仓库卫生

1. **构建产物被提交**：`extension/` 下 136 个文件被 git 跟踪，含 `.obj`、`.sconsign.dblite`、`.vs/`、`.dll`、`.exp`、`.lib` 和 `~*.TMP` 临时文件。
   - 修复：在 `.gitignore` 增加：
     ```
     the-range-of-justice/extension/bin/
     the-range-of-justice/extension/src/*.obj
     the-range-of-justice/extension/.sconsign*
     the-range-of-justice/extension/.vs/
     the-range-of-justice/extension/src/.vs/
     ```

2. **`extension/godot-cpp/` 整目录未管理**（untracked）。
   - 修复：改为 git submodule 或独立仓库。

3. **`managers.gdextension` 所有平台条目都指向 `template_debug` 库**，`windows.template_release.x86_64` 也指向 debug 库——导出 Release 必崩；`SConstruct` 也只生成 debug 副本。
   - 修复：给 SConstruct 加 `target=template_release` 变体（`env.Append(CPPDEFINES=["NDEBUG"])`），并让 `.gdextension` 正确分派。

4. **`release/the_range_of_justice_v0.2.zip` 二进制入库**。
   - 修复：移出或用 LFS。

5. **`project.godot`**：`config/features` 写的是 `"Forward Plus"` 但 `renderer/rendering_method="mobile"`，两者不一致（最近一次提交就是切 mobile）。
   - 修复：只保留 mobile 特性标记。

6. **三个原型工程**（`vector-field-pathfinding`、`extension/example`、`flow_field_manager_gdextension`）混在工作区。
   - 修复：移到独立分支或删除，主仓库只留 `the-range-of-justice/`。

---

## 七、建议实施顺序

| 阶段 | 内容 | 预估 |
|---|---|---|
| **0. 正确性** | 修复 §二 P0/P1 全部 | 半天 |
| **1. 安全** | §四 RPC 校验 + 掉线处理 | 1 天 |
| **2. 性能** | §五 迷雾节流 + 打印清理 + 快照差分 | 1 天 |
| **3. 重构** | §三 常量统一、死代码清除、配置日志 | 1 天 |
| **4. 工程化** | §六 gitignore/子模块/双版本构建/特性标记 | 半天 |

> 建议优先处理阶段 0 中的三处高风险项：
> 1. `main.gd:121` 近障碍代价 bug
> 2. `game_manager.gd` `fog_mode=3` 默认值越界
> 3. `set_control_group` 的 `units[-1]` 越界崩溃点
