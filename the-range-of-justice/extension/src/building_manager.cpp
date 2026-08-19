#include "attack_manager.h"
#include "building_manager.h"
#include "selection_manager.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/dir_access.hpp>

using namespace godot;

BuildingManager::BuildingManager() {}
BuildingManager::~BuildingManager() {}

void BuildingManager::set_attack_manager(Node* p_node) {
    attack_manager = Object::cast_to<AttackManager>(p_node);
}

void BuildingManager::set_flow_field_manager(Node* p_node) {
    flow_field_manager = Object::cast_to<FlowFieldManager>(p_node);
}

void BuildingManager::set_unit_manager(Node* p_node) {
    unit_manager = Object::cast_to<UnitManager>(p_node);
}

void BuildingManager::set_economy_manager(Node* p_node) {
    economy_manager = Object::cast_to<EconomyManager>(p_node);
}

void BuildingManager::set_fog_manager(Node* p_node) {
    fog_manager = Object::cast_to<FogManager>(p_node);
}

void BuildingManager::set_weapon_manager(Node* p_node) {
    weapon_manager = Object::cast_to<WeaponManager>(p_node);
}

void BuildingManager::setup_system(int p_width, int p_height, Vector2i p_cell_size) {
    _setup_progress_bar_system();
    _gather_resource_positions();
    _setup_minimap_renderer(p_width, p_height, p_cell_size);

    if (fog_manager) {
        for (auto& [s_ptr, mmi] : type_renderers) {
            Ref<ShaderMaterial> mat = mmi->get_material_override();
            if (mat.is_valid()) {
                mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
        }
        for (auto& [s_ptr, s_mmi] : shadow_renderers) {
            Ref<ShaderMaterial> s_mat = s_mmi->get_material_override();
            if (s_mat.is_valid()) {
                s_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                s_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                s_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
        }
        for (auto& [s_ptr, g_mmi] : ghost_renderers) {
            Ref<ShaderMaterial> g_mat = g_mmi->get_material_override();
            if (g_mat.is_valid()) {
                g_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
                g_mat->set_shader_parameter("tex_history", fog_manager->get_history_texture());
                g_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
                g_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
            }
        }
    }
}

void BuildingManager::_setup_progress_bar_system() {
    if (global_progress_bar_renderer) return;

    global_progress_bar_renderer = memnew(MultiMeshInstance3D);
    global_progress_bar_renderer->set_name("GlobalBuildingProgressBars");
    add_child(global_progress_bar_renderer);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_custom_data(true); // X: HP比例, Y: 状态预留
    mm->set_use_colors(true);      // 队伍颜色

    Ref<QuadMesh> mesh;
    mesh.instantiate();
    mesh->set_size(Vector2(1.0, 1.0));
    mm->set_mesh(mesh);

    global_progress_bar_renderer->set_multimesh(mm);

    // 材质设置
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (progress_bar_shader.is_null()) {
        progress_bar_shader = ResourceLoader::get_singleton()->load("res://shader/hp_bar.gdshader");
    }

    if (fog_manager) {
        mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }
    mat->set_shader(progress_bar_shader);

    global_progress_bar_renderer->set_material_override(mat);
}

void BuildingManager::_setup_minimap_renderer(int p_width, int p_height, Vector2i p_cell_size) {
    if (minimap_dot_renderer) return;

    minimap_dot_renderer = memnew(MultiMeshInstance3D);
    minimap_dot_renderer->set_name("MinimapBuildingDots");
    // Layer 2 为小地图专用层
    minimap_dot_renderer->set_layer_mask(2);
    add_child(minimap_dot_renderer);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);

    Ref<QuadMesh> qm;
    qm.instantiate();

    // 计算点的大小
    float map_max_dim = float(std::max(p_width * p_cell_size.x, p_height * p_cell_size.y));
    float dot_size_val = map_max_dim * MINIMAP_DOT_SCALE;

    qm->set_size(Vector2(dot_size_val, dot_size_val));
    mm->set_mesh(qm);
    minimap_dot_renderer->set_multimesh(mm);

    // 材质设置
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (minimap_dot_shader.is_null()) {
        minimap_dot_shader = ResourceLoader::get_singleton()->load("res://shader/minimap_dot.gdshader");
    }
    mat->set_shader(minimap_dot_shader);

    if (fog_manager) {
        mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    minimap_dot_renderer->set_material_override(mat);
}

void BuildingManager::_gather_resource_positions() {
    if (!flow_field_manager || resources_gathered) return;

    cached_resource_positions.clear();
    int w = flow_field_manager->get_width();
    int h = flow_field_manager->get_height();
    Vector2i cell_sz = flow_field_manager->get_cell_size();
    Vector2i origin = flow_field_manager->get_grid_origin();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            Vector2i gp = origin + Vector2i(x, y);
            // 检查元数据是否包含资源标记
            if (flow_field_manager->get_cell_metadata(gp) & 1 /* CELL_META_RESOURCE */) {
                // 计算世界坐标中心
                Vector2 pos = Vector2(gp.x * cell_sz.x, gp.y * cell_sz.y) + Vector2(cell_sz) * 0.5f;
                cached_resource_positions.push_back(pos);
            }
        }
    }
    resources_gathered = true;
}

