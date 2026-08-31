shader_type spatial;
// 加色混合版本: 用于炮口闪光等发光特效, 亮部会叠加而不是覆盖背景
render_mode blend_add, depth_draw_never, cull_disabled, unshaded;

uniform sampler2D albedo_tex : source_color;
uniform sampler2D tex_fog_live;
uniform vec2 map_size;
uniform vec2 map_pos;
uniform float modulate = 1.0;

// --- 像素风格程序化模式 (纯数学生成离散像素点阵与色阶) ---
uniform bool procedural = false;
uniform vec4 flash_color : source_color = vec4(1.0, 0.55, 0.1, 1.0); // 炽橙主色
uniform float pixel_size = 1.0;         // 像素网格大小 (对齐游戏16px世界网格)
uniform int palette_type = 0;           // 0=火炮(白/黄/橙/红), 1=机枪, 2=重炮, 3=能量(白/青/蓝/紫)
uniform float core_radius = 0.35;       // 白核相对尺寸
uniform float cone_length = 0.90;       // 前向火舌长度
uniform float side_strength = 0.50;     // 侧向制退器火羽强度
uniform float side_angle = 0.85;        // 制退器喷流夹角 (弧度, 约 50°)
uniform float side_length = 0.60;       // 制退器火羽长度
uniform float spark_intensity = 0.40;   // 飞溅方块火星数量

varying float v_life_ratio;
varying vec3 v_world_pos;
varying float v_is_ripple;
varying float v_custom_angle;  // custom_data.z: 炮口朝向 (世界弧度)
varying vec3 v_center;         // 粒子中心世界坐标
varying float v_half_size;     // 四边形世界空间半宽

void vertex() {
	v_life_ratio = INSTANCE_CUSTOM.x;
	v_is_ripple = INSTANCE_CUSTOM.y;
	v_custom_angle = INSTANCE_CUSTOM.z;

	if (v_is_ripple > 0.5) {
		float scale_factor = 0.5 + v_life_ratio * 1.5;
		VERTEX.xyz *= scale_factor;
	}

	v_world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	v_center = MODEL_MATRIX[3].xyz;
	v_half_size = length(MODEL_MATRIX[0].xyz);
}

// 离散色阶调色板映射 (Pixel Palette Mapping)
vec3 get_pixel_color(int level) {
	if (level >= 4) return vec3(1.0, 1.0, 0.95); // 纯白白热
	if (palette_type == 3) {
		// 能量脉冲: 白 -> 亮青 -> 霓虹蓝 -> 深紫
		if (level == 3) return vec3(0.40, 0.95, 1.0);
		if (level == 2) return vec3(0.15, 0.55, 1.0);
		return vec3(0.45, 0.10, 0.85);
	} else if (palette_type == 2) {
		// 导弹尾焰与排气烟幕: 白 -> 炽橙尾焰 -> 浅灰浓烟 -> 深灰排气
		if (level == 3) return vec3(1.0, 0.72, 0.18);
		if (level == 2) return vec3(0.82, 0.84, 0.88);
		return vec3(0.38, 0.40, 0.45);
	} else if (palette_type == 1) {
		// 速射机枪: 白 -> 金白 -> 明黄 -> 橙褐
		if (level == 3) return vec3(1.0, 0.95, 0.45);
		if (level == 2) return vec3(1.0, 0.72, 0.18);
		return vec3(0.80, 0.30, 0.08);
	} else {
		// 经典火炮/重型火炮: 白 -> 金黄 -> 炽橙 -> 暗红轮廓
		if (level == 3) return vec3(1.0, 0.86, 0.22);
		if (level == 2) return flash_color.rgb;
		return vec3(0.75, 0.15, 0.05);
	}
}

