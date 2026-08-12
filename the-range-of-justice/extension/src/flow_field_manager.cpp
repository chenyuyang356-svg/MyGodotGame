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

void FlowFieldManager::update(double p_delta) {
    process_one_task();
    
    cleanup_timer += p_delta;
    if (cleanup_timer >= flow_field_cleanup_interval) {
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
    FlowFieldKey key = calculation_queue.front();
    calculation_queue.pop();

    // 3. 检查这个流场是否还存在于哈希表中
    auto it = flow_fields.find(key);
    if (it != flow_fields.end()) {
        FlowField& field = it->second;

        // --- 执行重型计算逻辑 ---

        // 计算各点到目标的代价值 (Dijkstra)
        compute_integration_field(key);

        // 根据代价值生成方向向量
        compute_flow_directions(key);

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
        if (!field.is_computing && (current_time - field.last_used_time > flow_field_unused_threshold)) {
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
    if (is_setup) { return; }
    
    width = p_width;
    height = p_height;
    size = width * height;
    grid_origin = p_origin;
    cell_size = p_cell_size;

    // 初始化全局地图
    for (int i = 0; i < NAV_MAX; i++) {
        init_cost_maps[i].assign(size, 1);
        cost_maps[i].assign(size, 1);
    }

    metadata_grid.assign(size, CELL_META_NONE);
    density_maps[0].assign(size, 0.0f);
    density_maps[1].assign(size, 0.0f);

    is_setup = true;
}

void FlowFieldManager::create_flow_field(Vector2i p_target_grid_pos, int p_nav_type, bool p_overwrite) {
    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) return;

    FlowFieldKey key = { p_target_grid_pos, p_nav_type };
    auto it = flow_fields.find(key);

    if (it != flow_fields.end() && !p_overwrite) return;

    FlowField& field = flow_fields[key];
    field.target_position = p_target_grid_pos;
    field.nav_type = p_nav_type;
    field.reserve(size);

    std::fill(field.integration_field.begin(), field.integration_field.end(), 65535.0f);
    std::fill(field.flow_directions.begin(), field.flow_directions.end(), Vector2(0, 0));

    Vector2i relative_target_grid_pos = p_target_grid_pos - grid_origin;
    int target_idx = relative_target_grid_pos.y * width + relative_target_grid_pos.x;
    if (target_idx >= 0 && target_idx < (width * height)) {
        field.integration_field[target_idx] = 0.0f;
    }

    field.is_computing = true;

    calculation_queue.push(key);
}

void FlowFieldManager::remove_flow_field(Vector2i p_target_grid_pos, int p_nav_type) {
    FlowFieldKey key = { p_target_grid_pos, p_nav_type };
    size_t erased_count = flow_fields.erase(key);
}

void FlowFieldManager::clear_all_fields() {
    flow_fields.clear();
}

void FlowFieldManager::make_all_dirty() {
    // 1. 遍历哈希表中的所有流场
    for (auto& pair : flow_fields) {
        FlowField& field = pair.second;

        // 标记为脏数据
        field.is_dirty = true;

        // 重置计算状态
        // 这样当单位下次调用 get_flow_direction 时，
        // 逻辑会发现 (is_dirty && !is_computing)，从而将其重新放入计算队列
        field.is_computing = false;
    }

    // 2. 清空当前的计算队列
    // 因为队列里的任务是基于旧地图触发的，已经没有意义了
    // 清空后，系统会根据单位当前的查询需求重新按优先级入队
    std::queue<FlowFieldKey> empty_queue;
    std::swap(calculation_queue, empty_queue);
}

void FlowFieldManager::set_cost(Vector2i p_cell_pos, uint8_t p_cost, int p_nav_type) {
    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) return;

    Vector2i relative = p_cell_pos - grid_origin;
    if (relative.x >= 0 && relative.x < width && relative.y >= 0 && relative.y < height) {
        int index = relative.y * width + relative.x;
        cost_maps[p_nav_type][index] = p_cost;
    }
}

void FlowFieldManager::set_init_cost(Vector2i p_cell_pos, uint8_t p_cost, int p_nav_type) {
    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) return;

    Vector2i relative = p_cell_pos - grid_origin;
    if (relative.x >= 0 && relative.x < width && relative.y >= 0 && relative.y < height) {
        int index = relative.y * width + relative.x;
        init_cost_maps[p_nav_type][index] = p_cost;
    }
}

