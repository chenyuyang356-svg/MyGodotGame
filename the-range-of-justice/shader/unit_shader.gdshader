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

    vec2 frame_size = vec2(1.0 / float(h_frames), 1.0 / float(v_frames));
    
    // 计算图集 UV
    vec2 atlas_uv = UV;
    atlas_uv.x = (atlas_uv.x + frame) * frame_size.x;
    atlas_uv.y = (atlas_uv.y + row) * frame_size.y;

    vec4 tex = texture(albedo_texture, atlas_uv);
    
    // 3D 排序核心：Alpha 裁剪
    // 只有透明度大于 0.5 的像素才会写入深度缓冲并被渲染
    if (tex.a < 0.5) {
        discard;
    }

    ALBEDO = tex.rgb * instance_color.rgb;
    // 如果需要半透明效果（如影子），可以将材质设为透明，但通常单位使用 discard 性能最好
}
