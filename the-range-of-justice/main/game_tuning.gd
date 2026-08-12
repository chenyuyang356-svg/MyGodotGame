class_name GameTuning
extends Resource

## 集中调参资源。
## 在 Godot 编辑器中双击打开 res://main/game_tuning.tres 即可修改所有可调参数，
## main.gd 在开局时会把这些值写入对应的 C++ Manager。
## 数值含义参考 LOG/LOG_02 的修改报告（网格16px修改报告.md）。
##
## 本文件当前数值已复原为“地图网格改为 16px 之前”（256px 时代）的调参基线，
## 便于从已知良好状态重新调试。量纲标注：【px】世界像素 / 【格】流场格子 /
## 【s】秒 / 【rad】弧度 / 【无】无量纲系数 / 【力】相对力单位（质量本身为相对值）。
##
## 说明：凡是会影响“单位移动逻辑”的硬编码参数都已下沉到这里，
## 便于在不改 C++ 的前提下做手感/寻路/碰撞的整体调优。

# ---------- 单位物理（写入 UnitManager） ----------
@export_group("单位物理 UnitManager")
@export var flow_factor: float = 2000.0          # 流场引导力倍率【无】方向向量×该值=流场力
@export var separation_factor: float = 100.0     # 单位间排斥力倍率【无】重叠比例×该值=排斥力
@export var separation_limit: float = 250.0      # 排斥力上限【力】排斥力向量最大长度
@export var separation_radius_factor: float = 3.0  # 排斥探测半径 = 碰撞半径 × 该值【无】
@export var lateral_separation_factor: float = 1.0 # 侧向排斥系数【无】（遗留，未在移动逻辑中使用）
@export var friction_factor: float = 20.0        # 摩擦力【1/s】每帧从速度中减去 velocity×该值，越大停得越快
@export var force_threshold_squared: float = 0.0 # 力的平方阈值【力²】引导力平方小于该值视为无动力（停止态死区）
@export var velocity_threshold_squared: float = 1.0 # 速度的平方阈值【px²/s²】速度平方小于该值强制置零（防滑行）
@export var desired_integration: float = 0.5     # 期望集成值【无】（遗留，未在移动逻辑中使用）

