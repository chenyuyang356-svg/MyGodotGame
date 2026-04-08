shader_type spatial;

// unshaded: 不受 3D 光照影响
// blend_mix: 标准透明混合
render_mode blend_mix, depth_draw_opaque, cull_back;

uniform sampler2D albedo_texture : source_color, filter_nearest;
uniform vec2 atlas_size; // Atlas 贴图的总格子数，例如 vec2(16.0, 16.0)

varying vec2 v_atlas_coords;

void vertex() {
    // 将 Atlas 坐标传递给 fragment
    v_atlas_coords = INSTANCE_CUSTOM.xy;
    // 保持原始 UV (0.0 到 1.0) 不变，传递给 fragment 处理
}

void fragment() {
    // 1. 获取纹理的实际像素尺寸
    vec2 tex_size = vec2(textureSize(albedo_texture, 0));
    
    // 2. 计算单个像素在 UV 空间的大小
    vec2 pixel_size = 1.0 / tex_size;
    
    // 3. 计算单个瓦片在整个图集中的比例 (0.0 ~ 1.0 之间)
    // 例如 16x16 的图集，每个瓦片占比 0.0625
    vec2 tile_unit = 1.0 / atlas_size;
    
    // --- 核心修复：UV 缩进 ---
    // 我们将原始 UV (0~1) 映射到 (epsilon ~ 1-epsilon) 之间
    // 这里的 0.5 表示缩进半个像素，这是消除缝隙的标准做法
    float margin = 0.5; 
    vec2 inset_uv = mix(pixel_size * margin, tile_unit - pixel_size * margin, UV);
    
    // 4. 计算最终的采样 UV
    // 公式：(当前瓦片坐标 * 每个瓦片的大小) + 缩进后的内部分量
    vec2 final_uv = (v_atlas_coords * tile_unit) + inset_uv;
    
    vec4 tex_color = texture(albedo_texture, final_uv);
    ALBEDO = tex_color.rgb;
    ALPHA = tex_color.a;
}