void FlowFieldManager::set_cell_metadata(Vector2i p_grid_pos, uint32_t p_meta_flag, bool p_enabled) {
    Vector2i relative = p_grid_pos - grid_origin;
    if (is_in_grid(p_grid_pos)) {
        int index = relative.y * width + relative.x;
        if (p_enabled) metadata_grid[index] |= p_meta_flag;
        else metadata_grid[index] &= ~p_meta_flag;
    }
}

void FlowFieldManager::compute_integration_field(FlowFieldKey p_key) {
    // 1. 查找对应的流场数据
    auto it = flow_fields.find(p_key);
    if (it == flow_fields.end()) {
        return;
    }

    FlowField& field = it->second;
    const std::vector<uint8_t>& current_cost_map = cost_maps[p_key.nav_type];

    // 2. 初始化：将所有格子的集成场设为最大值
    std::fill(field.integration_field.begin(), field.integration_field.end(), 65535.0f);

    // 检查目标点是否越界
    Vector2i relative_target_grid_pos = p_key.target - grid_origin;
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

    int d_idx = (p_key.nav_type == NAV_AIR) ? 1 : 0;
    const std::vector<float>& current_density = density_maps[d_idx];

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
                    uint8_t cell_cost = current_cost_map[neighbor_idx];

                    // 如果是墙 (255)，不可通行
                    if (cell_cost == 255) continue;

                    // --- 读取动态密度代价 ---
                    float d_cost = current_density[neighbor_idx] * density_weight;
                    d_cost = std::min(d_cost, max_density_cost); // 限制上限

                    // 计算移动代价：直线为 1.0，对角线为 1.414 (√2)
                    float move_dist = (x_off != 0 && y_off != 0) ? 1.414f : 1.0f;
                    float total_cell_cost = (float)cell_cost + d_cost;

                    // 邻居的总代价 = 当前格子的总代价 + (移动距离 * 地形权重)
                    float new_dist = current_dist + (move_dist * total_cell_cost);

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

void FlowFieldManager::compute_flow_directions(FlowFieldKey p_key) {
    if (0){
        auto it = flow_fields.find(p_key);
        if (it == flow_fields.end()) return;

        FlowField& field = it->second;
        const std::vector<uint8_t>& current_cost_map = cost_maps[p_key.nav_type];

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int current_idx = y * width + x;

                // 如果当前格子本身是墙，方向设为零
                if (current_cost_map[current_idx] == 255) {
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
                            if (current_cost_map[neighbor_idx] == 255) continue;

                            // 3. 墙角检测 (Corner Cutting Prevention)
                            // 如果是寻找对角线邻居 (例如 x_off=1, y_off=1)
                            if (x_off != 0 && y_off != 0) {
                                // 检查侧向的两个格子是否为墙
                                // 例如：要去右下角，必须保证 右边 和 下边 都不是墙
                                int side_neighbor_x = y * width + (x + x_off);
                                int side_neighbor_y = (y + y_off) * width + x;

                                if (current_cost_map[side_neighbor_x] == 255 ||
                                    current_cost_map[side_neighbor_y] == 255) {
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
    else {
        auto it = flow_fields.find(p_key);
        if (it == flow_fields.end()) return;

        FlowField& field = it->second;
        const std::vector<uint8_t>& cost_map = cost_maps[p_key.nav_type];

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = y * width + x;

                // 如果当前是墙，没必要计算
                if (cost_map[idx] == 255) {
                    field.flow_directions[idx] = Vector2(0, 0);
                    continue;
                }

                // 获取 8 个邻居的集成值，如果是边界或墙，则使用“当前值 + 较大偏移”来产生排斥感
                auto get_val = [&](int nx, int ny) -> float {
                    if (nx < 0 || nx >= width || ny < 0 || ny >= height) return field.integration_field[idx] + wall_gradient_offset;
                    int n_idx = ny * width + nx;
                    if (cost_map[n_idx] == 255) return field.integration_field[idx] + wall_gradient_offset; // 给墙壁一个虚假的“高地”感
                    return field.integration_field[n_idx];
                    };

                float vNW = get_val(x - 1, y - 1);
                float vN = get_val(x, y - 1);
                float vNE = get_val(x + 1, y - 1);
                float vW = get_val(x - 1, y);
                float vE = get_val(x + 1, y);
                float vSW = get_val(x - 1, y + 1);
                float vS = get_val(x, y + 1);
                float vSE = get_val(x + 1, y + 1);

                // 使用类似 Sobel 算子的加权梯度
                // x 方向：左边 - 右边
                float gx = (vNW + 2 * vW + vSW) - (vNE + 2 * vE + vSE);
                // y 方向：上边 - 下边
                float gy = (vNW + 2 * vN + vNE) - (vSW + 2 * vS + vSE);

                Vector2 dir(gx, gy);

                field.flow_directions[idx] = dir.normalized();
            }
        }
    }
}

// 接收由 UnitManager 算好的精细密度图并进行模糊处理
void FlowFieldManager::inject_density_and_blur(int p_map_idx, const std::vector<float>& p_raw_density) {
    if (p_map_idx < 0 || p_map_idx > 1 || p_raw_density.size() != size) return;

    std::vector<float>& d_map = density_maps[p_map_idx];

    for (int i = 0; i < size; ++i) {
        d_map[i] = d_map[i] * density_decay_factor + p_raw_density[i];
    }

    // 均值模糊（半径 density_blur_radius，0 表示不模糊）
    if (density_blur_radius <= 0) return;
    std::vector<float> temp = d_map;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float sum = 0.0f;
            int count = 0;
            for (int dy = -density_blur_radius; dy <= density_blur_radius; ++dy) {
                int ny = y + dy;
                if (ny < 0 || ny >= height) continue;
                for (int dx = -density_blur_radius; dx <= density_blur_radius; ++dx) {
                    int nx = x + dx;
                    if (nx < 0 || nx >= width) continue;
                    sum += temp[ny * width + nx];
                    count++;
                }
            }
            if (count > 0) d_map[y * width + x] = sum / (float)count;
        }
    }
}

float FlowFieldManager::get_cost(Vector2i p_grid_pos, int p_nav_type) {
    Vector2i relative_grid_pos = p_grid_pos - grid_origin;

    if (relative_grid_pos.x < 0 || relative_grid_pos.x >= width || relative_grid_pos.y < 0 || relative_grid_pos.y >= height) {
        return -1.0;
    }

    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) {
        return -1.0;
    }

    int index = relative_grid_pos.y * width + relative_grid_pos.x;
    return cost_maps[p_nav_type][index];
}

