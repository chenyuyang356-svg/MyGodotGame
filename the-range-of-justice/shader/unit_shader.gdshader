shader_type spatial;
// unshaded: 不受 3D 灯光影响
// cull_disabled: 双面可见
// depth_draw_always: 强制写入深度，保证 Y-Sort 准确
render_mode unshaded, cull_disabled, depth_draw_always;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform int h_frames = 1;
uniform int v_frames = 1;

varying vec4 custom_data;
varying vec4 instance_color;

void vertex() {
    custom_data = INSTANCE_CUSTOM;
    instance_color = COLOR;
    
    // 让 Quad 始终面向摄像机（看板模式），如果你不需要可以删掉
    // MODELVIEW_MATRIX = VIEW_MATRIX * mat4(vec4(normalize(cross(vec3(0.0, 1.0, 0.0), VIEW_MATRIX[2].xyz)), 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(normalize(cross(VIEW_MATRIX[0].xyz, vec3(0.0, 1.0, 0.0))), 0.0), WORLD_MATRIX[3]);
}

void fragment() {
    float frame = floor(custom_data.x);
    float row = floor(custom_data.y); 
	float modulate = custom_data.z;

    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    
    // 计算图集 UV
    vec2 atlas_uv = UV;
    atlas_uv.x = (atlas_uv.x + frame) * frame_size.x;
    atlas_uv.y = (atlas_uv.y + row) * frame_size.y;

    vec4 tex = texture(albedo_texture, atlas_uv);
    
	if (tex.a < 0.5) {
        discard;
    }
	
    vec3 final_rgb = tex.rgb;

    // 团队染色逻辑 (检查 Alpha 是否接近 254/255)
    // 使用一个小范围误差 (epsilon) 以防浮点数精度问题
    // 254.0 / 255.0 = 0.99607843
    if (tex.a > 0.994 && tex.a < 0.998) {
        // 使用加权亮度公式（最准确，符合人眼对不同颜色亮度的感知）
        float brightness = dot(tex.rgb, vec3(0.299, 0.587, 0.114));

        // 将团队颜色乘以亮度
        // 这样：浅绿色会变浅团队色，深绿色（阴影）会变深团队色
        final_rgb = instance_color.rgb * brightness;
    } else {
        // 如果是普通像素，可以选乘实例颜色（如果你想让整个建筑也带一点色调）
        // 或者直接使用原色：final_rgb = tex.rgb;
        final_rgb = tex.rgb; 
    }

    ALBEDO = final_rgb * modulate;
}