void BuildingManager::physics_update(double p_delta) {
    if (attack_manager) {
        attack_manager->update_buildings(p_delta);
    }

    for (auto& pair : buildings) {
        BuildingData& b = pair.second;

        if (b.state == BuildingState::DYING) continue;

        // 递减一次性动画计时器（如生产完成开门动画）
        if (b.one_shot_anim_time > 0.0f) {
            b.one_shot_anim_time -= (float)p_delta;
            if (b.one_shot_anim_time <= 0.0f) {
                b.one_shot_anim_time = -1.0f;
            }
        }

        for (auto& weapon : b.weapons) {
            weapon.prev_rotation = weapon.rotation;
            weapon.update(0.0f, p_delta);
            weapon.next_rotation = weapon.rotation;
        }

        // 逻辑 A：处理建筑自身的建造过程
        if (b.state == BuildingState::BUILDING) {
            // 如果血量满了，自动转为 IDLE
            if (b.current_health >= b.stats->get_health_max()) {
                b.current_health = b.stats->get_health_max();
                b.state = BuildingState::IDLE;
                b.build_timer = 0.0f;
            }
            // 移除原本基于时间的自动回血逻辑，改为由建造者提供
            continue;
        }


        // 逻辑 B：处理兵营单位生产 (仅限 BARRACKS 类型)
        if (b.stats->get_building_type() == BUILDING_BARRACKS) {
            if (!b.production_queue.empty()) {
                // 1. 进入工作状态（触发 WORKING 动画）
                b.state = BuildingState::WORKING;

                // 2. 获取当前正在生产的单位类型
                String unit_type = b.production_queue.front();

                // 3. 获取该单位的 Stats 以读取其所需的 build_time
                Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(unit_type);

                if (u_stats.is_valid()) {
                    // 计算进度：考虑兵营的生产速度加成
                    // 实际时间 = 单位基础建造时间 / 兵营生产效率
                    float production_speed = b.stats->get_production_speed();
                    if (production_speed <= 0.0f) production_speed = 1.0f;

                    b.unit_production_timer += (float)p_delta * production_speed;

                    // 4. 检查是否生产完成
                    if (b.unit_production_timer >= u_stats->get_build_time()) {
                        // --- 生产完成：执行生成 ---

                        // 计算生成位置：建筑中心点下方偏移一点（或出口点）
                        Vector2 cell_sz = Vector2(flow_field_manager->get_cell_size());
                        Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
                        Vector2 spawn_pos = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
                        float random_angle = UtilityFunctions::randf_range(0, Math_TAU);
                        Vector2 random_offset = Vector2(UtilityFunctions::cos(random_angle), UtilityFunctions::sin(random_angle));
                        spawn_pos.y += fp_size.y * 0.6f; // 向下偏离中心，防止重叠
                        spawn_pos += random_offset;

                        // 注意：这里由服务器直接调用 spawn。
                        // 在联机版中，GameManager 会负责随后将此 ID 广播给所有客户端。
                        request_spawn_unit(unit_type, spawn_pos, b.team_id);

                        // 5. 清理队列
                        b.production_queue.erase(b.production_queue.begin());
                        b.unit_production_timer = 0.0f;

                        // 触发生产完成的一次性动画（如开门动画）
                        // 总时长 = 动画帧时长 + 最后一帧暂停时长
                        b.one_shot_anim_total = (float)b.stats->get_working_frames() / b.stats->get_anim_fps();
                        b.one_shot_anim_hold = b.stats->get_working_hold_time();
                        b.one_shot_anim_time = b.one_shot_anim_total + b.one_shot_anim_hold;
                        b.anim_time = 0.0f;

                        // 如果队列空了，回到 IDLE 状态
                        if (b.production_queue.empty()) {
                            b.state = BuildingState::IDLE;
                        }
                    }
                }
                else {
                    // 如果单位 Stats 无效，直接跳过该任务
                    b.production_queue.erase(b.production_queue.begin());
                }
            }
            else {
                // 队列为空时，如果当前是 WORKING 状态则切回 IDLE
                if (b.state == BuildingState::WORKING) {
                    b.state = BuildingState::IDLE;
                }
            }
        }

        // 处理采集器逻辑
        if (b.stats->get_building_type() == BUILDING_COLLECTOR) {
            // 每秒产生的资源 = 采集率 * delta
            double income = b.stats->get_collection_rate() * p_delta;

            // 直接加到经济管理器中
            economy_manager->add_resources(b.team_id, income);
        }
    }
}

void BuildingManager::update(double p_delta) {
    handle_dead_buildings(p_delta);
    maintain_ghosts(p_delta);
}

