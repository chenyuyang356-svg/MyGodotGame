#pragma once

#include <vector>
#include <queue>
#include <unordered_map>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/classes/time.hpp>

namespace godot {

    // 为 Vector2i 提供哈希支持，以便将其用作 unordered_map 的 Key
    struct Vector2iHasher {
        size_t operator()(const Vector2i& v) const {
            // 将两个 32 位整数组合成一个 64 位哈希值
            return ((uint64_t)v.x << 32) | (uint32_t)v.y;
        }
    };

    // 单个流场的数据结构
    struct FlowField {
        bool is_dirty = false;       //dirty指cost_map更新后flow_field没有更新
        bool is_computing = false;       //是否已完成计算
        float last_used_time;       //上次被查询的时间
        Vector2i target_position;           // 该流场的目标网格坐标
        std::vector<float> integration_field; // Dijkstra 算法生成的集成场 (值越小离目标越近)
        std::vector<Vector2> flow_directions; // 最终生成的方向向量数组 (单位查询这个)

        FlowField() = default;

        // 初始化数组大小
        void reserve(int size) {
            integration_field.assign(size, 65535.0f);
            flow_directions.assign(size, Vector2(0, 0));
        }
    };

    class FlowFieldManager : public Node2D {
        GDCLASS(FlowFieldManager, Node2D)

    private:
        int width;       // 地图宽度（格子数）
        int height;      // 地图高度（格子数）
        int size;       //总格子数
        Vector2i grid_origin;       //地图的左上角坐标
        Vector2i cell_size; // 每个格子的尺寸
        std::vector<uint8_t> global_cost_map;      // 障碍物权重 (通常 1 为平地，255 为墙)

        // 哈希表存储：Key 为目标点坐标，Value 为对应的完整流场数据
        std::unordered_map<Vector2i, FlowField, Vector2iHasher> flow_fields;

        std::queue<Vector2i> calculation_queue;

        double cleanup_timer = 0.0;      // 累加时间
        const double CLEANUP_INTERVAL = 2.0; // 每 2 秒扫描一次
        const double UNUSED_THRESHOLD = 10.0; // 超过 10 秒没用就删除

    protected:
        static void _bind_methods();

    public:
        FlowFieldManager();
        ~FlowFieldManager();

        void update(double p_delta);

        void process_one_task();

        void cleanup_flow_fields();

        // --- 基础设置 ---

        // 初始化网格尺寸和配置
        void setup_grid(int p_width, int p_height, Vector2i p_origin, Vector2i p_cell_size);

        // --- 流场生命周期管理 ---

        // 为指定目标点创建一个新流场（如果已存在则重置）
        void create_flow_field(Vector2i p_target_grid_pos, bool p_overwrite = true);

        // 删除特定的流场
        void remove_flow_field(Vector2i p_target_grid_pos);

        // 清空所有流场数据
        void clear_all_fields();

        void make_all_dirty();

        // --- 数据操作与算法 ---

        // 修改特定流场的代价地图（例如动态添加障碍物）
        void set_cost(Vector2i p_cell_pos, uint8_t p_cost);

        // [核心] 计算指定目标的集成场 (Dijkstra/BFS)
        void compute_integration_field(Vector2i p_target_grid_pos);

        // [核心] 计算指定目标的向量方向场 (Gradient)
        void compute_flow_directions(Vector2i p_target_grid_pos);

        // --- 查询接口 (供单位调用) ---

        float get_cost(Vector2i p_grid_pos);

        // 根据世界坐标和目标坐标，获取该位置与目标的距离
        float get_integration(Vector2 p_world_pos, Vector2 p_target_world_pos);

        // 根据世界坐标和目标坐标，获取该位置应有的移动方向向量
        Vector2 get_flow_direction(Vector2 p_world_pos, Vector2 p_target_world_pos);

        // 将世界坐标转换为格点坐标
        Vector2i world_to_grid(Vector2 p_world_pos);

        Vector2i world_to_relative(Vector2 p_world_pos);

        Vector2i get_grid_origin();

        Vector2i get_cell_size();

        bool is_in_grid(Vector2i p_grid_pos);
    };


}