uint32_t FlowFieldManager::get_cell_metadata(Vector2i p_grid_pos) {
    Vector2i relative = p_grid_pos - grid_origin;
    if (is_in_grid(p_grid_pos)) {
        return metadata_grid[relative.y * width + relative.x];
    }
    return 0;
}

void FlowFieldManager::touch_field(Vector2i p_target_grid, int p_nav_type) {
    FlowFieldKey key = { p_target_grid, p_nav_type };
    auto it = flow_fields.find(key);
    if (it != flow_fields.end()) {
        it->second.last_used_time = Time::get_singleton()->get_ticks_msec() / 1000.0;
        // 如果脏了顺便入队
        if (it->second.is_dirty && !it->second.is_computing) {
            calculation_queue.push(key);
            it->second.is_computing = true;
        }
    }
    else {
        // 如果不存在直接创建
        create_flow_field(p_target_grid, p_nav_type, false);
    }
}

const std::vector<float>* FlowFieldManager::get_integration_field_ptr(Vector2i p_target_grid, int p_nav_type) {
    FlowFieldKey key = { p_target_grid, p_nav_type };
    auto it = flow_fields.find(key);
    if (it != flow_fields.end() && !it->second.is_computing) {
        return &it->second.integration_field;
    }
    return nullptr;
}

