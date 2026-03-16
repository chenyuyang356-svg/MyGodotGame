#pragma once
#include <godot_cpp/classes/ref_counted.hpp>
#include "building_stats.h"

namespace godot {
    class WeaponManager;

    class BuildingLoader : public RefCounted {
        GDCLASS(BuildingLoader, RefCounted)
    protected:
        static void _bind_methods();
        static Vector2i _parse_vector2i(String p_value);
    public:
        static Ref<BuildingStats> load_from_txt(String p_path, WeaponManager* p_weapon_manager, Ref<BuildingStats> p_target = nullptr);
    };
}