void BuildingManager::update_multimesh_buffer(double p_delta, float p_alpha, SelectionManager* p_selection_manager) {
    if (type_renderers.empty()) return;

    // 1. 分组
    for (auto& pair : type_grouping_cache) pair.second.clear();
    for (auto const& [id, data] : buildings) {
        type_grouping_cache[data.stats.ptr()].push_back(id);
    }

    // 假设默认单元格大小
    Vector2 cell_sz = Vector2(16, 16);
    if (flow_field_manager) {
        cell_sz = Vector2(flow_field_manager->get_cell_size());
    }

    // 2. 遍历渲染
    for (auto& [s_ptr, mmi] : type_renderers) {
        const std::vector<int>& ids = type_grouping_cache[s_ptr];
        int count = (int)ids.size();

        // 获取主体和影子 MultiMesh
        Ref<MultiMesh> mm = mmi->get_multimesh();
        MultiMeshInstance3D* s_mmi = shadow_renderers[s_ptr];
        Ref<MultiMesh> s_mm = s_mmi->get_multimesh();

        if (mm->get_instance_count() != count) {
            mm->set_instance_count(count);
            s_mm->set_instance_count(count);
        }

        for (int i = 0; i < count; ++i) {
            BuildingData& b = buildings[ids[i]];

            // --- A. 主体位置 ---
            Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
            Vector2 center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;
            float fake_depth_offset = center.y * 0.0001f;

            Transform3D xform;
            xform.origin = Vector3(center.x, fake_depth_offset, center.y);
            xform.basis = Basis().rotated(Vector3(1, 0, 0), -Math_PI / 2.0);
            mm->set_instance_transform(i, xform);

            // --- B. 影子位置 ---
            // 影子偏移量：X+4, Z+4 (仿照 UnitManager)
            // 影子高度稍微降低 (-0.1) 以免和主体重叠
            Transform3D s_xform;
            s_xform.origin = Vector3(center.x + 4.0f, fake_depth_offset - 0.1f, center.y + 4.0f);
            s_xform.basis = xform.basis; // 旋转角度一致
            s_mm->set_instance_transform(i, s_xform);

            // --- C. 动画同步 ---
            bool playing_one_shot = (b.one_shot_anim_time > 0.0f);
            // 一次性动画用 working 帧数（如 7 帧开门动画），其余按状态取帧
            int frames = playing_one_shot
                ? s_ptr->get_working_frames()
                : (b.stats)->get_frames(b.state);
            // 行号使用配置中的动画行（支持单行图集，所有状态共用第 0 行）
            // 建造中(BUILDING)用 -1 标记，让 shader 识别"建造中"并叠加灰色半透明效果
            int row;
            bool is_constructing = (b.state == BuildingState::BUILDING);
            if (playing_one_shot) {
                // 一次性动画（如生产完成开门动画）：按比例取帧，不循环
                row = s_ptr->get_working_row();
            }
            else if (b.state == BuildingState::IDLE) {
                row = s_ptr->get_idle_row();
            }
            else if (b.state == BuildingState::WORKING) {
                row = s_ptr->get_working_row();
            }
            else if (is_constructing) {
                row = -1;
            }
            else {
                row = 0; // DYING 等
            }

            int frame_idx;
            if (playing_one_shot) {
                // 一次性动画：先播放全部帧，然后可选在最后一帧暂停（hold），随后回待机
                float anim_dur = b.one_shot_anim_total > 0.0f ? b.one_shot_anim_total : 1.0f;
                float elapsed = (b.one_shot_anim_total + b.one_shot_anim_hold) - b.one_shot_anim_time;
                if (elapsed <= anim_dur) {
                    // 动画播放阶段：按比例取帧
                    float progress = Math::clamp(elapsed / anim_dur, 0.0f, 1.0f);
                    frame_idx = (int)(progress * (float)frames);
                    if (frame_idx >= frames) frame_idx = frames - 1;
                }
                else {
                    // 暂停阶段：停在最后一帧
                    frame_idx = frames - 1;
                }
            }
            else {
                float duration = (float)frames / s_ptr->get_anim_fps();
                frame_idx = (int)(Math::fmod(b.anim_time, duration) * s_ptr->get_anim_fps());
            }

            float modulate = 1.0;
            if (p_selection_manager->is_building_selected(b.id)) {
                modulate = 1.5f;
            }
            else if (p_selection_manager->is_building_hovered(b.id)) {
                modulate = 1.2f;
            }

            if (b.state == BuildingState::DYING) {
                modulate = 1.0f - (b.current_dying_time / b.stats->get_dying_time()) * 0.5f;
            }
            
            float construction_site_visibility = (b.team_id == p_selection_manager->get_team_id()) ? 1.0f : 0.0f;

            Color anim_data = Color((float)frame_idx, (float)row, modulate, construction_site_visibility);
            mm->set_instance_custom_data(i, anim_data);
            s_mm->set_instance_custom_data(i, anim_data); // 影子也播放同样动作

            mm->set_instance_color(i, get_team_color(b.team_id));

            b.anim_time += (float)p_delta;
        }
    }

    // ========== 处理残影渲染 ==========
    for (auto& pair : ghost_grouping_cache) pair.second.clear();
    for (auto const& [id, g_data] : ghost_buildings) {
        ghost_grouping_cache[g_data.stats.ptr()].push_back(id);
    }

    for (auto& [s_ptr, g_mmi] : ghost_renderers) {
        const std::vector<int>& ids = ghost_grouping_cache[s_ptr];
        int count = (int)ids.size();
        Ref<MultiMesh> g_mm = g_mmi->get_multimesh();

        if (g_mm->get_instance_count() != count) {
            g_mm->set_instance_count(count);
        }

        for (int i = 0; i < count; ++i) {
            const GhostBuildingData& g = ghost_buildings[ids[i]];

            Vector2 fp_size = Vector2(g.stats->get_footprint()) * cell_sz;
            Vector2 center = Vector2(g.grid_pos) * cell_sz + fp_size * 0.5f;
            float fake_depth_offset = center.y * 0.0001f - 0.01f; // 微微降低高度以位于实体之下

            Transform3D xform;
            xform.origin = Vector3(center.x, fake_depth_offset, center.y);
            xform.basis = Basis().rotated(Vector3(1, 0, 0), -Math_PI / 2.0);

            g_mm->set_instance_transform(i, xform);
            g_mm->set_instance_color(i, get_team_color(g.team_id));
            // 不需要再设置 custom_data，Shader 会强行调用第1帧
        }
    }

    // ========== 处理建筑进度条渲染 ==========
    if (!global_progress_bar_renderer) {
        _setup_progress_bar_system();
    }

    int progress_bar_count = buildings.size() * 2; // 血条+进度条
    Ref<MultiMesh> progress_mm = global_progress_bar_renderer->get_multimesh();

    if (progress_mm->get_instance_count() != progress_bar_count) {
        progress_mm->set_instance_count(progress_bar_count);
    }

    int bar_idx = 0;
    for (auto const& [id, b] : buildings) {
        // 1. 获取基础视觉数据
        Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz;
        Vector2 center = Vector2(b.grid_pos) * cell_sz + fp_size * 0.5f;

        // 2. 计算血条和进度条位置和大小
        float hp_offset_y = fp_size.y * 0.6f;
        float progress_offset_y = hp_offset_y + 5.0f;
        float bar_width = fp_size.x * 0.8f; // 根据占地宽度决定血条长度

        Transform3D hp_xform;
        Vector3 hp_pos_3d = Vector3(
            center.x,
            10.0f, // 与兵种一致，稍微抬高 Z 深度以防止穿模
            center.y + hp_offset_y
        );
        hp_xform.origin = hp_pos_3d;
        hp_xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);
        hp_xform.basis = hp_xform.basis.scaled(Vector3(bar_width, 4.0, 4.0));

        progress_mm->set_instance_transform(bar_idx, hp_xform);

        Transform3D progress_xform;
        Vector3 progress_pos_3d = Vector3(
            center.x,
            10.0f, // 与兵种一致，稍微抬高 Z 深度以防止穿模
            center.y + progress_offset_y
        );
        progress_xform.origin = progress_pos_3d;
        progress_xform.basis = hp_xform.basis;

        progress_mm->set_instance_transform(bar_idx + 1, progress_xform);

        // 3. 计算百分比
        float max_health = b.stats->get_health_max();
        float hp_ratio = max_health > 0.0f ? (b.current_health / max_health) : 1.0f;

        float progress_ratio = 0.0f;
        if (b.state == BuildingState::WORKING) {
            // 如果 next 比 prev 小，说明是旧单位造完了，新单位刚开始，此时不插值直接用 next
            if (b.next_progress_percent < b.prev_progress_percent) {
                progress_ratio = b.next_progress_percent;
            }
            else {
                // 平滑插值
                progress_ratio = Math::lerp(b.prev_progress_percent, b.next_progress_percent, p_alpha);
            }
        }

        bool is_barrack = (b.stats->get_building_type() == BUILDING_BARRACKS);
        bool is_working = (b.state == BuildingState::WORKING);

        // 4. 控制显示逻辑：建造中、受伤或被选中时显示
        bool is_selected = p_selection_manager->is_building_selected(b.id);
        bool is_damaged = hp_ratio < 0.99f;
        bool is_building = (b.state == BuildingState::BUILDING);

        if (!(is_selected || is_damaged)) {
            // 如果不需要显示，将缩放归零
            progress_mm->set_instance_transform(bar_idx, Transform3D().scaled(Vector3(0, 0, 0)));
        }
        else {
            // 5. 传递数据给 Shader (X = 比例)
            progress_mm->set_instance_custom_data(bar_idx, Color(hp_ratio, 0.0, 0.0, 0.0));
            Color team_color = Color(1.0, 0.0, 0.0);
            progress_mm->set_instance_color(bar_idx, team_color);
        }

        if (!(is_barrack && is_working)) {
            // 如果不需要显示，将缩放归零
            progress_mm->set_instance_transform(bar_idx + 1, Transform3D().scaled(Vector3(0, 0, 0)));
        }
        else {
            // 5. 传递数据给 Shader (X = 比例)
            progress_mm->set_instance_custom_data(bar_idx + 1, Color(progress_ratio, 0.0, 0.0, 0.0));
            progress_mm->set_instance_color(bar_idx + 1, Color(0.0, 0.0, 1.0));
        }

        bar_idx += 2;
    }

    // ========== 处理小地图点渲染 ==========
    if (flow_field_manager) {
        if (!minimap_dot_renderer) {
            _setup_minimap_renderer(flow_field_manager->get_width(), flow_field_manager->get_height(), flow_field_manager->get_cell_size());
        }
        if (!resources_gathered) {
            _gather_resource_positions();
        }
    }

    if (!minimap_dot_renderer) return;

    Ref<MultiMesh> mmm = minimap_dot_renderer->get_multimesh();
    int building_count = (int)buildings.size();
    int resource_count = (int)cached_resource_positions.size();
    int total_instances = building_count + resource_count;

    if (mmm->get_instance_count() != total_instances) {
        mmm->set_instance_count(total_instances);
    }

    int inst_idx = 0;
    Vector2 cell_sz_vec = Vector2(16, 16);
    if (flow_field_manager) cell_sz_vec = Vector2(flow_field_manager->get_cell_size());

    // 1. 绘制建筑点
    for (auto const& [id, b] : buildings) {
        Vector2 fp_size = Vector2(b.stats->get_footprint()) * cell_sz_vec;
        Vector2 center = Vector2(b.grid_pos) * cell_sz_vec + fp_size * 0.5f;

        Transform3D xform;
        // Y = -50.0f 使其受迷雾遮挡
        xform.origin = Vector3(center.x, -50.0f, center.y);
        xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

        if (b.state == BuildingState::DYING) {
            xform.basis = xform.basis.scaled(Vector3(0, 0, 0));
        }

        mmm->set_instance_transform(inst_idx, xform);
        mmm->set_instance_color(inst_idx, get_team_color(b.team_id));
        inst_idx++;
    }

    // 2. 绘制资源点 (白点)
    for (const Vector2& res_pos : cached_resource_positions) {
        Transform3D xform;
        // 偏移量：向右下偏移 4 像素，避免被采集器正中心遮挡
        Vector2 offset_pos = res_pos + Vector2(4.0f, 4.0f);

        // Y = 300.0f 使其位于迷雾平面之上，永远可见
        xform.origin = Vector3(offset_pos.x, 300.0f, offset_pos.y);
        xform.basis = Basis().rotated(Vector3(1, 0, 0), Math_PI / 2.0);

        mmm->set_instance_transform(inst_idx, xform);
        mmm->set_instance_color(inst_idx, Color(1, 1, 1, 1)); // 纯白色
        inst_idx++;
    }
}

