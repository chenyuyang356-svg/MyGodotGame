#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include "unit_stats.h" 
#include "weapon_manager.h"

namespace godot {
    class WeaponManager;

    class UnitLoader : public RefCounted {
        GDCLASS(UnitLoader, RefCounted)

    protected:
        static void _bind_methods();

        static int _parse_enum(String p_key, String p_value);
        static int _parse_bitfield(String p_value);

    public:
        // 输入 txt 文件路径，返回一个填充好数据的 UnitStats 资源
        static Ref<UnitStats> load_stats_from_txt(String p_path, WeaponManager* p_weapon_manager, Ref<UnitStats> p_target = nullptr);
    };

}