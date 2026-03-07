#pragma once

#include <godot_cpp/core/binder_common.hpp>

namespace godot {

    // --- 1. 定义所有枚举 ---

    enum ArmorType {
        ARMOR_LIGHT,
        ARMOR_HEAVY,
        ARMOR_BUILDING,
        ARMOR_HERO
    };

    enum AttackType {
        ATTACK_PHYSICAL,
        ATTACK_MAGIC,
        ATTACK_SIEGE
    };

    enum MoveType {
        MOVE_GROUND,
        MOVE_AIR,
        MOVE_SEA,
        MOVE_HOVER
    };

    enum TargetPriority {
        PRIORITY_CLOSEST,
        PRIORITY_LOWEST_HP,
        PRIORITY_HIGHEST_VALUE
    };

    // 位掩码 (BitField)
    enum UnitTag {
        TAG_NONE = 0,
        TAG_BIOLOGICAL = 1 << 0,
        TAG_MECHANICAL = 1 << 1,
        TAG_SUMMONED = 1 << 2,
        TAG_HERO = 1 << 3
    };

    enum NavigationType {
        NAV_LAND = 0,
        NAV_SEA = 1,
        NAV_HOVER = 2,
        NAV_MAX // 用于计数
    };

    // 单元格元数据定义
    enum CellMetadata {
        CELL_META_NONE = 0,
        CELL_META_RESOURCE = 1 << 0 // 该格子含有资源
    };

    inline Color get_team_color(int p_team_id) {
        switch (p_team_id) {
        case 1: return Color(0.2, 1.0, 0.2); // 绿色
        case 2: return Color(1.0, 0.2, 0.2); // 红色
        case 3: return Color(1.0, 1.0, 0.2); // 黄色
        case 4: return Color(0.2, 0.4, 1.0); // 蓝色
        default: return Color(1, 1, 1);      // 白色
        }
    }
}

// --- 2. 注册转换宏 (必须在 namespace 外面) ---

// 普通枚举用 VARIANT_ENUM_CAST
VARIANT_ENUM_CAST(godot::ArmorType);
VARIANT_ENUM_CAST(godot::AttackType);
VARIANT_ENUM_CAST(godot::MoveType);
VARIANT_ENUM_CAST(godot::TargetPriority);

// 位掩码用 VARIANT_BITFIELD_CAST
VARIANT_BITFIELD_CAST(godot::UnitTag);