void BuildingManager::handle_dead_buildings(double p_delta) {
    // 使用临时向量存储需要移除的 ID，避免在遍历时修改 map
    std::vector<int> to_remove;

    for (auto& pair : buildings) {
        BuildingData& b = pair.second;

        // 触发死亡状态
        if (b.current_health <= 0 && b.state != BuildingState::DYING) {
            b.state = BuildingState::DYING;
            b.current_dying_time = 0.0f;
            // 死亡时立即释放格子占位
        }

        // 死亡计时
        if (b.state == BuildingState::DYING) {
            b.current_dying_time += (float)p_delta;

            // 可以暂用固定值如 2.0f
            float max_dying_time = 2.0f;
            if (b.stats.is_valid()) {
                // 建议在 BuildingStats 加上这个属性
                // max_dying_time = b.stats->get_dying_time(); 
            }

            if (b.current_dying_time >= max_dying_time) {
                to_remove.push_back(b.id);
            }
        }
    }

    for (int id : to_remove) {
        // 发出信号让 GameManager 执行最终的逻辑移除（清理 Selection 等）
        emit_signal("despawn_building_requested", id);
    }
}

void BuildingManager::maintain_ghosts(double p_delta) {
    // 1. 同步活着的建筑到残影列表 (记录记忆)
    // 只要建筑还存在，它的信息就会持续写入并刷新
    for (const auto& pair : buildings) {
        const BuildingData& b = pair.second;
        GhostBuildingData gd;
        gd.stats = b.stats;
        gd.state = b.state;
        gd.grid_pos = b.grid_pos;
        gd.team_id = b.team_id;
        gd.weapons = b.weapons;
        ghost_buildings[b.id] = gd;
    }

    // 2. 清理已经被摧毁但处于玩家视野中的残影
    // GPU到CPU拿图像极其耗时，所以这里用 0.5秒 执行一次（节流）
    ghost_cleanup_timer += (float)p_delta;
    if (ghost_cleanup_timer >= 0.5f) {
        ghost_cleanup_timer = 0.0f;

        if (fog_manager) {
            Ref<Texture2D> live_tex = fog_manager->get_live_texture();
            if (live_tex.is_valid()) {
                // 基于 vpc_live 读取玩家是否有这里的视野
                Ref<Image> live_img = live_tex->get_image();
                if (live_img.is_valid() && !live_img->is_empty()) {
                    std::vector<int> ghosts_to_remove;
                    Vector2 cell_sz = Vector2(flow_field_manager->get_cell_size());
                    Vector2 map_size = fog_manager->get_map_size();
                    Vector2 map_pos = fog_manager->get_map_pos();

                    int img_w = live_img->get_width();
                    int img_h = live_img->get_height();

                    for (const auto& pair : ghost_buildings) {
                        int b_id = pair.first;

                        // 真实建筑存活着，直接跳过（它的隐藏交由Shader负责）
                        auto it = buildings.find(b_id);
                        if (it != buildings.end()) {
                            if (it->second.state != BuildingState::DYING) { continue; }
                        }

                        const GhostBuildingData& g = pair.second;
                        Vector2 fp_size = Vector2(g.stats->get_footprint()) * cell_sz;
                        Vector2 center = Vector2(g.grid_pos) * cell_sz + fp_size * 0.5f;

                        // 根据坐标映射到 vpc_live 像素坐标
                        Vector2 fog_uv = (center - map_pos) / map_size;
                        int px = Math::clamp(int(fog_uv.x * img_w), 0, img_w - 1);
                        int py = Math::clamp(int(fog_uv.y * img_h), 0, img_h - 1);

                        // 拿取该坐标处的迷雾颜色
                        Color c = live_img->get_pixel(px, py);

                        // 如果红色通道大于 0.1 说明此处没有迷雾（玩家看到了这个格子）
                        // 既然它没有在 buildings 里，意味着建筑已经被拔除了，我们需要永久忘掉这个残影
                        if (c.r > 0.1f) {
                            ghosts_to_remove.push_back(b_id);
                        }
                    }

                    // 擦除记忆中的建筑
                    for (int id : ghosts_to_remove) {
                        ghost_buildings.erase(id);
                    }
                }
            }
        }
    }
}

