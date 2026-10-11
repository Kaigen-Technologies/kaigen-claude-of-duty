// Moulded rubber bake: pebble grain, mould seam, chalky scuffs, ozone cracking.
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
    vec2 p = uv * P + uSeed * 9.6;

    vec4 pb = owWorley(p * 12.0, P * 12.0, 1.0);
    float pebble = smoothstep(0.42, 0.10, pb.x);
    float fine = owFbm01(p * 12.0, P * 12.0, 3, 0.5);
    float macro = owFbm01(p * 1.5, P * 1.5, 4, 0.6);

    h = 0.60 + pebble * 0.10 + (fine - 0.5) * 0.02 + (macro - 0.5) * 0.03;
    // 0.20 sRGB is 0.031 linear: darker would sit under the 0.02 albedo floor and flatten the surface
    vec3 c = owSRGB(vec3(0.200, 0.200, 0.206));
    c *= 0.85 + 0.25 * (pebble * 0.5 + 0.5);
    c *= 0.94 + 0.10 * fine;

    rough = 0.88 - pebble * 0.06 + (fine - 0.5) * 0.08;
    metal = 0.0;
    ao = mix(0.6, 1.0, pebble * 0.5 + 0.5);

    float seam = 1.0 - smoothstep(0.0, 0.012, abs(fract(uv.y * 2.0 + 0.5) - 0.5));
    h += seam * 0.03;
    c *= 1.0 + seam * 0.35;
    rough -= seam * 0.10;

    float scuff = smoothstep(0.55, 0.88, owFbm01(owWarp(p * 3.0, P * 3.0, 0.8, 3), P * 3.0, 4, 0.55));
    c = mix(c, owSRGB(vec3(0.220, 0.218, 0.212)), scuff * 0.45);
    rough += scuff * 0.06;
    h -= scuff * 0.015;

    float crack = owCracks(p * 7.0, P * 7.0, 0.9, 0.028, 0.62);
    h -= crack * 0.06;
    c *= 1.0 - crack * 0.35;
    ao -= crack * 0.35;

    float dust = smoothstep(0.5, 0.9, owFbm01(p * 8.0, P * 8.0, 4, 0.5));
    c = mix(c, owSRGB(vec3(0.290, 0.275, 0.250)), dust * 0.16);

    alb = clamp(c, vec3(0.02), vec3(0.35));
    rough = clamp(rough, 0.55, 0.99);
    ao = clamp(ao, 0.3, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
