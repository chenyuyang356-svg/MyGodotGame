#include "audio_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/tween.hpp>
#include <godot_cpp/classes/property_tweener.hpp>
#include <godot_cpp/classes/callback_tweener.hpp>

using namespace godot;

void AudioManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("play_bgm", "path/alias", "volume", "fade_duration"), &AudioManager::play_bgm, DEFVAL(0.0f), DEFVAL(1.5f));
    ClassDB::bind_method(D_METHOD("stop_bgm", "fade_duration"), &AudioManager::stop_bgm, DEFVAL(1.5f));
    ClassDB::bind_method(D_METHOD("play_sfx_3d", "path/alias", "pos", "volume", "pitch_range"), &AudioManager::play_sfx_3d, DEFVAL(0.0f), DEFVAL(0.1f));
    ClassDB::bind_method(D_METHOD("play_ui_sfx", "path/alias", "volume"), &AudioManager::play_ui_sfx, DEFVAL(0.0f));
    ClassDB::bind_method(D_METHOD("preload_sound", "path"), &AudioManager::preload_sound);
    ClassDB::bind_method(D_METHOD("register_sound", "alias", "path"), &AudioManager::register_sound);
}

AudioManager::AudioManager() {
    // 1. 设置背景音乐播放器
    for (int i = 0; i < 2; ++i) {
        bgm_players[i] = memnew(AudioStreamPlayer);
        add_child(bgm_players[i]);
        bgm_players[i]->set_bus("Music");
        bgm_players[i]->set_volume_db(-80.0f); // 初始全静音
    }
    active_bgm_idx = 0;

    // 2. 初始化 3D 音效池
    for (int i = 0; i < pool_size; ++i) {
        AudioStreamPlayer3D* player = memnew(AudioStreamPlayer3D);
        add_child(player);

        // --- 针对 2D 视角的 3D 音效优化 ---
        // 使用距离平方反比衰减
        player->set_attenuation_model(AudioStreamPlayer3D::ATTENUATION_INVERSE_SQUARE_DISTANCE);
        // unit_size 越大，声音传得越远。20.0 适合中等规模地图
        player->set_unit_size(2000.0f);
        // 限制最大距离，超出此距离不处理音频计算，节省 CPU
        player->set_max_distance(10000.0f);

        player->set_bus("SFX"); // 建议在 Godot 编辑器中创建一个名为 "SFX" 的总线
        sfx_pool_3d.push_back(player);
    }

    // 3. 初始化 2D 音效池
    for (int i = 0; i < pool_size_2d; ++i) {
        AudioStreamPlayer* player = memnew(AudioStreamPlayer);
        add_child(player);
        player->set_bus("UI"); // 指向 UI 总线
        sfx_pool_2d.push_back(player);
    }
}

AudioManager::~AudioManager() {
    // 节点会被 Godot 自动清理
}

Ref<AudioStream> AudioManager::_get_audio_stream(const String& p_id) {
    // 1. 优先检查缓存（包括别名和已加载过的路径）
    if (sound_cache.has(p_id)) {
        return sound_cache[p_id];
    }

    // 2. 如果缓存没有，且看起来像路径，则尝试加载
    if (p_id.begins_with("res://")) {
        Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(p_id);
        if (stream.is_valid()) {
            sound_cache[p_id] = stream; // 自动缓存该路径，下次直接走步骤1
            return stream;
        }
    }

    UtilityFunctions::print("[AudioManager] Error: Sound ID not found or invalid path: ", p_id);
    return nullptr;
}

AudioStreamPlayer3D* AudioManager::_get_best_3d_player() {
    AudioStreamPlayer3D* oldest_player = nullptr;
    float max_playback_pos = -1.0f;

    for (int i = 0; i < pool_size; ++i) {
        AudioStreamPlayer3D* p = sfx_pool_3d[i];

        // 1. 如果找到完全没在播放的，直接返回
        if (!p->is_playing()) {
            return p;
        }

        // 2. 如果都在播放，寻找播放进度最接近结束的（或者也可以记录开始时间，找播放最久的）
        float current_pos = p->get_playback_position();
        if (current_pos > max_playback_pos) {
            max_playback_pos = current_pos;
            oldest_player = p;
        }
    }

    // 如果全忙，返回那个快播完的
    return oldest_player;
}

void AudioManager::preload_sound(String p_path) {
    _get_audio_stream(p_path);
}

