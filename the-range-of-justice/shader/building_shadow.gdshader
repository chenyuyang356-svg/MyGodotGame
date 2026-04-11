shader_type spatial;
render_mode unshaded, blend_mix, depth_prepass_alpha, cull_disabled;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform int h_frames = 1;
uniform int v_frames = 1;
uniform vec4 shadow_color : source_color = vec4(0.0, 0.0, 0.0, 0.5); // 半透明黑

uniform sampler2D tex_fog_live;  // 实时视野贴图
uniform vec2 map_size;          // 地图的世界尺寸
uniform vec2 map_pos;          // 地图的左上角

varying vec4 custom_data;
varying vec3 world_pos;

void vertex() {
    // 获取实例数据
    custom_data = INSTANCE_CUSTOM;
	
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 fog_uv = vec2((world_pos.x - map_pos.x) / map_size.x,
	 (world_pos.z - map_pos.y) / map_size.y);
	float visibility = texture(tex_fog_live, fog_uv).r;
	
	float frame = floor(custom_data.x);
    float row = floor(custom_data.y); 

    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    
    // 计算图集 UV
    vec2 atlas_uv = UV;
    atlas_uv.x = (atlas_uv.x + frame) * frame_size.x;
    atlas_uv.y = (atlas_uv.y + row) * frame_size.y;
	
    vec4 tex = texture(albedo_texture, atlas_uv);
    // 影子的颜色由纹理的透明度决定
    ALBEDO = shadow_color.rgb;
    ALPHA = tex.a * shadow_color.a;
	ALPHA *= smoothstep(0.0, 0.2, visibility);
}