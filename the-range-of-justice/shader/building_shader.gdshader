shader_type spatial;
// unshaded: 不受 3D 灯光影响
// cull_disabled: 双面可见
// blend_mix, depth_prepass_alpha: 深度预通过与半透明混合
render_mode unshaded, cull_disabled, blend_mix, depth_prepass_alpha;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform int h_frames = 1;
uniform int v_frames = 1;

uniform sampler2D tex_fog_live;  // 实时视野贴图
uniform vec2 map_size;          // 地图的世界尺寸
uniform vec2 map_pos;          // 地图的左上角

varying vec4 custom_data;
varying vec4 instance_color;
varying vec3 world_pos;

void vertex() {
    custom_data = INSTANCE_CUSTOM;
    instance_color = COLOR;
    
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    // 让 Quad 始终面向摄像机（看板模式），如果你不需要可以删掉
    // MODELVIEW_MATRIX = VIEW_MATRIX * mat4(vec4(normalize(cross(vec3(0.0, 1.0, 0.0), VIEW_MATRIX[2].xyz)), 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(normalize(cross(VIEW_MATRIX[0].xyz, vec3(0.0, 1.0, 0.0))), 0.0), WORLD_MATRIX[3]);
}

void fragment() {
	vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
	 (world_pos.z - map_pos.y) / map_size.y);
	float visibility = texture(tex_fog_live, fog_uv).r;
	float fog_alpha = smoothstep(0.0, 0.2, visibility);
	
    float frame = floor(custom_data.x);
    float row = floor(custom_data.y); 
	float modulate = custom_data.z;
	float construction_site_visibility = custom_data.w;

    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    
    // 计算图集 UV
    vec2 atlas_uv = UV;
    atlas_uv.x = (atlas_uv.x + frame) * frame_size.x;
    // 建造中(BUILDING)行号为 -1，采样时按第 0 行处理
    atlas_uv.y = (atlas_uv.y + max(row, 0.0)) * frame_size.y;

    vec4 tex = texture(albedo_texture, atlas_uv);
    
	if (tex.a < 0.01) {
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

	// --- 建造状态特殊效果 ---
    // 判断是否为建造中（C++ 传 row = -1）
    bool is_constructing = row < -0.5;
    float current_alpha = 1.0;

    if (is_constructing) {
        // 1. 变灰：计算亮度并应用
        float gray = dot(final_rgb, vec3(0.299, 0.587, 0.114));
        final_rgb = vec3(gray);
        
        // 2. 变透明：设置一个固定的透明度（如 0.6）
        current_alpha = 0.4;
		
		if (construction_site_visibility > 0.5) {
			fog_alpha = 1.0;	
		}
    }

    ALBEDO = final_rgb * modulate;
	ALPHA = tex.a * fog_alpha * current_alpha;
}
