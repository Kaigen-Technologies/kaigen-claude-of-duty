// Plaster bake: trowel sweeps, skim-coat laps, the 0.1-1 m soiling band, crazing, blown patches, chips, staining and mould.
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
    vec2 p = uv * P + uSeed * 5.3;

    // trowel: broad anisotropic sweeps with a fine skim on top
    vec2 sw = owShear(p * 1.5, 1.0, 3.0);
    float trowel = owFbm01(sw, owShearPer(P * 1.5, 3.0), 5, 0.55);
    float skim = owFbm01(p * 12.0, P * 12.0, 5, 0.5);
    float micro = owFbm01(p * 24.0, P * 24.0, 3, 0.5);
    float macro = owFbm01(p * 0.6, P * 0.6, 3, 0.6);

    vec3 cBase = owSRGB(vec3(0.598, 0.578, 0.538));
    vec3 cWarm = owSRGB(vec3(0.512, 0.462, 0.395));
    vec3 cGrey = owSRGB(vec3(0.382, 0.378, 0.372));
    vec3 c = mix(cBase, cWarm, smoothstep(0.3, 0.8, macro));
    c *= 0.94 + 0.12 * skim;
    c = mix(c, cGrey, smoothstep(0.45, 0.95, trowel) * 0.42);
    c = mix(c, cBase * 1.10, smoothstep(0.55, 0.15, trowel) * 0.30);

    h = 0.70 + (trowel - 0.5) * 0.10 + (skim - 0.5) * 0.030 + (micro - 0.5) * 0.012;
    rough = 0.80 + (skim - 0.5) * 0.12 - smoothstep(0.5, 0.9, trowel) * 0.10;
    ao = 1.0;
    metal = 0.0;

    // skim-coat laps: ~40 cm passes, each a hair lighter or darker with a faint arris
    vec2 lapUv = owShear(p * 0.7, 1.0, 1.0);
    float lapF = lapUv.y + owFbm01(p * 1.1, P * 1.1, 3, 0.6) * 1.4;
    float lapI = floor(lapF);
    float lapT = fract(lapF);
    float lapR = owHash11(lapI * 1.71 + uSeed * 2.3);
    c *= 0.885 + 0.240 * lapR;
    rough += (lapR - 0.5) * 0.10;

    // the 0.1-1 m band: damp bloom, hand-height soiling and a soft dirt wash, each re-centred and contrast-expanded
    float dampB = owFbm01(owWarp(p * 1.6 + 3.7, P * 1.6, 0.7, 3), P * 1.6, 4, 0.58);
    dampB = clamp((dampB - 0.5) * 2.6 + 0.5, 0.0, 1.0);
    c *= 0.80 + 0.42 * dampB;
    rough += (dampB - 0.5) * 0.12;
    float soil2 = owFbm01(owWarp(p * 3.4 + 21.0, P * 3.4, 0.55, 3), P * 3.4, 4, 0.55);
    soil2 = clamp((soil2 - 0.5) * 2.4 + 0.5, 0.0, 1.0);
    c *= 0.875 + 0.26 * soil2;
    float wash = owFbm01(p * 8.0 + 6.0, P * 8.0, 4, 0.5);
    wash = clamp((wash - 0.5) * 2.2 + 0.5, 0.0, 1.0);
    c *= 0.925 + 0.155 * wash;
    float lapEdge = (1.0 - smoothstep(0.0, 0.05, lapT)) * (0.35 + 0.65 * lapR);
    h += lapEdge * 0.022 - (lapR - 0.5) * 0.014;
    c *= 1.0 + lapEdge * 0.07;

    // 6-9 mm float grain with a matching height channel
    vec4 tooth = owWorley(p * 20.0, P * 20.0, 1.0);
    float grain = smoothstep(0.46, 0.06, tooth.x);
    float grainSel = 0.40 + 0.60 * step(0.32, tooth.z);
    h += grain * grainSel * 0.030;
    ao -= grain * 0.07;
    c *= 1.0 + (grain * grainSel - 0.20) * 0.16;
    rough += (tooth.z - 0.5) * 0.11 + grain * 0.05;
    float trough = smoothstep(0.52, 0.86, tooth.x);
    c = mix(c, c * 0.84, trough * 0.40);

    // pinholes from the float
    vec4 ph = owWorley(p * 22.0, P * 22.0, 1.0);
    float hole = smoothstep(0.24, 0.0, ph.x) * step(0.80, ph.w);
    h -= hole * 0.06;
    ao -= hole * 0.4;

    // hairline crazing, then structural cracks
    float hair = owCracks(p * 9.0, P * 9.0, 0.9, 0.016, 0.52);
    hair += owCracks(p * 16.0 + 6.0, P * 16.0, 0.95, 0.015, 0.62) * 0.5;
    hair = clamp(hair, 0.0, 1.0);
    h -= hair * 0.030;
    ao -= hair * 0.18;
    c = mix(c, c * 0.80, hair * 0.45);

    float crack = owCracks(p * 4.5 + 17.0, P * 4.5, 0.8, 0.018, 0.62);
    h -= crack * 0.16;
    ao -= crack * 0.6;
    c = mix(c, owSRGB(vec3(0.300, 0.278, 0.250)), crack * 0.8);

    // blown plaster revealing render and brick beneath
    float blowMask = owFbm01(owWarp(p * 1.05 + 9.0, P * 1.05, 1.1, 3), P * 1.05, 4, 0.55);
    float blow = smoothstep(0.775, 0.845, blowMask);
    float blowEdge = smoothstep(0.745, 0.790, blowMask) - blow;
    vec3 substrate = mix(owSRGB(vec3(0.360, 0.245, 0.195)), owSRGB(vec3(0.430, 0.400, 0.360)),
                         owFbm01(p * 9.0, P * 9.0, 4, 0.5));
    substrate *= 0.85 + 0.3 * owFbm01(p * 20.0, P * 20.0, 3, 0.5);
    c = mix(c, substrate, blow * 0.85);
    h -= blow * 0.13;
    ao -= blow * 0.26;
    rough += blow * 0.10;
    c += blowEdge * 0.06;
    h += blowEdge * 0.02;

    // 6-9 cm flakes knocked off the skim, fewer and larger than a fine speckle
    vec4 ck = owWorley(owWarp(p * 4.2 + 13.0, P * 4.2, 0.6, 3), P * 4.2, 0.95);
    float ckSel = step(0.930, ck.w);
    float ckSize = 0.22 + 0.20 * ck.z;
    float ckShape = smoothstep(ckSize, ckSize * 0.3,
                               ck.x * (0.70 + 0.60 * owFbm01(p * 16.0, P * 16.0, 3, 0.5)));
    float chip = ckSel * ckShape;
    vec3 coat = mix(c, owSRGB(vec3(0.392, 0.336, 0.284)), 0.52);
    coat *= 0.90 + 0.20 * owFbm01(p * 18.0, P * 18.0, 3, 0.5);
    c = mix(c, coat, chip * 0.58);
    h -= chip * 0.05;
    ao -= chip * 0.26;
    rough += chip * 0.09;
    float ckLip = max(ckSel * (smoothstep(ckSize * 1.25, ckSize, ck.x) - ckShape), 0.0);
    c *= 1.0 + ckLip * 0.10;
    h += ckLip * 0.010;

    // water staining: tide marks and slow brown bleed
    float stain = owFbm01(vec2(p.x * 1.6, p.y * 3.2), vec2(P.x * 1.6, P.y * 3.0), 5, 0.6);
    float tide = smoothstep(0.60, 0.78, stain) * (1.0 - smoothstep(0.78, 0.94, stain));
    c = mix(c, owSRGB(vec3(0.400, 0.330, 0.245)), tide * 0.45);
    c *= 1.0 - smoothstep(0.50, 0.95, stain) * 0.34;
    rough += tide * 0.05;

    float mould = smoothstep(0.72, 0.95, owFbm01(p * 4.0 + 25.0, P * 4.0, 5, 0.6)) *
                  smoothstep(0.45, 0.8, stain);
    c = mix(c, owSRGB(vec3(0.085, 0.090, 0.080)), mould * 0.7);
    rough += mould * 0.08;

    float cavity = 1.0 - smoothstep(0.48, 0.72, h);
    c = mix(c, owSRGB(vec3(0.22, 0.21, 0.19)), cavity * 0.30);

    alb = clamp(c, vec3(0.02), vec3(0.88));
    rough = clamp(rough, 0.35, 0.99);
    ao = clamp(ao, 0.15, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
