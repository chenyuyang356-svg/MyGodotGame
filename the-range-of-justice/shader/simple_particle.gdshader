shader_type spatial;
// 使用 blend_mix 混合，depth_draw_never 防止遮挡单位，unshaded 保证亮度
render_mode blend_mix, depth_draw_never, cull_disabled, unshaded;

uniform sampler2D albedo_tex : source_color;
uniform sampler2D tex_fog_live; 
uniform vec2 map_size;
uniform vec2 map_pos;
uniform float modulate = 1.0;

varying float v_life_ratio;
varying vec3 v_world_pos;
varying float v_is_ripple; // 通过 custom_data.y 传递，1.0 表示是波纹

void vertex() {
	v_life_ratio = INSTANCE_CUSTOM.x; 
	v_is_ripple = INSTANCE_CUSTOM.y; 
	
	// 如果是波纹类型，让顶点随寿命扩张 (从 0.5倍 到 2.0倍)
	if (v_is_ripple > 0.5) {
		float scale_factor = 0.5 + v_life_ratio * 1.5;
		VERTEX.xyz *= scale_factor;
	}
	
	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec4 tex = texture(albedo_tex, UV);
	
	vec2 fog_uv = (v_world_pos.xz - map_pos) / map_size;
	float fog_val = texture(tex_fog_live, fog_uv).r;
	
	ALBEDO = tex.rgb;
	
	// 1. 贴图 Alpha
	// 2. 寿命后期平滑淡出 (1.0 - v_life_ratio)
	// 3. 寿命极早期淡入 (防止突然出现，使用 smoothstep)
	float fade = (1.0 - v_life_ratio) * smoothstep(0.0, 0.1, v_life_ratio);
	
	ALPHA = tex.a * fade * fog_val * modulate;
	
	if (ALPHA < 0.01) discard;
}
