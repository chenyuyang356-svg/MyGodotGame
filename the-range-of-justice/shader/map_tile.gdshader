shader_type spatial;
// unshaded: 不受 3D 光照影响（如果你想让地图受灯光影响，可以删掉这个）
// blend_mix: 标准透明混合
render_mode unshaded, blend_mix, depth_draw_opaque, cull_back;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform vec2 atlas_size; // Atlas 贴图的总格子数，例如 vec2(16.0, 16.0)

void vertex() {
    // 1. 获取当前实例在 MultiMesh 中存储的自定义数据
    // INSTANCE_CUSTOM_DATA.xy 存储的是瓦片的 Atlas 坐标
    vec2 atlas_coords = INSTANCE_CUSTOM.xy;
    
    // 2. 重新计算 UV 坐标
    // 将 0.0~1.0 的 Quad UV 映射到 Atlas 贴图中对应的格子区域
    UV = (UV + atlas_coords) / atlas_size;
}

void fragment() {
    vec4 tex_color = texture(albedo_texture, UV);
    ALBEDO = tex_color.rgb;
    ALPHA = tex_color.a;
}
