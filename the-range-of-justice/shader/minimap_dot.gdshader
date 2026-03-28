shader_type spatial;
render_mode blend_mix, unshaded, depth_draw_never, cull_disabled;

uniform sampler2D tex_fog_live;
uniform vec2 map_size;
uniform vec2 map_pos;

varying vec3 world_pos;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
                       (world_pos.z - map_pos.y) / map_size.y);
    float visibility = texture(tex_fog_live, fog_uv).r;
	if (world_pos.y > 200.0) {
		visibility = 1.0;
	}
	
	ALBEDO = COLOR.xyz;
	ALPHA = 2.0 * smoothstep(0.0, 0.2, visibility);
}