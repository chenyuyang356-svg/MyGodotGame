shader_type canvas_item;

void vertex() {}

void fragment() {
    float flicker = 1.02 + 0.02 * sin(TIME * 2.0);
    COLOR = texture(TEXTURE, UV) * flicker;
}