#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/audio_stream_player3d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <vector>

namespace godot {

    class AudioManager : public Node {
        GDCLASS(AudioManager, Node)

    private:
        // 使用两个播放器来实现交叉淡入淡出
        AudioStreamPlayer* bgm_players[2];
        int active_bgm_idx = 0; // 记录当前正在播放的播放器索引

        // 3D 音效对象池
        std::vector<AudioStreamPlayer3D*> sfx_pool_3d;
        int next_pool_idx = 0;
        int pool_size = 32; // 同时允许发出的最大 3D 音效数量

        // 2D 音效对象池 (用于 UI 或全局提示音)
        std::vector<AudioStreamPlayer*> sfx_pool_2d;
        int next_pool_idx_2d = 0;
        int pool_size_2d = 8; // UI 同时发声通常不需要太多

        // 资源缓存：防止频繁 IO 加载同一音效
        HashMap<String, Ref<AudioStream>> sound_cache;

        // 内部方法：获取或加载音频流
        Ref<AudioStream> _get_audio_stream(const String& p_path);

    protected:
        static void _bind_methods();

    public:
        AudioManager();
        ~AudioManager();

        // 播放背景音乐
        void play_bgm(String p_path, float p_volume = 0.0f, float p_fade_duration = 1.5f);
        void stop_bgm(float p_fade_duration = 1.5f);

        // 在 3D 位置播放音效 (适合单位开火、爆炸、粒子触发)
        // p_pitch_range: 音高随机偏移量，0.1 表示在 0.9-1.1 之间随机，增加听感多样性
        void play_sfx_3d(String p_path, Vector3 p_pos, float p_volume = 0.0f, float p_pitch_range = 0.1f);

        // 播放非空间音效 (UI)
        void play_ui_sfx(String p_path, float p_volume = 0.0f);

        // 预加载音效 (建议在游戏初始化或关卡加载时调用)
        void preload_sound(String p_path);
    };

} // namespace godot