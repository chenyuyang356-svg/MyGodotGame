shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_always;

uniform vec3 bg_color : source_color = vec3(0.2, 0.0, 0.0);

varying float hp_ratio;
varying vec3 color;

void vertex() {
	// INSTANCE_CUSTOM.x 存储血量百分比 (0.0 到 1.0)
    hp_ratio = INSTANCE_CUSTOM.x;
	color = COLOR.rgb;
}

void fragment() { 
    if (UV.x < hp_ratio) {
        ALBEDO = color;
    } else {
        ALBEDO = bg_color;
    }
}

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
