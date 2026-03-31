shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_always;

uniform vec3 bg_color : source_color = vec3(0.2, 0.0, 0.0);

uniform sampler2D tex_fog_live;  // 实时视野贴图
uniform vec2 map_size;          // 地图的世界尺寸
uniform vec2 map_pos;          // 地图的左上角

varying float hp_ratio;
varying vec3 color;
varying vec3 world_pos;

void vertex() {
	// INSTANCE_CUSTOM.x 存储血量百分比 (0.0 到 1.0)
    hp_ratio = INSTANCE_CUSTOM.x;
	color = COLOR.rgb;
	
	world_pos = (MODEL_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
}

void fragment() {
	vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
	 (world_pos.z - map_pos.y) / map_size.y);
	float visibility = texture(tex_fog_live, fog_uv).r;
	if (visibility < 0.1) {
        discard;
    }
	 
    if (UV.x < hp_ratio) {
        ALBEDO = color;
    } else {
        ALBEDO = bg_color;
    }
	ALPHA *= 0.6;
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
