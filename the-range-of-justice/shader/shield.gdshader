shader_type spatial;
// 机甲护盾：两片相对的淡蓝色半透明圆弧，覆盖约一半周长，缓慢旋转
render_mode unshaded, cull_disabled, blend_mix, depth_draw_never;

uniform sampler2D tex_fog_live;  // 实时视野贴图
uniform vec2 map_size;
uniform vec2 map_pos;

uniform vec4 shield_color : source_color = vec4(0.45, 0.75, 1.0, 0.55);
uniform float rot_speed = 0.6;    // 旋转速度（弧度/秒）
uniform float arc_degrees = 95.0; // 每片护盾的张角（度）
uniform float radius_min = 0.34;  // 护盾内半径（相对半宽）
uniform float radius_max = 0.47;  // 护盾外半径
uniform float soft = 0.02;        // 边缘柔化

varying float shield_ratio;
varying vec3 world_pos;

void vertex() {
    // INSTANCE_CUSTOM.x = 当前护盾比例 (0~1)，护盾越低越淡
    shield_ratio = INSTANCE_CUSTOM.x;
    world_pos = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

void fragment() {
    // 迷雾可见性
    vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
                       (world_pos.z - map_pos.y) / map_size.y);
    float visibility = texture(tex_fog_live, fog_uv).r;
    if (visibility < 0.1) discard;

    vec2 uv = UV - vec2(0.5);
    float ang = atan(uv.y, uv.x);
    float r = length(uv);

    float base = TIME * rot_speed;
    float half_arc = radians(arc_degrees) * 0.5;

    // 第一片：以 base 为中心
    float d1 = min(mod(ang - base, TAU), TAU - mod(ang - base, TAU));
    float m1 = 1.0 - smoothstep(half_arc - soft, half_arc, d1);

    // 第二片：与第一片相对（错开 180°）
    float d2 = min(mod(ang - (base + PI), TAU), TAU - mod(ang - (base + PI), TAU));
    float m2 = 1.0 - smoothstep(half_arc - soft, half_arc, d2);

    float ang_mask = max(m1, m2);

    // 圆环带（护盾在单位边缘一圈）
    float ring = 1.0 - smoothstep(radius_max - soft, radius_max, r);
    float inner = 1.0 - smoothstep(radius_min, radius_min + soft, r);
    float radius_mask = ring * inner;

    float alpha = ang_mask * radius_mask;

    if (alpha < 0.02) discard;
    ALBEDO = shield_color.rgb;
    ALPHA = shield_color.a * alpha * shield_ratio;
}