float FlowFieldManager::get_integration(Vector2 p_world_pos, Vector2 p_target_world_pos, int p_nav_type) {
    Vector2i target_grid = world_to_grid(p_target_world_pos);
    FlowFieldKey key = { target_grid, p_nav_type };

    auto it = flow_fields.find(key);
    if (it == flow_fields.end()) {
        create_flow_field(target_grid, p_nav_type, false);
        return 65535.0f;
    }

    FlowField& field = it->second;
    field.last_used_time = Time::get_singleton()->get_ticks_msec() / 1000.0;

    if (field.is_dirty && !field.is_computing) {
        calculation_queue.push(key);
        field.is_computing = true;
    }

    if (field.is_computing && field.integration_field.empty()) {
        return 65535.0f;
    }

    // --- 核心逻辑修改：处理不可达方块 ---

    // 1. 先确定单位中心点所在的格子坐标和它的原始值
    Vector2i center_grid = world_to_grid(p_world_pos) - grid_origin;
    float center_val = 65535.0f;
    if (center_grid.x >= 0 && center_grid.x < width && center_grid.y >= 0 && center_grid.y < height) {
        center_val = field.integration_field[center_grid.y * width + center_grid.x];
    }

    // 2. 计算插值采样位置
    Vector2 f_relative = (p_world_pos - Vector2(grid_origin * cell_size)) / Vector2(cell_size);
    Vector2 sample_pos = f_relative - Vector2(0.5f, 0.5f);

    int x0 = (int)Math::floor(sample_pos.x);
    int y0 = (int)Math::floor(sample_pos.y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float tx = sample_pos.x - (float)x0;
    float ty = sample_pos.y - (float)y0;

    // 3. 内部安全采样函数：如果格子是障碍物，返回中心值 + 1
    auto get_val_corrected = [&](int gx, int gy) -> float {
        if (gx < 0 || gx >= width || gy < 0 || gy >= height) {
            return center_val + 1.0f;
        }
        float val = field.integration_field[gy * width + gx];
        // 如果该格是不可达（极大值），则平滑处理
        if (val >= 65530.0f) {
            return center_val + 1.0f;
        }
        return val;
        };

    float v00 = get_val_corrected(x0, y0);
    float v10 = get_val_corrected(x1, y0);
    float v01 = get_val_corrected(x0, y1);
    float v11 = get_val_corrected(x1, y1);

    float top = Math::lerp(v00, v10, tx);
    float bottom = Math::lerp(v01, v11, tx);
    return Math::lerp(top, bottom, ty);
}

Vector2 FlowFieldManager::get_flow_direction(Vector2 p_world_pos, Vector2 p_target_world_pos, int p_nav_type) {
    // --- 1. 基础坐标与边界检查 (保留部分) ---
    Vector2i grid_pos = world_to_grid(p_world_pos);
    Vector2i relative_grid_pos = grid_pos - grid_origin;

    if (relative_grid_pos.x < 0 || relative_grid_pos.x >= width || relative_grid_pos.y < 0 || relative_grid_pos.y >= height) {
        return Vector2(0, 0);
    }

    // --- 2. 流场查找与状态维护 (保留并整合部分) ---
    Vector2i target_grid = world_to_grid(p_target_world_pos);
    FlowFieldKey key = { target_grid, p_nav_type };

    auto it = flow_fields.find(key);
    if (it == flow_fields.end()) {
        create_flow_field(target_grid, p_nav_type, false);
        return Vector2(0, 0);
    }

    FlowField& field = it->second;
    field.last_used_time = Time::get_singleton()->get_ticks_msec() / 1000.0;

    // 如果流场标记为脏且未在计算队列中，则重新入队
    if (field.is_dirty && !field.is_computing) {
        calculation_queue.push(key);
        field.is_computing = true;
    }

    // 如果流场正在计算中（尚未完成），先返回当前格子的原始方向（或零），避免空指针
    if (field.is_computing && field.flow_directions.empty()) {
        return Vector2(0, 0);
    }

    // --- 3. 双线性插值采样逻辑 (新增：解决摇摆核心) ---

    // 计算相对于网格起点的浮点坐标 (单位：格)
    // 例如：单位在 (10.5, 20.2)，格子大小 10，则结果为 (1.05, 2.02)
    Vector2 f_relative = (p_world_pos - Vector2(grid_origin * cell_size)) / Vector2(cell_size);

    // 将采样中心偏移半个格子，使得在格中心处权重最高，向四周平滑过渡
    Vector2 sample_pos = f_relative - Vector2(0.5f, 0.5f);

    int x0 = (int)Math::floor(sample_pos.x);
    int y0 = (int)Math::floor(sample_pos.y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    // 计算插值权重 (0.0 ~ 1.0)
    float tx = sample_pos.x - (float)x0;
    float ty = sample_pos.y - (float)y0;

    // 内部安全采样函数
    auto get_dir_safe = [&](int gx, int gy) -> Vector2 {
        if (gx < 0 || gx >= width || gy < 0 || gy >= height) {
            // 如果越界，尝试采样最近的合法格子的方向，或者返回零
            return Vector2(0, 0);
        }
        return field.flow_directions[gy * width + gx];
        };

    // 获取相邻 4 个格子的方向向量
    Vector2 d00 = get_dir_safe(x0, y0);
    Vector2 d10 = get_dir_safe(x1, y0);
    Vector2 d01 = get_dir_safe(x0, y1);
    Vector2 d11 = get_dir_safe(x1, y1);

    // 进行双线性插值 (Bilinear Interpolation)
    Vector2 top = d00.lerp(d10, tx);
    Vector2 bottom = d01.lerp(d11, tx);
    Vector2 final_dir = top.lerp(bottom, ty);

    // 防止在四个方向完全抵消时（如死角）出现零向量报错
    if (final_dir.length_squared() < 0.001f) {
        // 回退到点采样
        int safe_idx = relative_grid_pos.y * width + relative_grid_pos.x;
        return field.flow_directions[safe_idx];
    }

    return final_dir.normalized();
}

Vector2 FlowFieldManager::get_density_gradient(Vector2 p_world_pos, int p_map_idx)
{
    if (p_map_idx < 0 || p_map_idx > 1) return Vector2(0, 0);

    Vector2i grid_pos = world_to_grid(p_world_pos) - grid_origin;
    if (grid_pos.x < 1 || grid_pos.x >= width - 1 || grid_pos.y < 1 || grid_pos.y >= height - 1) {
        return Vector2(0, 0);
    }

    const std::vector<float>& d_map = density_maps[p_map_idx];

    // 使用中心差分法计算梯度
    // gx = 右边密度 - 左边密度
    // gy = 下边密度 - 上边密度
    float gx = d_map[grid_pos.y * width + (grid_pos.x + 1)] - d_map[grid_pos.y * width + (grid_pos.x - 1)];
    float gy = d_map[(grid_pos.y + 1) * width + grid_pos.x] - d_map[(grid_pos.y - 1) * width + grid_pos.x];

    return Vector2(gx, gy);
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

Vector2i FlowFieldManager::get_cell_size() {
    return cell_size;
}

bool FlowFieldManager::is_in_grid(Vector2i p_grid_pos) {
    int rx = p_grid_pos.x - grid_origin.x;
    int ry = p_grid_pos.y - grid_origin.y;
    return (rx >= 0 && rx < width && ry >= 0 && ry < height);
}

bool FlowFieldManager::is_path_clear(Vector2 p_start_world, Vector2 p_end_world, int p_nav_type) {
    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) return false;

    Vector2i start_grid = world_to_grid(p_start_world) - grid_origin;
    Vector2i end_grid = world_to_grid(p_end_world) - grid_origin;

    // 使用 DDA 算法遍历直线经过的所有格子
    int x1 = start_grid.x;
    int y1 = start_grid.y;
    int x2 = end_grid.x;
    int y2 = end_grid.y;

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int x = x1;
    int y = y1;
    int n = 1 + dx + dy;
    int x_inc = (x2 > x1) ? 1 : -1;
    int y_inc = (y2 > y1) ? 1 : -1;
    int error = dx - dy;
    dx *= 2;
    dy *= 2;

    const std::vector<uint8_t>& cost_map = cost_maps[p_nav_type];

    for (; n > 0; --n) {
        // 边界检查
        if (x >= 0 && x < width && y >= 0 && y < height) {
            if (cost_map[y * width + x] == 255) {
                return false; // 撞墙了
            }
        }
        else {
            return false; // 出界了
        }

        if (error > 0) {
            x += x_inc;
            error -= dy;
        }
        else if (error < 0) {
            y += y_inc;
            error += dx;
        }
        else { // 刚好经过对角点
            x += x_inc;
            y += y_inc;
            error += dx - dy;
            n--;
        }
    }
    return true;
}

bool FlowFieldManager::is_path_traversable(Vector2 p_start, Vector2 p_end, int p_nav_type, float p_max_density_threshold, bool count_density) {
    if (p_nav_type < 0 || p_nav_type >= NAV_MAX) return false;

    Vector2i start_grid = world_to_grid(p_start) - grid_origin;
    Vector2i end_grid = world_to_grid(p_end) - grid_origin;

    // DDA 算法基础变量
    int x1 = start_grid.x, y1 = start_grid.y;
    int x2 = end_grid.x, y2 = end_grid.y; // 这是目的地格子
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int x = x1, y = y1;
    int n = 1 + dx + dy;
    int x_inc = (x2 > x1) ? 1 : -1, y_inc = (y2 > y1) ? 1 : -1;
    int error = dx - dy;
    dx *= 2; dy *= 2;

    const std::vector<uint8_t>& cost_map = cost_maps[p_nav_type];
    int d_idx = (p_nav_type == NAV_AIR) ? 1 : 0;
    const std::vector<float>& d_map = density_maps[d_idx];

    // 定义忽略密度的“安全区”半径（单位：格子数）
    // 建议设为 2-3，这样单位在最后准备进入阵型位时不会被自己人挡住路径判定
    const int SAFE_ZONE_RADIUS = path_safe_zone_radius;

    for (; n > 0; --n) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            int idx = y * width + x;

            // 1. 静态障碍检查：无论在哪，撞墙绝对不行
            if (cost_map[idx] == 255) return false;

            // 2. 目的地附近安全区判断
            // 如果当前检查的格子坐标 (x, y) 距离目的地坐标 (x2, y2) 很近
            bool is_near_destination = (abs(x - x2) <= SAFE_ZONE_RADIUS && abs(y - y2) <= SAFE_ZONE_RADIUS);

            if (!is_near_destination && count_density) {
                // 3. 动态密度检查：只有在安全区外才检测是否被“人墙”堵死
                if (d_map[idx] > p_max_density_threshold) {
                    return false;
                }
            }
        }
        else {
            return false;
        }

        // DDA 步进逻辑
        if (error > 0) { x += x_inc; error -= dy; }
        else if (error < 0) { y += y_inc; error += dx; }
        else { x += x_inc; y += y_inc; error += dx - dy; n--; }
    }
    return true;
}