// 纯正像素风格炮口火焰与导弹发射生成器
vec4 gen_pixel_flash() {
	vec2 offset = v_world_pos.xz - v_center.xz;
	
	// 旋转到局部炮口坐标系 (X=沿炮管前方, Y=侧向垂直)
	float cos_a = cos(-v_custom_angle);
	float sin_a = sin(-v_custom_angle);
	vec2 local_p = vec2(
		offset.x * cos_a - offset.y * sin_a,
		offset.x * sin_a + offset.y * cos_a
	);

	// 像素网格离散量化 (Pixel Quantization)
	float px = max(pixel_size, 0.5);
	vec2 grid_p = floor(local_p / px + 0.5) * px;

	float scale = max(v_half_size, 1.0);
	float gx = grid_p.x / scale;
	float gy = grid_p.y / scale;
	float gr = length(vec2(gx, gy));

	// 逐帧像素动画阶段 (4 帧爆裂节奏)
	float life = clamp(v_life_ratio, 0.0, 1.0);
	int frame = int(life * 4.0);
	if (frame >= 4) return vec4(0.0);

	int level = 0;

	if (frame == 0) {
		// Frame 0: 瞬时点火 —— 筒口初速耀斑 (纯白光点 + 筒口启动光环)
		float r_px = length(grid_p);
		float white_r = max(px * 1.0, scale * core_radius * 0.42);
		float yellow_r = white_r + px * 1.0;
		float orange_r = yellow_r + px * 0.9;
		
		float manhattan = abs(grid_p.x) + abs(grid_p.y);
		if (manhattan <= white_r * 1.35 || r_px <= white_r) {
			level = 4; // 纯白高亮点火核
		} else if (manhattan <= yellow_r * 1.38 || r_px <= yellow_r) {
			level = 3; // 明黄喷流/过渡
		} else if (manhattan <= orange_r * 1.35) {
			level = 2; // 外围初生烟圈/微光
		}
	}
	else if (frame == 1) {
		// Frame 1: 强力推进 (狭长火箭尾焰 + 侧向/后向导流排气烟羽)
		float cone_x = gx - 0.1;
		float cone_len = cone_length * 0.95;
		float cone_shape = (cone_x > 0.0 && cone_x < cone_len) ? (abs(gy) / (1.0 - cone_x / cone_len * 0.6)) : 99.0;
		
		// 侧喷与背部导流羽 (斜角/钝角)
		float s_tan = tan(side_angle);
		float side_diff1 = abs(gy - gx * s_tan);
		float side_diff2 = abs(-gy - gx * s_tan);
		bool is_side = (side_strength > 0.15) && (side_diff1 < 0.18 || side_diff2 < 0.18) && (abs(gx) < side_length * 0.85);

		if (gr <= core_radius * 0.4 && abs(gx) < 0.25) {
			level = 4; // 喷口高热白核
		} else if (cone_shape <= 0.14 && cone_x > 0.0) {
			level = 3; // 火箭尾焰内芯 (金橙)
		} else if (cone_shape <= 0.26 || is_side) {
			level = 2; // 尾焰主体 / 排气白烟
		} else if (cone_shape <= 0.38 || (is_side && (side_diff1 < 0.25 || side_diff2 < 0.25))) {
			level = 1; // 导流烟幕外轮廓
		}
	}
	else if (frame == 2) {
		// Frame 2: 弹体离架 (尾焰前飞脱离 + 发射口周围扩散方块烟雾团)
		vec2 burst_center = vec2(0.45, 0.0);
		float d_burst = length(vec2(gx - burst_center.x, gy * 1.5));

		// 独立的方形像素烟雾与火星团
		bool smoke_puff = false;
		if (spark_intensity > 0.1) {
			vec2 sp1 = vec2(0.65, 0.22);
			vec2 sp2 = vec2(0.72, -0.18);
			vec2 sp3 = vec2(-0.25, 0.32); // 筒口后侧烟团
			vec2 sp4 = vec2(-0.25, -0.32);
			vec2 sp5 = vec2(0.15, 0.38);
			vec2 sp6 = vec2(0.15, -0.38);
			float sp_r = px / scale * 1.25;
			if (length(vec2(gx, gy) - sp1) <= sp_r ||
				length(vec2(gx, gy) - sp2) <= sp_r ||
				length(vec2(gx, gy) - sp3) <= sp_r ||
				length(vec2(gx, gy) - sp4) <= sp_r ||
				length(vec2(gx, gy) - sp5) <= sp_r ||
				length(vec2(gx, gy) - sp6) <= sp_r) {
				smoke_puff = true;
			}
		}

		if (d_burst <= 0.16) {
			level = 3; // 离去尾焰
		} else if (d_burst <= 0.32 || smoke_puff) {
			level = 2; // 膨胀浓烟团
		} else if (d_burst <= 0.48) {
			level = 1; // 烟雾外缘
		}
	}
	else if (frame == 3) {
		// Frame 3: 烟幕升腾消散 (飘散的暗灰色像素烟雾碎屑)
		vec2 em1 = vec2(0.55, 0.15);
		vec2 em2 = vec2(0.68, -0.12);
		vec2 em3 = vec2(-0.20, 0.35);
		vec2 em4 = vec2(-0.20, -0.35);
		float em_r = px / scale * 1.1;
		if (length(vec2(gx, gy) - em1) <= em_r ||
			length(vec2(gx, gy) - em2) <= em_r ||
			length(vec2(gx, gy) - em3) <= em_r ||
			length(vec2(gx, gy) - em4) <= em_r) {
			level = 1;
		}
	}

	if (level == 0) return vec4(0.0);

	vec3 col = get_pixel_color(level);
	return vec4(col, 1.0);
}

void fragment() {
	vec4 tex;
	if (procedural) {
		tex = gen_pixel_flash();
	} else {
		tex = texture(albedo_tex, UV);
	}

	vec2 fog_uv = (v_world_pos.xz - map_pos) / map_size;
	float fog_val = texture(tex_fog_live, fog_uv).r;

	ALBEDO = tex.rgb;

	// 像素风格硬边缘剔除: 像素点非 0 即 1, 杜绝半透明抗锯齿模糊
	if (tex.a < 0.5) discard;

	ALPHA = tex.a * fog_val * modulate;
}
