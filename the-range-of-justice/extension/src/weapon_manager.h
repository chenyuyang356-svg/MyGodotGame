#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <unordered_map>
#include <vector>

#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/quad_mesh.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "weapon_stats.h"
#include "weapon_loader.h"

namespace godot {
    class UnitManager;
    class BuildingManager;
    class SelectionManager;
    class FogManager;

    class WeaponManager : public Node3D {
        GDCLASS(WeaponManager, Node3D)

    private:
        static WeaponManager* singleton;
        HashMap<String, Ref<WeaponStats>> weapons_cache;

        FogManager* fog_manager = nullptr;
        Ref<Shader> unit_shader;
        Ref<Shader> shadow_shader;

        // 渲染器映射及分组缓存
        std::unordered_map<WeaponStats*, MultiMeshInstance3D*> weapon_renderers;
        std::unordered_map<WeaponStats*, MultiMeshInstance3D*> weapon_shadow_renderers;
        // 缓存：pair(单位/建筑的索引, 其对应的武器槽位索引)
        std::unordered_map<WeaponStats*, std::vector<std::pair<int, int>>> weapon_grouping_cache;


    protected:
        static void _bind_methods();

    public:
        WeaponManager();
        ~WeaponManager();

        // 供 C++ 其他模块（如 UnitLoader）获取单例实例
        static WeaponManager* get_singleton();

        // 系统初始化：挂载 FogManager 并更新已有材质的战争迷雾参数
        void setup_system(Node* p_fog_manager);

        // 核心渲染更新：提取 UnitManager/BuildingManager 数据进行渲染插值
        void update_multimesh_buffer(double p_delta, float p_alpha, UnitManager* p_unit_mgr, BuildingManager* p_bld_mgr, SelectionManager* p_sel_mgr);

        // 注册单个武器
        void register_weapon(String p_name, String p_path);
        // 一键加载指定目录下所有武器配置文件
        void register_weapons_from_dir(String p_dir_path);
        // 根据名称获取已被注册的武器配置资源
        Ref<WeaponStats> get_weapon(String p_name);
    };
}