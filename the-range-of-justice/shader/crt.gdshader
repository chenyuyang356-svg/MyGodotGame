shader_type canvas_item;

uniform float scanline_count : hint_range(0, 1080) = 300.0;
uniform sampler2D SCREEN_TEXTURE : hint_screen_texture, filter_linear_mipmap;

void fragment() {
    vec4 screen_color = texture(SCREEN_TEXTURE, SCREEN_UV);
    
    // 模拟横向扫描线
    float scanline = sin(SCREEN_UV.y * scanline_count * 2.0);
    scanline = (scanline * 0.1) + 0.9; // 调节明暗对比
    
    // 模拟屏幕边缘暗角 (Vignette)
    float vignette = distance(UV, vec2(0.5));
    vignette = pow(1.0 - vignette, 0.5);
    
    COLOR = screen_color * scanline * vignette;
}
