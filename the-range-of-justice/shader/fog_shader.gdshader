shader_type spatial;
render_mode unshaded, blend_mix, depth_test_disabled;

uniform sampler2D tex_live;    // 实时视野 (vpc_live)
uniform sampler2D tex_history; // 探索历史 (vpc_history)
uniform vec3 fog_color : source_color = vec3(0.0, 0.0, 0.0);
uniform float shroud_opacity : hint_range(0.0, 1.0) = 0.5; // 重雾透明度

void fragment() {
    float live = texture(tex_live, UV).r;
    float history = texture(tex_history, UV).r;

    float alpha = 1.0;
    if (live > 0.1) {
        alpha = 0.0; // 当前可见：完全透明
    } else if (history > 0.1) {
        alpha = shroud_opacity; // 已探索：半透明（显示下方地图）
    } else {
        alpha = 1.0; // 未探索：纯黑
    }
    
    ALBEDO = fog_color;
    ALPHA = alpha;
}