void BuildingManager::_internal_register_building(Ref<BuildingStats> p_stats) {
    String p_name = p_stats->get_building_name();
    building_types_cache[p_name] = p_stats;
    BuildingStats* s_ptr = p_stats.ptr();

    if (type_renderers.count(s_ptr)) return;

    // --- A. 初始化建筑主体渲染器 ---
    MultiMeshInstance3D* mmi = memnew(MultiMeshInstance3D);
    mmi->set_name(p_name + "_Renderer");
    add_child(mmi);

    Ref<MultiMesh> mm;
    mm.instantiate();
    mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    mm->set_use_colors(true);
    mm->set_use_custom_data(true);

    Ref<QuadMesh> qmesh;
    qmesh.instantiate();

    Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(p_stats->get_texture_path());
    if (tex.is_valid()) {
        Vector2 frame_size = tex->get_size() / Vector2(p_stats->get_h_frames(), p_stats->get_v_frames());
        qmesh->set_size(frame_size);
    } else {
        UtilityFunctions::printerr("[BuildingManager] 无法加载建筑贴图: ", p_stats->get_texture_path(), " 建筑: ", p_name);
    }
    mm->set_mesh(qmesh);
    mmi->set_multimesh(mm);

    // 设置主体材质
    Ref<ShaderMaterial> mat;
    mat.instantiate();
    if (building_shader.is_null()) {
        building_shader = ResourceLoader::get_singleton()->load("res://shader/building_shader.gdshader");
    }
    mat->set_shader(building_shader);
    mat->set_shader_parameter("albedo_texture", tex);
    mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    if (fog_manager) {
        mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    mmi->set_material_override(mat);
    type_renderers[s_ptr] = mmi;

    // --- B. 初始化影子渲染器 ---
    MultiMeshInstance3D* s_mmi = memnew(MultiMeshInstance3D);
    s_mmi->set_name(p_name + "_Shadow");
    add_child(s_mmi);

    Ref<MultiMesh> s_mm;
    s_mm.instantiate();
    s_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    s_mm->set_use_custom_data(true); // 影子也要同步动画帧

    Ref<QuadMesh> s_qmesh;
    s_qmesh.instantiate();
    s_qmesh->set_size(qmesh->get_size()); // 影子大小与建筑一致
    s_mm->set_mesh(s_qmesh);
    s_mmi->set_multimesh(s_mm);

    // 设置影子材质
    Ref<ShaderMaterial> s_mat;
    s_mat.instantiate();
    if (shadow_shader.is_null()) {
        shadow_shader = ResourceLoader::get_singleton()->load("res://shader/building_shadow.gdshader");
    }
    s_mat->set_shader(shadow_shader);
    s_mat->set_shader_parameter("albedo_texture", tex);
    s_mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    s_mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    if (fog_manager) {
        s_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        s_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        s_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    s_mmi->set_material_override(s_mat);
    shadow_renderers[s_ptr] = s_mmi;

    // --- C. 初始化残影渲染器 ---
    MultiMeshInstance3D* g_mmi = memnew(MultiMeshInstance3D);
    g_mmi->set_name(p_name + "_Ghost");
    add_child(g_mmi);

    Ref<MultiMesh> g_mm;
    g_mm.instantiate();
    g_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    g_mm->set_use_colors(true);
    g_mm->set_use_custom_data(false); // 禁用 custom_data，节约内存与总线带宽，因为第一帧已在 Shader 写死

    Ref<QuadMesh> g_qmesh;
    g_qmesh.instantiate();
    g_qmesh->set_size(qmesh->get_size());
    g_mm->set_mesh(g_qmesh);
    g_mmi->set_multimesh(g_mm);

    Ref<ShaderMaterial> g_mat;
    g_mat.instantiate();
    if (ghost_shader.is_null()) {
        ghost_shader = ResourceLoader::get_singleton()->load("res://shader/ghost_building_shader.gdshader");
    }
    g_mat->set_shader(ghost_shader);
    g_mat->set_shader_parameter("albedo_texture", tex);
    g_mat->set_shader_parameter("h_frames", p_stats->get_h_frames());
    g_mat->set_shader_parameter("v_frames", p_stats->get_v_frames());
    if (fog_manager) {
        g_mat->set_shader_parameter("tex_fog_live", fog_manager->get_live_texture());
        g_mat->set_shader_parameter("tex_history", fog_manager->get_history_texture());
        g_mat->set_shader_parameter("map_size", fog_manager->get_map_size());
        g_mat->set_shader_parameter("map_pos", fog_manager->get_map_pos());
    }

    g_mmi->set_material_override(g_mat);
    ghost_renderers[s_ptr] = g_mmi;
}

void BuildingManager::register_building_type(String p_name, String p_path) {
    Ref<BuildingStats> stats = BuildingLoader::load_from_cfg(p_path, weapon_manager);
    if (stats.is_null()) return;
    if (stats->get_building_name().is_empty()) stats->set_building_name(p_name);
    _internal_register_building(stats);
}

void BuildingManager::register_buildings_from_dir(String p_dir_path) {
    Ref<DirAccess> dir = DirAccess::open(p_dir_path);
    if (dir.is_null()) return;

    dir->list_dir_begin();
    String file_name = dir->get_next();
    while (file_name != "") {
        if (!dir->current_is_dir() && file_name.ends_with(".cfg")) {
            String full_path = p_dir_path.path_join(file_name);
            Ref<BuildingStats> stats = BuildingLoader::load_from_cfg(full_path, weapon_manager);
            if (stats.is_valid() && !stats->get_building_name().is_empty()) {
                _internal_register_building(stats);
            }
        }
        file_name = dir->get_next();
    }
}

bool BuildingManager::is_area_clear(Vector2i p_grid_pos, Ref<BuildingStats> p_stats) {
    if (!flow_field_manager || p_stats.is_null()) return false;

    Vector2i footprint = p_stats->get_footprint();
    Vector2i clearance = p_stats->get_clearance_size();
    uint32_t req = p_stats->get_placement_requirement();

    // 1. 计算 Clearance 检查的起始点
    // 假设 p_grid_pos 是建筑实际占用(footprint)的左上角
    // 偏移量 = (Clearance尺寸 - Footprint尺寸) / 2
    Vector2i offset = (clearance - footprint) / 2;
    Vector2i check_start = p_grid_pos - offset;

    // --- 逻辑检查 A: 地形与 Clearance 范围 ---
    // 这个范围内不能有任何已存在的建筑 (Cost 255)
    for (int x = 0; x < clearance.x; ++x) {
        for (int y = 0; y < clearance.y; ++y) {
            Vector2i current_cell = check_start + Vector2i(x, y);

            if (!flow_field_manager->is_in_grid(current_cell)) return false;

            // 如果要求在陆地：陆地层不能是墙 (255)
            if (req & PLACE_LAND) {
                if (flow_field_manager->get_cost(current_cell, NAV_LAND) == 255) { return false; }
            }

            // 如果要求在水上：检查 NAV_SEA 层
            if (req & PLACE_WATER) {
                if (flow_field_manager->get_cost(current_cell, NAV_SEA) == 255) { return false; }
            }

            if (req & PLACE_ON_RESOURCE) {
                if (!(flow_field_manager->get_cell_metadata(current_cell) & CELL_META_RESOURCE)) { return false; }
            }
            else if (flow_field_manager->get_cell_metadata(current_cell) & CELL_META_RESOURCE) { return false; }
        }
    }

    // --- 逻辑检查 B: 单位阻挡 (仅检查 Footprint 范围) ---
    if (unit_manager) {
        Vector2i cell_size = flow_field_manager->get_cell_size();
        Vector2 world_pos = Vector2(p_grid_pos.x * cell_size.x, p_grid_pos.y * cell_size.y);
        Vector2 world_size = Vector2(footprint.x * cell_size.x, footprint.y * cell_size.y);
        Rect2 building_rect(world_pos, world_size);

        Vector2 center = world_pos + world_size * 0.5;
        float query_radius = world_size.length() * 0.5;

        std::vector<int> nearby_units = unit_manager->get_nearby_units(center, query_radius);
        for (int unit_idx : nearby_units) {
            if (building_rect.has_point(unit_manager->units[unit_idx].position)) {
                return false;
            }
        }
    }

    return true;
}

int BuildingManager::get_building_at_position(Vector2 p_world_pos) {
    if (!flow_field_manager) return -1;
    Vector2i grid_pos = flow_field_manager->world_to_grid(p_world_pos);

    // 遍历当前建筑列表
    for (const auto& pair : buildings) {
        const BuildingData& b = pair.second;
        Vector2i fp = b.stats->get_footprint();
        if (grid_pos.x >= b.grid_pos.x && grid_pos.x < b.grid_pos.x + fp.x &&
            grid_pos.y >= b.grid_pos.y && grid_pos.y < b.grid_pos.y + fp.y) {
            return b.id;
        }
    }
    return -1;
}

std::vector<int> BuildingManager::get_buildings_of_type_in_area(Ref<BuildingStats> p_stats, Rect2 p_rect, int p_team_id) {
    std::vector<int> result;
    Vector2i grid_rect_pos = flow_field_manager->world_to_grid(p_rect.position);
    Vector2i grid_rect_end = flow_field_manager->world_to_grid(p_rect.get_end())+ Vector2i(1, 1);

    for (const auto& pair : buildings) {
        const BuildingData& b = pair.second;
        Vector2i fp = b.stats->get_footprint();
        if (grid_rect_pos.x < b.grid_pos.x && grid_rect_end.x > b.grid_pos.x + fp.x &&
            grid_rect_pos.y < b.grid_pos.y && grid_rect_end.y > b.grid_pos.y + fp.y) {
            if (b.stats == p_stats && b.team_id == p_team_id) {
                result.push_back(b.id);
            }
        }
    }

    return result;
}

std::vector<int> BuildingManager::get_buildings_in_box(Rect2 p_box, int p_team_id) {
    std::vector<int> result;
    Vector2i grid_rect_pos = flow_field_manager->world_to_grid(p_box.position);
    Vector2i grid_rect_end = flow_field_manager->world_to_grid(p_box.get_end()) + Vector2i(1, 1);

    for (const auto& pair : buildings) {
        const BuildingData& b = pair.second;
        Vector2i fp = b.stats->get_footprint();
        if (grid_rect_pos.x < b.grid_pos.x && grid_rect_end.x > b.grid_pos.x + fp.x &&
            grid_rect_pos.y < b.grid_pos.y && grid_rect_end.y > b.grid_pos.y + fp.y) {
            if (b.team_id == p_team_id) {
                result.push_back(b.id);
            }
        }
    }

    return result;
}

int BuildingManager::place_building_by_type(String p_type_name, Vector2i p_grid_pos, int p_team_id, int p_forced_id, bool is_pre_placed, bool p_force) {
	if (!building_types_cache.has(p_type_name)) return -1;

	Ref<BuildingStats> stats = building_types_cache[p_type_name];
	uint32_t req = stats->get_placement_requirement();

	if (!p_force && !is_area_clear(p_grid_pos, stats)) return -1;

    // 1. 创建建筑
    int b_id = p_forced_id;
    if (b_id == -1) b_id = next_building_id++;
    else if (b_id >= next_building_id) next_building_id = b_id + 1;

    BuildingData b;
    b.id = b_id;
    b.grid_pos = p_grid_pos;
    b.stats = stats;
    b.team_id = p_team_id;

    if (is_pre_placed) {
        b.state = BuildingState::IDLE;
        b.current_health = b.stats->get_health_max();
    }
    else {
        b.state = BuildingState::BUILDING;
        b.current_health = 1.0f;
    }

    for (const auto& mount : stats->weapon_mounts) {
        WeaponData wd;
        wd.stats = mount.weapon_resource;
        wd.local_position = mount.local_position;
        b.weapons.push_back(wd);
    }

    buildings[b_id] = b;

    // 2. 修改代价地图：注意这里只设置 Footprint 的格子
    Vector2i footprint = stats->get_footprint();
    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            if (req & PLACE_LAND) {
                flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255, NAV_LAND);
            }
            if (req & PLACE_WATER) {
                flow_field_manager->set_cost(p_grid_pos + Vector2i(x, y), 255, NAV_SEA);
            }
        }
    }

    flow_field_manager->make_all_dirty();
    return b_id;
}

