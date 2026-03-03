// src/projectile_loader.h
#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include "projectile_stats.h"

namespace godot {
    class ProjectileLoader : public RefCounted {
        GDCLASS(ProjectileLoader, RefCounted)

    protected:
        static void _bind_methods();
        static int _parse_enum(String p_key, String p_value);

    public:
        // 输入 txt 文件路径，返回一个填充好数据的 ProjectileStats 资源
        static Ref<ProjectileStats> load_stats_from_txt(String p_path, Ref<ProjectileStats> p_target = nullptr);
    };

}