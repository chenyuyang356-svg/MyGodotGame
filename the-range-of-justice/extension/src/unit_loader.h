#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include "unit_stats.h" 

namespace godot {

    class UnitLoader : public RefCounted {
        GDCLASS(UnitLoader, RefCounted)

    protected:
        static void _bind_methods();
        //从字符到枚举的转换
        static int _parse_enum(String p_key, String p_value);

    public:
        // 输入 txt 文件路径，返回一个填充好数据的 UnitStats 资源
        static Ref<UnitStats> load_stats_from_txt(String p_path, Ref<UnitStats> p_target = nullptr);
    };

}