Vector2i FlowFieldManager::find_nearest_walkable_cell(Vector2i p_start_grid, int p_nav_type) {
    // 1. 如果起点本身就是可达的，直接返回
    if (get_cost(p_start_grid, p_nav_type) < 255) {
        return p_start_grid;
    }

    // 2. BFS 准备
    std::queue<Vector2i> queue;
    queue.push(p_start_grid);

    // 使用集合记录已访问过的点，防止死循环
    // 提示：在大地图上，可以使用一个局部 bool 数组或在全局网格里加标记位来优化性能
    std::unordered_set<uint64_t> visited;
    auto get_key = [](Vector2i p) { return ((uint64_t)p.x << 32) | (uint32_t)p.y; };
    visited.insert(get_key(p_start_grid));

    // 方向向量：上下左右 (曼哈顿距离)
    const Vector2i dirs[] = { Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0) };

    // 限制搜索半径，防止在全是障碍的极端地图上搜遍全图导致卡死
    const int MAX_SEARCH_DIST = max_search_dist;
    int cells_processed = 0;

    while (!queue.empty()) {
        Vector2i current = queue.front();
        queue.pop();
        cells_processed++;

        // 检查周围 4 个邻居
        for (const Vector2i& dir : dirs) {
            Vector2i neighbor = current + dir;

            // 越界检查
            if (!is_in_grid(neighbor)) continue;

            uint64_t key = get_key(neighbor);
            if (visited.find(key) != visited.end()) continue;

            // 3. 检查是否可达
            if (get_cost(neighbor, p_nav_type) < 255) {
                return neighbor; // 找到第一个，一定是曼哈顿距离最近的
            }

            // 4. 继续向外扩张（如果没超过最大搜索半径）
            int dist = std::abs(neighbor.x - p_start_grid.x) + std::abs(neighbor.y - p_start_grid.y);
            if (dist < MAX_SEARCH_DIST) {
                visited.insert(key);
                queue.push(neighbor);
            }
        }

        // 安全阀
        if (cells_processed > nearest_walkable_cell_limit) break;
    }

    // 如果实在找不到，返回原点
    return p_start_grid;
}

