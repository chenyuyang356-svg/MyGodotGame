#include "flow_field_manager.h"

#include <queue>

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

FlowFieldManager::FlowFieldManager() {
    width = 0;
    height = 0;
    size = 0;
    grid_origin = Vector2i(0, 0);
    cell_size = Vector2i(0, 0);
}

FlowFieldManager::~FlowFieldManager() {}

void FlowFieldManager::_physics_process(double p_delta) {
    process_one_task();
    
    cleanup_timer += p_delta;
    if (cleanup_timer >= CLEANUP_INTERVAL) {
        cleanup_timer = 0.0;
        cleanup_flow_fields();
    }
}

void FlowFieldManager::process_one_task() {
    // 1. 如果队列为空，直接返回
    if (calculation_queue.empty()) {
        return;
    }

    // 2. 取出队列头部的目标点坐标
    Vector2i target = calculation_queue.front();
    calculation_queue.pop();

    // 3. 检查这个流场是否还存在于哈希表中
    auto it = flow_fields.find(target);
    if (it != flow_fields.end()) {
        FlowField& field = it->second;

        // --- 执行重型计算逻辑 ---

        // 计算各点到目标的代价值 (Dijkstra)
        compute_integration_field(target);

        // 根据代价值生成方向向量
        compute_flow_directions(target);

        // --- 计算完成，更新状态 ---
        field.is_dirty = false;
        field.is_computing = false;
    }
}

void FlowFieldManager::cleanup_flow_fields() {
    // 1. 获取当前系统时间（秒）
    float current_time = Time::get_singleton()->get_ticks_msec() / 1000.0;

    // 2. 安全遍历并删除
    auto it = flow_fields.begin();
    while (it != flow_fields.end()) {
        FlowField& field = it->second;

        // 只有同时满足以下条件才删除：
        // - 没在计算队列中 (is_computing == false)
        // - 距离上次使用时间超过了阈值
        if (!field.is_computing && (current_time - field.last_used_time > UNUSED_THRESHOLD)) {
            // UtilityFunctions::print("正在清理过期的流场，目标点: ", it->first);

            // erase(it) 会返回下一个有效的迭代器，这是 C++ 中安全删除的标志写法
            it = flow_fields.erase(it);
        }
        else {
            // 否则继续指向下一个
            ++it;
        }
    }
}

void FlowFieldManager::setup_grid(int p_width, int p_height, Vector2i p_origin, Vector2i p_cell_size) {
    width = p_width;
    height = p_height;
    size = width * height;
    grid_origin = p_origin;
    cell_size = p_cell_size;

    // 初始化全局地图
    global_cost_map.assign(size, 1);
}

void FlowFieldManager::create_flow_field(Vector2i p_target_grid_pos, bool p_overwrite) {
    Vector2i relative_target_grid_pos = p_target_grid_pos - grid_origin;
    if (relative_target_grid_pos.x < 0 || relative_target_grid_pos.x >= width ||
        relative_target_grid_pos.y < 0 || relative_target_grid_pos.y >= height) {
        return;
    }
    
    // 1. 检查该目标点的流场是否已经存在
    auto it = flow_fields.find(p_target_grid_pos);
    bool exists = (it != flow_fields.end());

    // 2. 如果已存在且不要求覆盖，则直接返回
    if (exists && !p_overwrite) {
        // UtilityFunctions::print("Flow field for ", p_target_grid_pos, " already exists. Skipping.");
        return;
    }

    // 3. 获取或创建流场对象
    // operator[] 会在 key 不存在时自动创建一个默认构造的对象
    FlowField& field = flow_fields[p_target_grid_pos];

    // 4. 初始化数据
    field.target_position = p_target_grid_pos;

    // 调用我们在头文件中定义的 reserve 函数分配空间
    // width 和 height 是在 setup_grid 中设置的成员变量
    field.reserve(width * height);

    // 5. 进行一些基础的默认值填充（可选）
    // 例如：将集成场初始化为极大值
    std::fill(field.integration_field.begin(), field.integration_field.end(), 65535.0f);
    std::fill(field.flow_directions.begin(), field.flow_directions.end(), Vector2(0, 0));
    field.is_computing = true;

    // 如果目标点在地图范围内，将目标点的集成场值设为 0
    int target_idx = relative_target_grid_pos.y * width + relative_target_grid_pos.x;
    if (target_idx >= 0 && target_idx < (width * height)) {
        field.integration_field[target_idx] = 0.0f;
    }

    // UtilityFunctions::print("Created flow field for target: ", p_target_grid_pos);
}

void FlowFieldManager::remove_flow_field(Vector2i p_target_grid_pos) {
    size_t erased_count = flow_fields.erase(p_target_grid_pos);
}

void FlowFieldManager::clear_all_fields() {
    flow_fields.clear();
}