# ---------- 单位物理扩展（写入 UnitManager） ----------
@export_group("单位物理扩展 UnitManager")
@export var air_height_threshold: float = 20.0    # 空中/地面判定高度阈值【px】高度>该值按空中处理（走 NAV_AIR、不参与地面密度/碰撞/扬尘）
@export var density_limit: float = 4.0            # 路径"人墙"密度阈值【密度】直线探测中格子密度超过该值判定被堵死，切回流场绕行（一格子约能塞下六辆坦克）
@export var density_update_interval: float = 0.5  # 动态密度图重建间隔【s】越小避让越灵敏但越耗性能
@export var idle_density_factor: float = 5.0      # 待机单位密度贡献倍率【无】待机单位占格更久，让移动单位绕行
@export var density_next_cell_factor: float = 0.8 # 移动方向下一格密度贡献系数【无】让"行进前方"也占一点密度，提前避让
@export var arrival_stop_distance_sq: float = 10.0 # 到达死区距离（平方）【px²】距目标小于该值时返回零流场力，防终点横跳
@export var density_avoidance_strength: float = 0.1  # 直线模式密度避让强度【无】（建议 0.05~0.5），越大越早绕开人群
@export var density_avoidance_flow_strength: float = 0.02 # 流场模式密度避让强度【无】只取垂直分量叠加，增强动态绕行感
@export var separation_extra_radius: float = 30.0 # 排斥搜索半径额外余量【px】保证搜索半径能覆盖大型单位
@export var separation_min_dist_factor: float = 1.1 # 排斥理想间距系数【无】理想间距=(双方碰撞半径之和)×该值，>1 单位留缝
@export var sep_idle_vs_idle_k: float = 0.5       # 双方都待机时排斥力权重【无】
@export var sep_idle_vs_moving_k: float = 2.0     # 待机单位推开移动单位的排斥力权重【无】
@export var sep_moving_vs_idle_k: float = 0.1     # 移动单位被待机单位顶开的权重【无】
@export var sep_force_to_total_ratio: float = 0.5 # 分离力计入"总物理力"的比例【无】(0~1)
@export var target_integration_factor: float = 1.1 # 组目标集成值倍率【无】到达判定用，给边缘单位余量
@export var critical_area_integration_margin: float = 3.0 # 临界区判定余量【集成值】集成值<组目标×倍率+该值视为进入终点区，清空阵型偏移
@export var arrival_desired_distance_factor: float = 1.5 # 期望到达距离 = 碰撞半径 × 该值【无】
@export var soft_arrival_factor: float = 1.1      # 软到达半径倍率【无】软到达半径=sqrt(组内已停半径平方和)×该值+自身半径
@export var soft_arrival_neighbor_radius_factor: float = 3.0 # 软到达邻居扫描半径 = 碰撞半径 × 该值【无】
@export var stuck_check_interval: float = 0.5     # 卡住检测间隔【s】不宜过小，要给单位"挤过去"的时间
@export var stuck_threshold_move_factor: float = 0.025 # 卡住位移阈值系数【无】阈值=min(该值×移速, stuck_threshold_min)
@export var stuck_threshold_min: float = 0.5      # 卡住位移阈值下限【px】
@export var stuck_rotation_threshold_factor: float = 0.08 # 卡住转角阈值 = 该值 × 转向速度【无】在转圈也算"没卡住"
@export var stuck_give_up_time: float = 8.0       # 连续无位移超过该秒数判定卡死，放弃并停下【s】
@export var path_recheck_interval: float = 0.8    # 移动中直线路径重查间隔【s】
@export var chase_path_recheck_interval: float = 0.5 # 追击中路径重查间隔【s】
@export var arrival_slow_radius_factor: float = 5.0 # 到达减速半径 = 碰撞半径 × 该值【无】
@export var arrival_min_speed_factor: float = 0.2 # 到达减速后的最小速度比例【无】(0~1)
@export var chase_slow_down_factor: float = 1.5   # 追击减速起始距离 = 停火距离 × 该值【无】
@export var chase_min_speed_factor: float = 0.5   # 追击最小速度比例【无】(0~1)，保持至少 50% 冲刺
@export var rubberband_sensitivity: float = 0.02  # 组内速度"橡皮筋"灵敏度【1/px】每落后 100 像素集成值提速该比例
@export var rubberband_speed_min: float = 0.8     # 橡皮筋速度下限【无】
@export var rubberband_speed_max: float = 1.2     # 橡皮筋速度上限【无】
@export var engine_forward_ratio: float = 0.7     # 引擎方向中"当前朝向"权重【无】(0~1)，0.7=七分向前三分横移
@export var propulsion_force_scale: float = 1000.0 # 推进力总缩放系数【力/px·s⁻²】整体动力强度
@export var propulsion_min_power: float = 0.1     # 推进力基础功率比例【无】原地转身时也能缓慢挪动（0.1=10%）
@export var idle_friction_multiplier: float = 3.0 # 待机/攻击/部署状态摩擦力倍率【无】更快停下防滑行
@export var turn_ramp_angle: float = 0.5          # 转向线速区【rad】角度差小于该值时角速度按比例下降，防到位后摇头
@export var collision_resolve_radius_factor: float = 2.5 # 单位-单位碰撞解算搜索半径 = 碰撞半径 × 该值【无】
@export var idle_resistance: float = 4.0          # 待机/攻击单位阻力倍率【无】质量×该值，移动单位更难推动静止单位
@export var collision_smoothing: float = 0.4      # 重叠修正平滑系数【无】(0~1) 每帧只消除该比例重叠，防瞬移
@export var wall_collision_factor_moving: float = 0.5 # 移动中撞墙回推系数【无】(0~1) 保留滑墙手感
@export var wall_collision_factor_idle: float = 1.0   # 待机撞墙回推系数【无】立即推回防陷入墙体
@export var target_projection_margin: float = 1.0 # 目标点投影到合法格 AABB 边缘的安全边距【px】越小越贴墙

