// The shared micro detail map: rgb micro albedo variation (linear data), alpha micro height; one 0.25 m tile.
// At 1024 texels a texel is 0.244 mm; every band stays at or under K = 20 (6.4 texels) so it survives the first mips.
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
    const vec2 P = vec2(8.0);
    vec2 p = uv * P + uSeed;
    float a = owFbm01(p * 3.0, P * 3.0, 4, 0.55);
    float b = owFbm01(p * 9.0, P * 9.0, 4, 0.52);
    vec4 pores = owWorley(p * 8.0, P * 8.0, 1.0);
    vec4 grit = owWorley(p * 20.0, P * 20.0, 1.0);
    float scr = owScratches(p * 2.5, P * 2.5, 16.0, 1.0, 0.66) + owScratches(p * 4.0 + 5.0, P * 4.0, 11.0, -2.0, 0.70) * 0.8;
    float gritA = smoothstep(0.34, 0.08, pores.x) * step(0.38, pores.z);
    float gritB = smoothstep(0.30, 0.06, grit.x) * step(0.34, grit.z);
    float pit = smoothstep(0.26, 0.0, pores.x) * step(0.72, pores.w);
    h = 0.5 + (a - 0.5) * 0.34 + (b - 0.5) * 0.26;
    h -= pit * 0.38;
    h += gritA * 0.26 * (0.5 + grit.z) + gritB * 0.20;
    h -= clamp(scr, 0.0, 1.0) * 0.18;
    alb = vec3(0.5 + (a - 0.5) * 0.22 + (b - 0.5) * 0.15 + gritA * 0.16 + gritB * 0.10 - pit * 0.14);
    rough = 0.5 + (b - 0.5) * 0.5;
    metal = 0.0;
    ao = 1.0 - pit * 0.45 - gritB * 0.10;
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