void BuildingManager::remove_building(int p_building_id, SelectionManager* p_selection_manager) {
    auto it = buildings.find(p_building_id);
    if (it == buildings.end()) return;

    BuildingData& b = it->second;
    Vector2i footprint = b.stats->get_footprint();
    uint32_t req = b.stats->get_placement_requirement();

    for (int x = 0; x < footprint.x; ++x) {
        for (int y = 0; y < footprint.y; ++y) {
            if (req & PLACE_LAND) {
                flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1, NAV_LAND);
            }
            if (req & PLACE_WATER) {
                flow_field_manager->set_cost(b.grid_pos + Vector2i(x, y), 1, NAV_SEA);
            }
        }
    }

    buildings.erase(it);
    flow_field_manager->make_all_dirty();
    p_selection_manager->on_building_removed(p_building_id);
}

void BuildingManager::clear_all_buildings() {
	buildings.clear();
	ghost_buildings.clear();
}

void BuildingManager::add_unit_to_production_queue(int p_building_id, String p_unit_type) {
    auto it = buildings.find(p_building_id);
    if (it == buildings.end()) return;

    BuildingData& b = it->second;

    // 验证：该单位是否在兵营的可生产列表中？
    PackedStringArray producible = b.stats->get_producible_units();
    bool can_produce = false;
    for (int i = 0; i < producible.size(); ++i) {
        if (producible[i] == p_unit_type) {
            can_produce = true;
            break;
        }
    }

    if (can_produce) {
        b.production_queue.push_back(p_unit_type);
        // 如果是从空队列开始，状态切换到 WORKING 已经在之前的 update 逻辑中处理了
    }
}

