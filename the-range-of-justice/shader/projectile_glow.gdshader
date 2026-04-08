shader_type spatial;
render_mode cull_disabled, blend_mix, depth_prepass_alpha;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform int h_frames = 1;
uniform int v_frames = 1;
uniform float emission_boost = 3.0; // 控制辉光强度的系数

uniform sampler2D tex_fog_live;
uniform vec2 map_size;
uniform vec2 map_pos;

varying vec4 custom_data;
varying vec4 instance_color;
varying vec3 world_pos;

void vertex() {
    custom_data = INSTANCE_CUSTOM;
    instance_color = COLOR;
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
    // 1. 迷雾计算
    vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
                       (world_pos.z - map_pos.y) / map_size.y);
    float visibility = texture(tex_fog_live, fog_uv).r;

    // 2. 动画帧计算
    float frame = floor(custom_data.x);
    float row = floor(custom_data.y); 
    float modulate = custom_data.z;

    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    vec2 atlas_uv = UV;
    atlas_uv.x = (atlas_uv.x + frame) * frame_size.x;
    atlas_uv.y = (atlas_uv.y + row) * frame_size.y;

    // 3. 采样颜色
    vec4 tex = texture(albedo_texture, atlas_uv);
    vec3 final_rgb = tex.rgb;

    // 4. 团队染色
    if (tex.a > 0.994 && tex.a < 0.998) {
        float brightness = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
        final_rgb = instance_color.rgb * brightness;
    }

    // 5. 最终输出
    ALBEDO = final_rgb * modulate;
    // 关键：增加自发光以触发 WorldEnvironment 的 Glow 效果
    EMISSION = ALBEDO * emission_boost;
    
    float alpha_out = tex.a * smoothstep(0.0, 0.2, visibility);
    if (alpha_out < 0.01) discard; 
    ALPHA = alpha_out;
}