void AudioManager::play_bgm(String p_path, float p_volume, float p_fade_duration) {
    Ref<AudioStream> next_stream = _get_audio_stream(p_path);
    if (next_stream.is_null()) return;

    AudioStreamPlayer* current_player = bgm_players[active_bgm_idx];

    // 如果新曲目已经在当前播放，且正在播放中，则直接跳过（或根据需要调整音量）
    if (current_player->get_stream() == next_stream && current_player->is_playing()) {
        return;
    }

    // 切换索引：寻找下一个可用的播放器
    int next_idx = (active_bgm_idx + 1) % 2;
    AudioStreamPlayer* next_player = bgm_players[next_idx];

    // 配置新播放器
    next_player->set_stream(next_stream);
    next_player->set_volume_db(-80.0f); // 从静音开始
    next_player->play();

    // 创建 Tween 处理交叉淡入淡出
    // 注意：create_tween() 需要在 SceneTree 运行状态下调用
    Ref<Tween> tween = get_tree()->create_tween();
    tween->set_parallel(true); // 让淡入和淡出同时进行

    // 1. 旧播放器淡出
    tween->tween_property(current_player, "volume_db", -80.0f, p_fade_duration)
        ->set_trans(Tween::TRANS_SINE);

    // 2. 新播放器淡入
    tween->tween_property(next_player, "volume_db", p_volume, p_fade_duration)
        ->set_trans(Tween::TRANS_SINE);

    // 切换完成后，停止旧播放器以释放资源
    tween->set_parallel(false); // 这一行后的操作在前面完成后执行
    tween->tween_callback(Callable(current_player, "stop"));

    // 更新当前活跃索引
    active_bgm_idx = next_idx;
}

void AudioManager::stop_bgm(float p_fade_duration) {
    AudioStreamPlayer* current_player = bgm_players[active_bgm_idx];
    if (!current_player->is_playing()) return;

    Ref<Tween> tween = get_tree()->create_tween();
    tween->tween_property(current_player, "volume_db", -80.0f, p_fade_duration)
        ->set_trans(Tween::TRANS_SINE);
    tween->tween_callback(Callable(current_player, "stop"));
}

void AudioManager::play_sfx_3d(String p_path, Vector3 p_pos, float p_volume, float p_pitch_range) {
    // --- 1. 分组截断逻辑 ---
    uint64_t now = Time::get_singleton()->get_ticks_msec();

    if (last_play_times.has(p_path)) {
        uint64_t last_time = last_play_times[p_path];
        if (now - last_time < MIN_SFX_INTERVAL_MS) {
            // 如果距离上次播放太近，直接放弃本次播放，保护音效池和玩家耳朵
            return;
        }
    }
    // 更新最后播放时间
    last_play_times[p_path] = now;

    // --- 2. 获取资源 ---
    Ref<AudioStream> stream = _get_audio_stream(p_path);
    if (stream.is_null()) return;

    // --- 3. 获取播放器并处理防爆音 ---
    AudioStreamPlayer3D* player = _get_best_3d_player();

    if (player->is_playing()) {
        // 如果该播放器正在播放，且我们不得不覆盖它：
        // 瞬间停止可能会产生“咔哒”声，理想做法是调用 stop 并在总线上挂载 Limiter
        player->stop();
    }

    // --- 4. 配置并播放 ---
    Vector3 fixed_pos = p_pos;
    fixed_pos.y = 0.0f;
    player->set_position(fixed_pos);
    player->set_stream(stream);
    player->set_volume_db(p_volume);

    // 随机音高仍然保留，因为它能显著减轻“机器感”
    float random_pitch = 1.0f + (UtilityFunctions::randf() * 2.0f - 1.0f) * p_pitch_range;
    player->set_pitch_scale(random_pitch);

    player->play();
}

void AudioManager::play_ui_sfx(String p_path, float p_volume) {
    uint64_t now = Time::get_singleton()->get_ticks_msec();
    if (last_play_times.has(p_path) && (now - last_play_times[p_path] < MIN_SFX_INTERVAL_MS)) {
        return;
    }
    last_play_times[p_path] = now;

    Ref<AudioStream> stream = _get_audio_stream(p_path);
    if (stream.is_null()) return;

    AudioStreamPlayer* player = sfx_pool_2d[next_pool_idx_2d];
    if (player->is_playing()) player->stop();

    player->set_stream(stream);
    player->set_volume_db(p_volume);
    player->play();

    next_pool_idx_2d = (next_pool_idx_2d + 1) % pool_size_2d;
}

void AudioManager::register_sound(String p_alias, String p_path) {
    Ref<AudioStream> stream = ResourceLoader::get_singleton()->load(p_path);
    if (stream.is_valid()) {
        sound_cache[p_alias] = stream;
        // 同时也缓存路径名，避免重复加载
        if (!sound_cache.has(p_path)) {
            sound_cache[p_path] = stream;
        }
        UtilityFunctions::print("[AudioManager] Registered alias: ", p_alias, " -> ", p_path);
    }
    else {
        UtilityFunctions::print("[AudioManager] Failed to register: ", p_alias, " (Invalid path: ", p_path, ")");
    }
}