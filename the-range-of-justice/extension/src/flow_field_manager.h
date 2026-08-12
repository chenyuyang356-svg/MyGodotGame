#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector2i.hpp>
#include <godot_cpp/classes/time.hpp>

#include "game_definitions.h"

namespace godot {

    // 复合 Key：用于在哈希表中区分 [目标点 + 移动类型]
    struct FlowFieldKey {
        Vector2i target;
        int nav_type;

        bool operator==(const FlowFieldKey& other) const {
            return target == other.target && nav_type == other.nav_type;
        }
    };

    struct FlowFieldKeyHasher {
        size_t operator()(const FlowFieldKey& k) const {
            // 合并坐标和导航类型的哈希
            size_t h1 = ((uint64_t)k.target.x << 32) | (uint32_t)k.target.y;
            return h1 ^ (size_t)(k.nav_type * 0x9e3779b9);
        }
    };

    // 单个流场的数据结构
    struct FlowField {
        bool is_dirty = false;       //dirty指cost_map更新后flow_field没有更新
        bool is_computing = false;       //是否已完成计算
        float last_used_time;       //上次被查询的时间
        Vector2i target_position;           // 该流场的目标网格坐标
        int nav_type;
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
        // 修改为多层代价地图：每个导航类型对应一个 vector
        std::vector<uint8_t> init_cost_maps[NAV_MAX];
        std::vector<uint8_t> cost_maps[NAV_MAX];

        // 修改哈希表 Key 为 FlowFieldKey
        std::unordered_map<FlowFieldKey, FlowField, FlowFieldKeyHasher> flow_fields;

        // 任务队列也需要包含类型信息
        std::queue<FlowFieldKey> calculation_queue;

        double cleanup_timer = 0.0;      // 累加时间
        float flow_field_cleanup_interval = 2.0f; // 每 2 秒扫描一次 (可调)
        float flow_field_unused_threshold = 1.0f; // 超过 1 秒没用就删除 (可调)
        float wall_gradient_offset = 3.0f;        // 梯度计算中墙壁/边界的虚假集成值增量 (可调)
        int nearest_walkable_cell_limit = 1000;   // 最近可走格 BFS 搜索的安全阀 (可调)

        std::vector<uint32_t> metadata_grid; // 存储每个格子的元数据（位掩码）

        // 0: 地面/水面密度 (Ground/Sea), 1: 空中密度 (Air)
        std::vector<float> density_maps[2];
        float density_weight = 0.3f; // 每个单位产生的额外代价权重
        float density_decay_factor = 0.2f;
        float max_density_cost = 100.0f;  // 动态代价上限，防止单位乱跑

        int max_search_dist = 30;          // 找最近可走格的搜索半径（格）
        int density_blur_radius = 1;       // 密度模糊半径（格，0 表示不模糊）
        int path_safe_zone_radius = 1;     // 直线检测目的地的安全区半径（格）

        bool is_setup = false;

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
        void create_flow_field(Vector2i p_target_grid_pos, int p_nav_type ,bool p_overwrite = true);

        // 删除特定的流场
        void remove_flow_field(Vector2i p_target_grid_pos, int p_nav_type);

        // 清空所有流场数据
        void clear_all_fields();

        void make_all_dirty();

        // --- 数据操作与算法 ---

        // 修改特定流场的代价地图（例如动态添加障碍物）
        void set_cost(Vector2i p_cell_pos, uint8_t p_cost, int p_nav_type);

        void set_init_cost(Vector2i p_cell_pos, uint8_t p_cost, int p_nav_type);

        void set_cell_metadata(Vector2i p_grid_pos, uint32_t p_meta_flag, bool p_enabled);

        // [核心] 计算指定目标的集成场 (Dijkstra/BFS)
        void compute_integration_field(FlowFieldKey p_key);

        // [核心] 计算指定目标的向量方向场 (Gradient)
        void compute_flow_directions(FlowFieldKey p_key);

        void inject_density_and_blur(int p_map_idx, const std::vector<float>& p_raw_density);

        // --- 查询接口 (供单位调用) ---

        float get_cost(Vector2i p_grid_pos, int p_nav_type);

        uint32_t get_cell_metadata(Vector2i p_grid_pos);

        void touch_field(Vector2i p_target_grid, int p_nav_type);
        const std::vector<float>* get_integration_field_ptr(Vector2i p_target_grid, int p_nav_type);

        // 根据世界坐标和目标坐标，获取该位置与目标的距离
        float get_integration(Vector2 p_world_pos, Vector2 p_target_world_pos, int p_nav_type);

        // 根据世界坐标和目标坐标，获取该位置应有的移动方向向量
        Vector2 get_flow_direction(Vector2 p_world_pos, Vector2 p_target_world_pos, int p_nav_type);

        // 根据世界坐标获取密度梯度向量（指向密度高的方向）
        Vector2 get_density_gradient(Vector2 p_world_pos, int p_map_idx);

        // 将世界坐标转换为格点坐标
        Vector2i world_to_grid(Vector2 p_world_pos);

        Vector2i world_to_relative(Vector2 p_world_pos);

        Vector2i get_grid_origin();

        Vector2i get_cell_size();

        int get_width() { return width; }
        int get_height() { return height; }

        bool is_in_grid(Vector2i p_grid_pos);

        bool is_path_clear(Vector2 p_start_world, Vector2 p_end_world, int p_nav_type);
        bool is_path_traversable(Vector2 p_start, Vector2 p_end, int p_nav_type, float p_max_density_threshold, bool count_density = true);

        // 寻找距离目标点最近的、代价不为 255 的格子
        // p_max_range 为搜索半径（单位：格子），防止在全封闭地图中搜索过久
        Vector2i find_nearest_walkable_cell(Vector2i p_start_grid, int p_nav_type);


        void set_density_weight(float p_val) { density_weight = p_val; }
        float get_density_weight() const { return density_weight; }

        void set_density_decay_factor(float p_val) { density_decay_factor = p_val; }
        float get_density_decay_factor() const { return density_decay_factor; }

        void set_max_density_cost(float p_val) { max_density_cost = p_val; }
        float get_max_density_cost() const { return max_density_cost; }

        void set_max_search_dist(int p_val) { max_search_dist = p_val; }
        int get_max_search_dist() const { return max_search_dist; }

        void set_density_blur_radius(int p_val) { density_blur_radius = p_val; }
        int get_density_blur_radius() const { return density_blur_radius; }

        void set_path_safe_zone_radius(int p_val) { path_safe_zone_radius = p_val; }
        int get_path_safe_zone_radius() const { return path_safe_zone_radius; }

        void set_flow_field_cleanup_interval(float p_val) { flow_field_cleanup_interval = p_val; }
        float get_flow_field_cleanup_interval() const { return flow_field_cleanup_interval; }

        void set_flow_field_unused_threshold(float p_val) { flow_field_unused_threshold = p_val; }
        float get_flow_field_unused_threshold() const { return flow_field_unused_threshold; }

        void set_wall_gradient_offset(float p_val) { wall_gradient_offset = p_val; }
        float get_wall_gradient_offset() const { return wall_gradient_offset; }

        void set_nearest_walkable_cell_limit(int p_val) { nearest_walkable_cell_limit = p_val; }
        int get_nearest_walkable_cell_limit() const { return nearest_walkable_cell_limit; }

    };


}