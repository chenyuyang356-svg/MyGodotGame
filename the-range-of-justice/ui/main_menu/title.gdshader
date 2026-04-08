shader_type canvas_item;

uniform vec4 line_color : source_color = vec4(1.0, 1.0, 1.0, 1.0); // 线条颜色
uniform float line_width : hint_range(0.0, 1.0) = 0.5;             // 线条宽度
uniform float frequency = 50.0;                                    // 线条密度
uniform float speed = 0.5;                                         // 移动速度（如果想做动效）

void fragment() {
	// 获取文字本身的透明度（让斜线只出现在文字内部）
    float text_alpha = texture(TEXTURE, UV).a;
    
    // 计算斜线逻辑：x+y 产生 45度角
    float moving_uv = (UV.x + UV.y) + TIME * speed;
    float stripe = step(line_width, fract(moving_uv * frequency));
    
    // 混合颜色
    vec4 final_color = line_color;
    final_color.a *= stripe * text_alpha; // 结合斜线和文字形状
    
    COLOR = final_color;
}
