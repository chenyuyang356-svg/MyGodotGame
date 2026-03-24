shader_type spatial;
render_mode blend_mix, depth_draw_always, cull_disabled, unshaded;

uniform sampler2D albedo_tex : source_color;
uniform sampler2D tex_fog_live; // 引入迷雾
uniform vec2 map_size;
uniform vec2 map_pos;
uniform float modulate;

varying float v_life_ratio;
varying vec3 v_world_pos;

void vertex() {
	v_life_ratio = INSTANCE_CUSTOM.x; // 获取 C++ 传来的寿命比例
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec4 tex = texture(albedo_tex, UV);
	
	// 计算迷雾遮罩
	vec2 fog_uv = (v_world_pos.xz - map_pos) / map_size;
	float fog_val = texture(tex_fog_live, fog_uv).r;
	
	ALBEDO = tex.rgb;
	
	// 混合多个淡出效果：
	// 1. 贴图本身的 Alpha
	// 2. 随寿命结束淡出 (1.0 - v_life_ratio)
	// 3. 迷雾视野 (fog_val)
	ALPHA = tex.a * (1.0 - v_life_ratio) * fog_val * modulate;
	
	// 性能优化：如果完全透明则放弃像素
	if (ALPHA < 0.05) {
		discard;
	}
}
