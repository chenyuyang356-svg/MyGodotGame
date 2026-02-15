shader_type canvas_item;

uniform int h_frames = 2;
uniform int v_frames = 2;

// 定义一个 varying 变量，用于从 vertex 传递到 fragment
varying vec4 custom_data;
varying vec4 color;

void vertex() {
    // 在 vertex 函数中捕捉实例的自定义数据
    custom_data = INSTANCE_CUSTOM;
	color = COLOR;
}

void fragment() {
    // 此时可以使用从 vertex 传过来的 custom_data 了
    float frame = floor(custom_data.x);
    float row = floor(custom_data.y); 

    vec2 size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    
    vec2 uv = UV;
    // 计算当前帧在图集中的实际 UV 偏移
    uv.x = (uv.x + frame) * size.x;
    uv.y = (uv.y + row) * size.y;

	COLOR = texture(TEXTURE, uv) * color;
}