void FlowFieldManager::set_cost(Vector2i p_cell_pos, uint8_t p_cost) {
    // 1. 边界检查：防止索引越界导致程序崩溃
    Vector2i relative_cell_pos = p_cell_pos - grid_origin;
    if (relative_cell_pos.x < 0 || relative_cell_pos.x >= width || relative_cell_pos.y < 0 || relative_cell_pos.y >= height) {
        return;
    }

    // 2. 计算一维数组索引
    int index = relative_cell_pos.y * width + relative_cell_pos.x;

    // 3. 写入全局代价地图
    global_cost_map[index] = p_cost;
}

void FlowFieldManager::compute_integration_field(Vector2i p_target_grid_pos) {
    // 1. 查找对应的流场数据
    auto it = flow_fields.find(p_target_grid_pos);
    if (it == flow_fields.end()) {
        return;
    }

    FlowField& field = it->second;

    // 2. 初始化：将所有格子的集成场设为最大值
    std::fill(field.integration_field.begin(), field.integration_field.end(), 65535.0f);

    // 检查目标点是否越界
    Vector2i relative_target_grid_pos = p_target_grid_pos - grid_origin;
    if (relative_target_grid_pos.x < 0 || relative_target_grid_pos.x >= width ||
        relative_target_grid_pos.y < 0 || relative_target_grid_pos.y >= height) {
        return;
    }

    // 3. 准备 Dijkstra 优先队列
    // 存储结构: Pair<代价, 一维索引>
    // 使用 std::greater 确保它是最小堆（每次弹出代价最小的格子）
    typedef std::pair<float, int> CostIndexPair;
    std::priority_queue<CostIndexPair, std::vector<CostIndexPair>, std::greater<CostIndexPair>> pq;

    // 设置目标点代价为 0 并入队
    int target_idx = relative_target_grid_pos.y * width + relative_target_grid_pos.x;
    field.integration_field[target_idx] = 0.0f;
    pq.push({ 0.0f, target_idx });

    // 4. 开始扩散
    while (!pq.empty()) {
        CostIndexPair current = pq.top();
        pq.pop();

        float current_dist = current.first;
        int current_idx = current.second;

        // 优化：如果弹出的代价已经大于记录的代价，跳过
        if (current_dist > field.integration_field[current_idx]) {
            continue;
        }

        // 获取当前坐标
        int cur_x = current_idx % width;
        int cur_y = current_idx / width;

        // 5. 检查 8 个方向的邻居
        for (int x_off = -1; x_off <= 1; x_off++) {
            for (int y_off = -1; y_off <= 1; y_off++) {
                if (x_off == 0 && y_off == 0) continue; // 跳过自己

                int nx = cur_x + x_off;
                int ny = cur_y + y_off;

                // 边界检查
                if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                    int neighbor_idx = ny * width + nx;

                    // 获取邻居格子的地形代价
                    uint8_t cell_cost = global_cost_map[neighbor_idx];

                    // 如果是墙 (255)，不可通行
                    if (cell_cost == 255) continue;

                    // 计算移动代价：直线为 1.0，对角线为 1.414 (√2)
                    float move_dist = (x_off != 0 && y_off != 0) ? 1.414f : 1.0f;

                    // 邻居的总代价 = 当前格子的总代价 + (移动距离 * 地形权重)
                    float new_dist = current_dist + (move_dist * (float)cell_cost);

                    // 如果找到更短路径，更新并入队
                    if (new_dist < field.integration_field[neighbor_idx]) {
                        field.integration_field[neighbor_idx] = new_dist;
                        pq.push({ new_dist, neighbor_idx });
                    }
                }
            }
        }
    }
}

void FlowFieldManager::compute_flow_directions(Vector2i p_target_grid_pos) {
    auto it = flow_fields.find(p_target_grid_pos);
    if (it == flow_fields.end()) return;

    FlowField& field = it->second;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int current_idx = y * width + x;

            // 如果当前格子本身是墙，方向设为零
            if (global_cost_map[current_idx] == 255) {
                field.flow_directions[current_idx] = Vector2(0, 0);
                continue;
            }

            float current_min = field.integration_field[current_idx];
            Vector2 best_direction(0, 0);

            // 检查 8 个方向的邻居
            for (int x_off = -1; x_off <= 1; x_off++) {
                for (int y_off = -1; y_off <= 1; y_off++) {
                    if (x_off == 0 && y_off == 0) continue;

                    int nx = x + x_off;
                    int ny = y + y_off;

                    // 1. 基础边界检查
                    if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                        int neighbor_idx = ny * width + nx;

                        // 2. 墙壁检查：邻居不能是墙
                        if (global_cost_map[neighbor_idx] == 255) continue;

                        // 3. 墙角检测 (Corner Cutting Prevention)
                        // 如果是寻找对角线邻居 (例如 x_off=1, y_off=1)
                        if (x_off != 0 && y_off != 0) {
                            // 检查侧向的两个格子是否为墙
                            // 例如：要去右下角，必须保证 右边 和 下边 都不是墙
                            int side_neighbor_x = y * width + (x + x_off);
                            int side_neighbor_y = (y + y_off) * width + x;

                            if (global_cost_map[side_neighbor_x] == 255 ||
                                global_cost_map[side_neighbor_y] == 255) {
                                continue; // 只要有一侧是墙，就不允许走对角线
                            }
                        }

                        // 4. 寻找最小值
                        float neighbor_val = field.integration_field[neighbor_idx];
                        if (neighbor_val < current_min) {
                            current_min = neighbor_val;
                            // 方向向量 = 邻居位置 - 当前位置
                            best_direction = Vector2((float)x_off, (float)y_off);
                        }
                    }
                }
            }

            // 归一化方向向量，以便单位移动速度一致
            field.flow_directions[current_idx] = best_direction.normalized();
        }
    }
}

