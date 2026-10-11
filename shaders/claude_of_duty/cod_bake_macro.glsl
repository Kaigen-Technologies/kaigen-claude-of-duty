// The shared 4-band low-frequency variation map: r very low fbm, g warped blotches, b mid fbm, a fine fbm.
#include "common.glsl"

@vs
layout(location = 0) out vec2 v_uv;
void main() {
    v_uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(v_uv, 0.0);
}
@end

@fs
#include "cod_bake_head.glsl"

void owSurface(vec2 uv, out vec3 alb, out float h, out float rough, out float metal, out float ao) {
    const vec2 P = vec2(6.0);
    vec2 p = uv * P + uSeed * 3.0;
    float a = owFbm01(p * 0.5, P * 0.5, 4, 0.62);
    float b = owFbm01(owWarp(p * 1.0, P, 1.1, 3), P, 4, 0.58);
    float c = owFbm01(p * 2.5, P * 2.5, 4, 0.55);
    float d = owFbm01(p * 7.0, P * 7.0, 4, 0.5);
    alb = vec3(a, b, c);
    h = d;
    rough = 0.5;
    metal = 0.0;
    ao = 1.0;
}

#include "cod_bake_tail.glsl"
@end
