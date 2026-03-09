       shader_type spatial;
       render_mode unshaded, blend_mix, depth_draw_never;

       uniform sampler2D fog_data_tex;
       uniform int fog_mode = 2; // 0:None, 1:Light, 2:Heavy

       void fragment() {
           float val = texture(fog_data_tex, UV).r;
           vec3 fog_color = vec3(0.05, 0.05, 0.05);
           float alpha = 1.0;

           if (fog_mode == 0) {
               alpha = 0.0; // 无雾
           } else if (fog_mode == 1) {
               // 轻雾：只要值 > 0 (探索过)，就不显出雾
               alpha = (val > 0.01) ? 0.0 : 1.0;
           } else {
               // 重雾：1.0透明，0.5半透(阴影)，0.0纯黑
               if (val > 0.9) alpha = 0.0;
               else if (val > 0.4) alpha = 0.6; // 阴影深度
               else alpha = 0.9;
           }
           ALBEDO = fog_color;
           ALPHA = alpha;
       }

//void light() {
//	// Called for every pixel for every light affecting the material.
//	// Uncomment to replace the default light processing function with this one.
//}
