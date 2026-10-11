// Dirt bake.
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
    vec2 p = uv * P + uSeed * 3.4;

    float macro = owFbm01(p * 0.6, P * 0.6, 4, 0.62);
    float clump = owBillow(owWarp(p * 3.0, P * 3.0, 0.6, 3), P * 3.0, 5, 0.55);
    float fine = owFbm01(p * 14.0, P * 14.0, 4, 0.5);
    float micro = owFbm01(p * 22.0, P * 22.0, 3, 0.5);

    vec3 cDry = owSRGB(vec3(0.430, 0.350, 0.255));
    vec3 cWet = owSRGB(vec3(0.185, 0.140, 0.100));
    vec3 cPale = owSRGB(vec3(0.560, 0.490, 0.385));
    vec3 c = mix(cDry, cPale, smoothstep(0.45, 0.9, macro));
    c = mix(c, cWet, smoothstep(0.55, 0.15, macro) * 0.8);
    c *= 0.94 + 0.11 * fine;
    c *= 0.975 + 0.05 * micro;

    h = 0.55 + (macro - 0.5) * 0.14 + (clump - 0.5) * 0.16 + (fine - 0.5) * 0.075;
    rough = 0.88 + (fine - 0.5) * 0.14 + (micro - 0.5) * 0.10;
    metal = 0.0;
    ao = 1.0;

    // dried mud cracks in the flat pans; the plates curl up at their edges
    float pan = smoothstep(0.35, 0.65, macro);
    float mud = owCracks(p * 2.4, P * 2.4, 0.85, 0.045, 0.35) * pan;
    h -= mud * 0.16;
    ao -= mud * 0.32;
    c = mix(c, cWet * 0.7, mud * 0.75);
    float plateLift = smoothstep(0.10, 0.0, mud) * pan;
    h += plateLift * 0.01;

    // stones of two grades
    vec4 st = owWorley(p * 11.0, P * 11.0, 1.0);
    float stone = smoothstep(0.30, 0.11, st.x) * step(0.62, st.w);
    vec3 scol = mix(owSRGB(vec3(0.330, 0.315, 0.295)), owSRGB(vec3(0.600, 0.575, 0.540)), st.z);
    c = mix(c, scol, stone * 0.6);
    h += stone * 0.085;
    rough = mix(rough, 0.52 + 0.28 * st.z, stone * 0.8);
    ao -= smoothstep(0.36, 0.28, st.x) * step(0.62, st.w) * 0.10;

    vec4 grit = owWorley(p * 22.0, P * 22.0, 1.0);
    float gritM = smoothstep(0.26, 0.08, grit.x) * step(0.55, grit.w);
    c = mix(c, mix(scol, cPale, grit.z), gritM * 0.4);
    h += gritM * 0.015;

    // dead grass and organic litter
    float litter = smoothstep(0.70, 0.86, owFbm01(owShear(p * 8.0, 1.0, 5.0), owShearPer(P * 8.0, 5.0), 4, 0.5));
    litter *= smoothstep(0.4, 0.8, macro);
    c = mix(c, owSRGB(vec3(0.330, 0.290, 0.160)), litter * 0.5);
    h += litter * 0.012;
    rough += litter * 0.05;

    // sparse moss in the damp low spots
    float moss = smoothstep(0.74, 0.92, owFbm01(p * 4.5 + 19.0, P * 4.5, 5, 0.6)) * smoothstep(0.5, 0.1, macro);
    c = mix(c, owSRGB(vec3(0.150, 0.185, 0.105)), moss * 0.65);

    float cavity = 1.0 - smoothstep(0.40, 0.70, h);
    ao -= cavity * 0.14;

    alb = clamp(c, vec3(0.02), vec3(0.72));
    rough = clamp(rough, 0.45, 0.99);
    ao = clamp(ao, 0.72, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
