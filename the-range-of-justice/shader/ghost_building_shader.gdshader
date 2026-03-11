shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_always;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform int h_frames = 1;
uniform int v_frames = 1;

uniform sampler2D tex_fog_live;  // 对应 vpc_live
uniform sampler2D tex_history;   // 对应 vpc_history
uniform vec2 map_size;          
uniform vec2 map_pos;          

varying vec4 instance_color;
varying vec3 world_pos;

void vertex() {
    instance_color = COLOR;
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
    vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
                       (world_pos.z - map_pos.y) / map_size.y);
                       
    float live = texture(tex_fog_live, fog_uv).r;
    float history = texture(tex_history, fog_uv).r;

    // 基于 vpc_live 数据：如果当前处于视野内 (live > 0.1)，
    // 或此处从未探索过 (history < 0.1)，则丢弃残影像素
    if (live > 0.1 || history < 0.1) {
        discard;
    }
    
    // 强制锁定精灵表的左上角 (IDLE的第一帧)
    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    vec2 atlas_uv = UV * frame_size; // 相当于 (UV.x + 0) * frame_size.x 

    vec4 tex = texture(albedo_texture, atlas_uv);
    if (tex.a < 0.5) {
        discard;
    }
    
    vec3 final_rgb = tex.rgb;

    // 团队染色
    if (tex.a > 0.994 && tex.a < 0.998) {
        float brightness = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
        final_rgb = instance_color.rgb * brightness;
    } 

    // 残影特效：降低饱和度，偏向灰蓝，并降低亮度，形成“记忆残影”感
    float lum = dot(final_rgb, vec3(0.299, 0.587, 0.114));
    final_rgb = mix(vec3(lum) * vec3(0.8, 0.9, 1.0), final_rgb, 0.3) * 0.5;

    ALBEDO = final_rgb;
}
