// Window glass bake: near-black albedo, smears, dust, water spots, fine scratches.
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
    vec2 p = uv * P + uSeed * 2.2;

    float smear = owFbm01(owShear(p * 3.0, 1.0, 6.0), owShearPer(P * 3.0, 6.0), 4, 0.5);
    float dustF = owFbm01(p * 5.0, P * 5.0, 5, 0.55);
    float spots = owWorley(p * 24.0, P * 24.0, 1.0).x;
    float fine = owFbm01(p * 12.0, P * 12.0, 3, 0.5);

    // glass is almost black in albedo; the look comes from reflections
    vec3 c = owSRGB(vec3(0.045, 0.050, 0.052));

    float dirty = smoothstep(0.45, 0.85, dustF);
    c = mix(c, owSRGB(vec3(0.300, 0.290, 0.265)), dirty * 0.35);

    rough = 0.045 + smear * 0.10 * smoothstep(0.3, 0.9, dustF) + dirty * 0.22;
    rough += smoothstep(0.30, 0.05, spots) * 0.25; // water spots
    rough += (fine - 0.5) * 0.02;

    float scr = owScratches(p * 2.0, P * 2.0, 24.0, 1.0, 0.70);
    rough += scr * 0.25;
    c += scr * 0.02;

    h = 0.5 + (smear - 0.5) * 0.004;
    metal = 0.0;
    ao = 1.0 - dirty * 0.1;

    alb = clamp(c, vec3(0.02), vec3(0.5));
    rough = clamp(rough, 0.02, 0.7);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
