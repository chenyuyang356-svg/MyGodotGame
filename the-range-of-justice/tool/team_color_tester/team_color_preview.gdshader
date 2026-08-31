shader_type canvas_item;
render_mode blend_mix;

uniform vec4 team_color : source_color = vec4(0.2, 1.0, 0.2, 1.0);
// 0: 队伍染色, 1: 原图, 2: 遮罩高亮(品红标出254), 3: 明暗亮度分析
uniform int display_mode = 0;

void fragment() {
    vec4 tex = texture(TEXTURE, UV);
    
    if (tex.a < 0.01) {
        discard;
    }
    
    // Alpha 254 判定 (254.0 / 255.0 ≈ 0.996078)
    bool is_team_pixel = (tex.a > 0.994 && tex.a < 0.998);
    float brightness = dot(tex.rgb, vec3(0.299, 0.587, 0.114));

    if (display_mode == 1) {
        // 原图模式 (Original)
        COLOR = vec4(tex.rgb, tex.a);
    } else if (display_mode == 2) {
        // 遮罩高亮模式 (Alpha 254 染色区高亮为明亮荧光品红，普通像素为暗淡半透明)
        if (is_team_pixel) {
            COLOR = vec4(1.0, 0.08, 0.58, 1.0);
        } else {
            COLOR = vec4(vec3(brightness * 0.35), 0.35);
        }
    } else if (display_mode == 3) {
        // 明度分析模式
        if (is_team_pixel) {
            COLOR = vec4(vec3(brightness), 1.0);
        } else {
            COLOR = vec4(vec3(brightness * 0.25), 0.3);
        }
    } else {
        // 正常团队染色模式
        vec3 final_rgb = tex.rgb;
        if (is_team_pixel) {
            final_rgb = team_color.rgb * brightness;
        }
        COLOR = vec4(final_rgb, tex.a);
    }
}