// 绑定方法，以便在 GDScript 中调用
void FlowFieldManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("setup_grid", "width", "height", "grid_origin", "cell_size"), &FlowFieldManager::setup_grid);
    ClassDB::bind_method(
        D_METHOD("create_flow_field", "target_grid_position", "nav_type", "overwrite"),
        &FlowFieldManager::create_flow_field,
        DEFVAL(true) // 默认覆盖
    );
    ClassDB::bind_method(D_METHOD("remove_flow_field", "target_grid_position", "nav_type"), &FlowFieldManager::remove_flow_field);
    ClassDB::bind_method(D_METHOD("clear_all_fields"), &FlowFieldManager::clear_all_fields);
    ClassDB::bind_method(D_METHOD("set_cost", "grid_position", "cost", "nav_type"), &FlowFieldManager::set_cost);
    ClassDB::bind_method(D_METHOD("set_init_cost", "grid_position", "cost", "nav_type"), &FlowFieldManager::set_init_cost);
    ClassDB::bind_method(D_METHOD("get_cost", "grid_position", "nav_type"), &FlowFieldManager::get_cost);
    ClassDB::bind_method(D_METHOD("find_nearest_walkable_cell", "start_grid", "nav_type"), &FlowFieldManager::find_nearest_walkable_cell);
    ClassDB::bind_method(D_METHOD("set_cell_meta_data", "grid_position", "meta_flag", "enabled"), &FlowFieldManager::set_cell_metadata);
    ClassDB::bind_method(D_METHOD("get_integration", "world_position", "target_world_position", "nav_type"), &FlowFieldManager::get_integration);
    ClassDB::bind_method(D_METHOD("get_flow_direction", "world_position", "target_world_position", "nav_type"), &FlowFieldManager::get_flow_direction);
    ClassDB::bind_method(D_METHOD("world_to_grid", "world_pos"), &FlowFieldManager::world_to_grid);
    ClassDB::bind_method(D_METHOD("get_grid_origin"), &FlowFieldManager::get_grid_origin);
    ClassDB::bind_method(D_METHOD("get_cell_size"), &FlowFieldManager::get_cell_size);

    ClassDB::bind_method(D_METHOD("get_density_weight"), &FlowFieldManager::get_density_weight);
    ClassDB::bind_method(D_METHOD("set_density_weight", "weight"), &FlowFieldManager::set_density_weight);

    ClassDB::bind_method(D_METHOD("get_density_decay_factor"), &FlowFieldManager::get_density_decay_factor);
    ClassDB::bind_method(D_METHOD("set_density_decay_factor", "factor"), &FlowFieldManager::set_density_decay_factor);

    ClassDB::bind_method(D_METHOD("get_max_density_cost"), &FlowFieldManager::get_max_density_cost);
    ClassDB::bind_method(D_METHOD("set_max_density_cost", "cost"), &FlowFieldManager::set_max_density_cost);

    ClassDB::bind_method(D_METHOD("get_max_search_dist"), &FlowFieldManager::get_max_search_dist);
    ClassDB::bind_method(D_METHOD("set_max_search_dist", "dist"), &FlowFieldManager::set_max_search_dist);

    ClassDB::bind_method(D_METHOD("get_density_blur_radius"), &FlowFieldManager::get_density_blur_radius);
    ClassDB::bind_method(D_METHOD("set_density_blur_radius", "radius"), &FlowFieldManager::set_density_blur_radius);

    ClassDB::bind_method(D_METHOD("get_path_safe_zone_radius"), &FlowFieldManager::get_path_safe_zone_radius);
    ClassDB::bind_method(D_METHOD("set_path_safe_zone_radius", "radius"), &FlowFieldManager::set_path_safe_zone_radius);

    ClassDB::bind_method(D_METHOD("get_flow_field_cleanup_interval"), &FlowFieldManager::get_flow_field_cleanup_interval);
    ClassDB::bind_method(D_METHOD("set_flow_field_cleanup_interval", "val"), &FlowFieldManager::set_flow_field_cleanup_interval);
    ClassDB::bind_method(D_METHOD("get_flow_field_unused_threshold"), &FlowFieldManager::get_flow_field_unused_threshold);
    ClassDB::bind_method(D_METHOD("set_flow_field_unused_threshold", "val"), &FlowFieldManager::set_flow_field_unused_threshold);
    ClassDB::bind_method(D_METHOD("get_wall_gradient_offset"), &FlowFieldManager::get_wall_gradient_offset);
    ClassDB::bind_method(D_METHOD("set_wall_gradient_offset", "val"), &FlowFieldManager::set_wall_gradient_offset);
    ClassDB::bind_method(D_METHOD("get_nearest_walkable_cell_limit"), &FlowFieldManager::get_nearest_walkable_cell_limit);
    ClassDB::bind_method(D_METHOD("set_nearest_walkable_cell_limit", "val"), &FlowFieldManager::set_nearest_walkable_cell_limit);

    ADD_GROUP("Dynamic Flow Field", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density_weight", PROPERTY_HINT_RANGE, "0.0, 2.0, 0.01"), "set_density_weight", "get_density_weight");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density_decay_factor", PROPERTY_HINT_RANGE, "0.0, 1.0, 0.01"), "set_density_decay_factor", "get_density_decay_factor");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_density_cost", PROPERTY_HINT_RANGE, "0.0, 1000.0, 1.0"), "set_max_density_cost", "get_max_density_cost");

    ADD_GROUP("Path Search", "");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "max_search_dist", PROPERTY_HINT_RANGE, "1, 512, 1"), "set_max_search_dist", "get_max_search_dist");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "density_blur_radius", PROPERTY_HINT_RANGE, "0, 8, 1"), "set_density_blur_radius", "get_density_blur_radius");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "path_safe_zone_radius", PROPERTY_HINT_RANGE, "0, 8, 1"), "set_path_safe_zone_radius", "get_path_safe_zone_radius");

    ADD_GROUP("Flow Field Cache & Gradient", "");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flow_field_cleanup_interval"), "set_flow_field_cleanup_interval", "get_flow_field_cleanup_interval");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flow_field_unused_threshold"), "set_flow_field_unused_threshold", "get_flow_field_unused_threshold");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wall_gradient_offset"), "set_wall_gradient_offset", "get_wall_gradient_offset");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "nearest_walkable_cell_limit"), "set_nearest_walkable_cell_limit", "get_nearest_walkable_cell_limit");
}