# ---------- 流场（写入 FlowFieldManager） ----------
@export_group("流场 FlowFieldManager")
@export var density_weight: float = 0.3            # 密度代价权重【无】单位密度×该值加入路径代价
@export var density_decay_factor: float = 0.2      # 密度衰减系数【无】(0~1) 每帧旧密度保留比例
@export var max_density_cost: float = 100.0        # 密度代价上限【代价】防止密度代价过大导致乱跑
@export var max_search_dist: int = 60              # 找最近可走格搜索半径【格】
@export var density_blur_radius: int = 1           # 密度模糊半径【格】(0=不模糊)
@export var path_safe_zone_radius: int = 2         # 直线通行检测的目的地安全区【格】忽略密度判定的范围

# ---------- 流场缓存与梯度（写入 FlowFieldManager） ----------
@export_group("流场缓存与梯度 FlowFieldManager")
@export var flow_field_cleanup_interval: float = 2.0 # 流场缓存清理扫描间隔【s】
@export var flow_field_unused_threshold: float = 1.0 # 流场缓存超时时间【s】超过且未再使用则删除
@export var wall_gradient_offset: float = 3.0      # 梯度计算中墙壁/边界格的虚假集成值增量【集成值】越大越远离墙壁
@export var nearest_walkable_cell_limit: int = 1000 # 找最近可走格 BFS 安全阀【格】防全封闭地图卡死

# ---------- 编组（写入 GroupManager） ----------
@export_group("编组 GroupManager")
@export var group_cleanup_interval: float = 1.0   # 失效临时组清理间隔【s】
@export var group_area_margin_factor: float = 1.0 # 阵型所需面积余量【无】乘在单位总面积上
@export var group_radius_estimate_factor: float = 3.0 # 阵型半径估算系数【格】决定采集目标集成值的范围
@export var group_integration_tolerance: float = 1.1 # 组目标集成值容错倍率【无】避免边缘单位频繁切换状态
@export var group_air_height_threshold: float = 20.0 # 编组空中/地面判定高度阈值【px】与 unit_manager.air_height_threshold 保持一致

# ---------- 地形初始化（main.gd） ----------
@export_group("地形初始化 main.gd")
@export var near_obstacle_radius: int = 2          # 近障碍检测半径【格】越大越贴边避让

