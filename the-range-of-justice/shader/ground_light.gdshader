shader_type spatial;
render_mode blend_add, unshaded, cull_disabled;

uniform sampler2D light_texture : source_color;
uniform sampler2D tex_fog_live;
uniform vec2 map_size;
uniform vec2 map_pos;

varying vec3 world_pos;
varying vec4 instance_color;

void vertex() {
    world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    instance_color = COLOR;
}

void fragment() {
    vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
                       (world_pos.z - map_pos.y) / map_size.y);
    float visibility = texture(tex_fog_live, fog_uv).r;

    vec4 tex = texture(light_texture, UV);
    // 颜色 = 贴图颜色 * 实例颜色 * 迷雾可见度
    ALBEDO = tex.rgb * instance_color.rgb;
    ALPHA = tex.a * instance_color.a * smoothstep(0.0, 0.2, visibility);
}