void godot::BuildingManager::request_spawn_unit(String p_unit_type, Vector2 p_pos, int p_team_id){
    emit_signal("spawn_unit_requested", p_unit_type, p_pos, p_team_id);
}

int BuildingManager::get_building_team_id(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.team_id;
    return -1;
}

Vector2i BuildingManager::get_building_grid_pos(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.grid_pos;
    return Vector2i(-1, -1);
}

Ref<BuildingStats> BuildingManager::get_building_stats(int p_building_id) const {
    auto it = buildings.find(p_building_id);
    if (it != buildings.end()) return it->second.stats;
    return nullptr;
}

PackedStringArray BuildingManager::get_registered_building_types() const {
    PackedStringArray list;
    // 遍历 HashMap 的所有键
    for (const auto& E : building_types_cache) {
        list.append(E.key);
    }
    return list;
}

void BuildingManager::prepare_interpolation_snapshot() {
    for (auto& pair : buildings) {
        BuildingData& b = pair.second;

        // 1. 移动旧快照
        b.prev_progress_percent = b.next_progress_percent;

        // 2. 计算并存入新快照
        float current_prog = 0.0f;

        // 情况 A：正在建造建筑本身
        /*
        if (b.state == BuildingState::BUILDING) {
            float required_time = b.stats->get_build_time();
            if (required_time > 0) {
                current_prog = b.build_timer / required_time;
            }
        }
        */

        // 情况 B：正在生产单位 (WORKING)
        if (b.state == BuildingState::WORKING && !b.production_queue.empty()) {
            Ref<UnitStats> u_stats = unit_manager->get_unit_stats_by_type(b.production_queue.front());
            if (u_stats.is_valid()) {
                current_prog = b.unit_production_timer / u_stats->get_build_time();
            }
        }

        b.next_progress_percent = current_prog;
    }
}

void BuildingManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("register_building_type", "name", "path"), &BuildingManager::register_building_type);
    ClassDB::bind_method(D_METHOD("register_buildings_from_dir", "path"), &BuildingManager::register_buildings_from_dir);

    ClassDB::bind_method(D_METHOD("place_building_by_type", "type_name", "grid_pos", "team_id", "forced_id", "is_pre_placed", "force"),
        &BuildingManager::place_building_by_type, DEFVAL(-1), DEFVAL(false), DEFVAL(false));
    ClassDB::bind_method(D_METHOD("is_area_clear", "grid_pos", "building_stats"), &BuildingManager::is_area_clear);

    ClassDB::bind_method(D_METHOD("set_flow_field_manager", "node"), &BuildingManager::set_flow_field_manager);
    ClassDB::bind_method(D_METHOD("set_unit_manager", "node"), &BuildingManager::set_unit_manager);
    ClassDB::bind_method(D_METHOD("remove_building", "building_id"), &BuildingManager::remove_building);
    ClassDB::bind_method(D_METHOD("get_building_grid_pos", "building_id"), &BuildingManager::get_building_grid_pos);
    ClassDB::bind_method(D_METHOD("get_building_stats", "building_id"), &BuildingManager::get_building_stats);

    ClassDB::bind_method(D_METHOD("request_spawn_unit", "unit_type", "spawn_pos", "team_id"), &BuildingManager::request_spawn_unit);

    ClassDB::bind_method(D_METHOD("get_registered_building_types"), &BuildingManager::get_registered_building_types);
    ClassDB::bind_method(D_METHOD("get_building_stats_by_type", "type_name"), &BuildingManager::get_building_stats_by_type);

    // 参数：类型名称, 网格坐标, 队伍ID
    ADD_SIGNAL(MethodInfo("placement_requested", PropertyInfo(Variant::STRING, "type_name"), PropertyInfo(Variant::VECTOR2I, "grid_pos"), 
        PropertyInfo(Variant::INT, "team_id"), PropertyInfo(Variant::INT, "forced_id"), PropertyInfo(Variant::BOOL, "is_pre_placed")));
    ADD_SIGNAL(MethodInfo("spawn_unit_requested", PropertyInfo(Variant::STRING, "type_name"), PropertyInfo(Variant::VECTOR2, "spawn_pos"),
        PropertyInfo(Variant::INT, "team_id")));

    ADD_SIGNAL(MethodInfo("despawn_building_requested", PropertyInfo(Variant::INT, "building_id")));
}