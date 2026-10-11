// Canvas bake: plain weave, drape folds, threadbare patches, pulled threads, stains; tint_a and tint_b are the two thread tones.
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
    const float THREADS = 96.0;
    vec2 p = uv * P + uSeed * 3.9;

    // plain weave: warp over weft on alternating cells
    vec2 t = uv * THREADS;
    vec2 cell = floor(t);
    vec2 f = fract(t) - 0.5;
    float over = mod(cell.x + cell.y, 2.0);

    float warpProfile = cos(f.x * 3.14159);
    float weftProfile = cos(f.y * 3.14159);
    float top = mix(warpProfile, weftProfile, over);
    float bot = mix(weftProfile, warpProfile, over) * 0.45;
    float weave = max(top, bot);
    float threadId = owHash12(cell + uSeed);

    float fuzz = owFbm01(p * 12.0, P * 12.0, 3, 0.55);
    float slub = owFbm01(p * 14.0, P * 14.0, 4, 0.5);
    float macro = owFbm01(p * 1.2, P * 1.2, 4, 0.6);

    vec3 cA = uTintA;
    vec3 cB = uTintB;
    vec3 c = mix(cA, cB, threadId * 0.6 + slub * 0.4);
    c *= 0.865 + 0.215 * (weave * 0.5 + 0.5);
    c *= 0.960 + 0.075 * fuzz;
    c *= 0.90 + 0.20 * macro;

    h = 0.55 + weave * 0.30 + (fuzz - 0.5) * 0.03 + (slub - 0.5) * 0.05;
    rough = 0.86 + (1.0 - weave) * 0.08 + (fuzz - 0.5) * 0.06;
    metal = 0.0;
    ao = mix(0.82, 1.0, smoothstep(-0.4, 0.9, weave));

    // drape folds: soft parallel ridges about a hand's width apart, wandering as they run
    float foldC = uv.y * 2.6 + uv.x * 0.55 + owFbm01(p * 0.9, P * 0.9, 3, 0.62) * 2.2;
    float foldT = abs(fract(foldC) - 0.5) * 2.0;
    float crest = 1.0 - foldT;
    float foldR = owHash11(floor(foldC) * 2.13 + uSeed);
    float fold = crest * crest * (0.55 + 0.75 * foldR);
    h += (fold - 0.30) * 0.115;
    c *= 0.895 + 0.21 * fold;
    ao -= (1.0 - crest) * 0.14;
    float creaseLine = 1.0 - smoothstep(0.0, 0.10, foldT);
    rough -= creaseLine * 0.06;
    c *= 1.0 + creaseLine * 0.05;

    // wear: threadbare patches, fraying, pulled threads
    float wearField = smoothstep(0.58, 0.82, owFbm01(owWarp(p * 2.0, P * 2.0, 0.8, 3), P * 2.0, 4, 0.55));
    c = mix(c, c * 1.35 + 0.02, wearField * 0.5);
    rough += wearField * 0.06;
    h -= wearField * 0.05;

    float pulled = owScratches(p * 3.0, P * 3.0, 18.0, 1.0, 0.68);
    h += pulled * 0.05;
    c *= 1.0 - pulled * 0.10;

    float stain = smoothstep(0.55, 0.9, owFbm01(owWarp(p * 1.5 + 7.0, P * 1.5, 1.0, 3), P * 1.5, 5, 0.6));
    c = mix(c, c * 0.42 + owSRGB(vec3(0.09, 0.08, 0.06)), stain * 0.55);
    rough += stain * 0.05;

    float dust = smoothstep(0.4, 0.85, owFbm01(p * 6.0, P * 6.0, 4, 0.5));
    c = mix(c, owSRGB(vec3(0.400, 0.375, 0.335)), dust * 0.14);

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(rough, 0.5, 0.99);
    ao = clamp(ao, 0.25, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