float FlowFieldManager::get_integration(Vector2 p_world_pos, Vector2 p_target_world_pos) {
    Vector2i relative_grid_pos = world_to_grid(p_world_pos) - grid_origin;
    Vector2i target_grid_pos = world_to_grid(p_target_world_pos);

    if (relative_grid_pos.x < 0 || relative_grid_pos.x >= width || relative_grid_pos.y < 0 || relative_grid_pos.y >= height) {
        return -1.0;
    }

    auto it = flow_fields.find(target_grid_pos);
    if (it == flow_fields.end()) {
        return -1.0;
    }

    FlowField& field = it->second;
    field.last_used_time = Time::get_singleton()->get_ticks_msec() / 1000.0;

    if (field.is_dirty && !field.is_computing) {
        calculation_queue.push(target_grid_pos);
        field.is_computing = true;
    }

    int index = relative_grid_pos.y * width + relative_grid_pos.x;

    return field.integration_field[index];
}

Vector2 FlowFieldManager::get_flow_direction(Vector2 p_world_pos, Vector2 p_target_world_pos) {
    Vector2i relative_grid_pos = world_to_grid(p_world_pos) - grid_origin;
    Vector2i target_grid_pos = world_to_grid(p_target_world_pos);
    
    if (relative_grid_pos.x < 0 || relative_grid_pos.x >= width || relative_grid_pos.y < 0 || relative_grid_pos.y >= height) {
        return Vector2(0, 0);
    }

    auto it = flow_fields.find(target_grid_pos);
    if (it == flow_fields.end()) {
        // 如果该目标的流场还没创建，返回零向量
        return Vector2(0, 0);
    }

    FlowField &field = it->second;
    field.last_used_time = Time::get_singleton()->get_ticks_msec() / 1000.0;

    if (field.is_dirty && !field.is_computing) {
        calculation_queue.push(target_grid_pos);
        field.is_computing = true;
    }

    int index = relative_grid_pos.y * width + relative_grid_pos.x;

    return field.flow_directions[index];
}

Vector2i FlowFieldManager::world_to_grid(Vector2 p_world_pos) {
    int32_t gx = (int32_t)Math::floor(p_world_pos.x / (float)(cell_size.x));
    int32_t gy = (int32_t)Math::floor(p_world_pos.y / (float)(cell_size.y));
    return Vector2i(gx, gy);
}

Vector2i FlowFieldManager::world_to_relative(Vector2 p_world_pos) {
    Vector2i grid_pos = world_to_grid(p_world_pos);
    return grid_pos - grid_origin;
}

Vector2i FlowFieldManager::get_grid_origin() {
    return grid_origin;
}

bool FlowFieldManager::is_in_grid(Vector2i p_grid_pos) {
    int rx = p_grid_pos.x - grid_origin.x;
    int ry = p_grid_pos.y - grid_origin.y;
    return (rx >= 0 && rx < width && ry >= 0 && ry < height);
}

// 绑定方法，以便在 GDScript 中调用
void FlowFieldManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup_grid", "width", "height", "grid_origin", "cell_size"), &FlowFieldManager::setup_grid);
    ClassDB::bind_method(
        D_METHOD("create_flow_field", "target_grid_position", "overwrite"),
        &FlowFieldManager::create_flow_field,
        DEFVAL(true) // 默认覆盖
    );
    ClassDB::bind_method(D_METHOD("remove_flow_field", "target_grid_position"), &FlowFieldManager::remove_flow_field);
    ClassDB::bind_method(D_METHOD("clear_all_fields"), &FlowFieldManager::clear_all_fields);
    ClassDB::bind_method(D_METHOD("set_cost", "grid_position", "cost"), &FlowFieldManager::set_cost);
    ClassDB::bind_method(D_METHOD("compute_integration_field", "target_grid_position"), &FlowFieldManager::compute_integration_field);
    ClassDB::bind_method(D_METHOD("compute_flow_directions", "target_grid_position"), &FlowFieldManager::compute_flow_directions);
    ClassDB::bind_method(D_METHOD("get_integration", "world_position", "target_world_position"), &FlowFieldManager::get_integration);
    ClassDB::bind_method(D_METHOD("get_flow_direction", "world_position", "target_world_position"), &FlowFieldManager::get_flow_direction);
    ClassDB::bind_method(D_METHOD("world_to_grid", "world_pos"), &FlowFieldManager::world_to_grid);
}