## 把本资源的值应用到 C++ Manager 节点
func apply_to(unit_manager, flow_field_manager, group_manager) -> void:
	if unit_manager:
		unit_manager.set_flow_factor(flow_factor)
		unit_manager.set_separation_factor(separation_factor)
		unit_manager.set_separation_limit(separation_limit)
		unit_manager.set_separation_radius_factor(separation_radius_factor)
		unit_manager.set_lateral_separation_factor(lateral_separation_factor)
		unit_manager.set_friction_factor(friction_factor)
		unit_manager.set_force_threshold_squared(force_threshold_squared)
		unit_manager.set_velocity_threshold_squared(velocity_threshold_squared)
		unit_manager.set_desired_integration(desired_integration)

		unit_manager.set_air_height_threshold(air_height_threshold)
		unit_manager.set_density_limit(density_limit)
		unit_manager.set_density_update_interval(density_update_interval)
		unit_manager.set_idle_density_factor(idle_density_factor)
		unit_manager.set_density_next_cell_factor(density_next_cell_factor)
		unit_manager.set_arrival_stop_distance_sq(arrival_stop_distance_sq)
		unit_manager.set_density_avoidance_strength(density_avoidance_strength)
		unit_manager.set_density_avoidance_flow_strength(density_avoidance_flow_strength)
		unit_manager.set_separation_extra_radius(separation_extra_radius)
		unit_manager.set_separation_min_dist_factor(separation_min_dist_factor)
		unit_manager.set_sep_idle_vs_idle_k(sep_idle_vs_idle_k)
		unit_manager.set_sep_idle_vs_moving_k(sep_idle_vs_moving_k)
		unit_manager.set_sep_moving_vs_idle_k(sep_moving_vs_idle_k)
		unit_manager.set_sep_force_to_total_ratio(sep_force_to_total_ratio)
		unit_manager.set_target_integration_factor(target_integration_factor)
		unit_manager.set_critical_area_integration_margin(critical_area_integration_margin)
		unit_manager.set_arrival_desired_distance_factor(arrival_desired_distance_factor)
		unit_manager.set_soft_arrival_factor(soft_arrival_factor)
		unit_manager.set_soft_arrival_neighbor_radius_factor(soft_arrival_neighbor_radius_factor)
		unit_manager.set_stuck_check_interval(stuck_check_interval)
		unit_manager.set_stuck_threshold_move_factor(stuck_threshold_move_factor)
		unit_manager.set_stuck_threshold_min(stuck_threshold_min)
		unit_manager.set_stuck_rotation_threshold_factor(stuck_rotation_threshold_factor)
		unit_manager.set_stuck_give_up_time(stuck_give_up_time)
		unit_manager.set_path_recheck_interval(path_recheck_interval)
		unit_manager.set_chase_path_recheck_interval(chase_path_recheck_interval)
		unit_manager.set_arrival_slow_radius_factor(arrival_slow_radius_factor)
		unit_manager.set_arrival_min_speed_factor(arrival_min_speed_factor)
		unit_manager.set_chase_slow_down_factor(chase_slow_down_factor)
		unit_manager.set_chase_min_speed_factor(chase_min_speed_factor)
		unit_manager.set_rubberband_sensitivity(rubberband_sensitivity)
		unit_manager.set_rubberband_speed_min(rubberband_speed_min)
		unit_manager.set_rubberband_speed_max(rubberband_speed_max)
		unit_manager.set_engine_forward_ratio(engine_forward_ratio)
		unit_manager.set_propulsion_force_scale(propulsion_force_scale)
		unit_manager.set_propulsion_min_power(propulsion_min_power)
		unit_manager.set_idle_friction_multiplier(idle_friction_multiplier)
		unit_manager.set_turn_ramp_angle(turn_ramp_angle)
		unit_manager.set_collision_resolve_radius_factor(collision_resolve_radius_factor)
		unit_manager.set_idle_resistance(idle_resistance)
		unit_manager.set_collision_smoothing(collision_smoothing)
		unit_manager.set_wall_collision_factor_moving(wall_collision_factor_moving)
		unit_manager.set_wall_collision_factor_idle(wall_collision_factor_idle)
		unit_manager.set_target_projection_margin(target_projection_margin)
	if flow_field_manager:
		flow_field_manager.set_density_weight(density_weight)
		flow_field_manager.set_density_decay_factor(density_decay_factor)
		flow_field_manager.set_max_density_cost(max_density_cost)
		flow_field_manager.set_max_search_dist(max_search_dist)
		flow_field_manager.set_density_blur_radius(density_blur_radius)
		flow_field_manager.set_path_safe_zone_radius(path_safe_zone_radius)
		flow_field_manager.set_flow_field_cleanup_interval(flow_field_cleanup_interval)
		flow_field_manager.set_flow_field_unused_threshold(flow_field_unused_threshold)
		flow_field_manager.set_wall_gradient_offset(wall_gradient_offset)
		flow_field_manager.set_nearest_walkable_cell_limit(nearest_walkable_cell_limit)
	if group_manager:
		group_manager.set_group_cleanup_interval(group_cleanup_interval)
		group_manager.set_group_area_margin_factor(group_area_margin_factor)
		group_manager.set_group_radius_estimate_factor(group_radius_estimate_factor)
		group_manager.set_group_integration_tolerance(group_integration_tolerance)
		group_manager.set_air_height_threshold(group_air_height_threshold)
