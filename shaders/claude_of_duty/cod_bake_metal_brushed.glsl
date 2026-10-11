// Brushed steel bake: fibres along X, score lines, handling scratches, fingerprints and grime.
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
    vec2 p = uv * P + uSeed * 15.1;

    // brushing runs along X: heavy shear stretches the noise into fibres
    vec2 bp = owShear(p, 0.0, 64.0);
    vec2 BP = owShearPer(P, 64.0);
    float brush1 = owFbm01(bp * 2.0, BP * 2.0, 4, 0.5);
    float brush2 = owFbm01(bp * 8.0 + 3.0, BP * 8.0, 3, 0.5);
    float brush3 = owFbm01(owShear(p * 4.0, 0.0, 24.0), owShearPer(P * 4.0, 24.0), 3, 0.5);
    float brush = brush1 * 0.5 + brush2 * 0.32 + brush3 * 0.18;

    float macro = owFbm01(p * 0.9, P * 0.9, 3, 0.6);

    vec3 c = owSRGB(vec3(0.560, 0.565, 0.575));
    c *= 0.93 + 0.13 * brush;
    c *= 0.97 + 0.06 * macro;

    metal = 1.0;
    rough = 0.22 + brush * 0.24 + (macro - 0.5) * 0.06;
    h = 0.78 + (brush - 0.5) * 0.012;
    ao = 1.0;

    float score = owScratches(p * 1.0, P, 40.0, 0.0, 0.60);
    rough += score * 0.22;
    h -= score * 0.006;
    c *= 1.0 - score * 0.05;

    float crossScr = owScratches(p * 3.0, P * 3.0, 8.0, 3.0, 0.70) * 0.7;
    rough += crossScr * 0.20;
    h -= crossScr * 0.004;

    // shallow wide dents that break the reflection
    float dent = owFbm01(p * 3.0 + 7.0, P * 3.0, 3, 0.6);
    h += (dent - 0.5) * 0.05;

    // fingerprints and grease smudges sell brushed metal
    float smudge = smoothstep(0.58, 0.86, owFbm01(owWarp(p * 2.2 + 19.0, P * 2.2, 0.7, 3), P * 2.2, 4, 0.55));
    rough += smudge * 0.22;
    c *= 1.0 - smudge * 0.06;
    metal -= smudge * 0.10;

    float grime = smoothstep(0.66, 0.95, owFbm01(p * 5.0, P * 5.0, 4, 0.55));
    c = mix(c, owSRGB(vec3(0.180, 0.175, 0.165)), grime * 0.35);
    rough += grime * 0.18;
    metal -= grime * 0.35;

    alb = clamp(c, vec3(0.02), vec3(0.88));
    rough = clamp(rough, 0.08, 0.95);
    metal = clamp(metal, 0.0, 1.0);
    ao = clamp(ao, 0.4, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
