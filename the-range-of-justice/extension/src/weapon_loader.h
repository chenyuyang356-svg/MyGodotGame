#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include "weapon_stats.h"

namespace godot {
    class WeaponLoader : public RefCounted {
        GDCLASS(WeaponLoader, RefCounted)

    protected:
        static void _bind_methods();

    public:
        // 解析 txt 文件生成 WeaponStats 资源
        static Ref<WeaponStats> load_stats_from_txt(String p_path, Ref<WeaponStats> p_target = nullptr);
    };
}