// Concrete and concrete floor bake: param.x board-formed wall, param.y saw-cut slab.
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
    vec2 p = uv * P + uSeed * 13.7;

    float macro = owFbm01(p * 0.5, P * 0.5, 4, 0.58);
    float mid = owFbm01(owWarp(p * 2.0, P * 2.0, 0.7, 3), P * 2.0, 5, 0.5);
    float fine = owFbm01(p * 18.0, P * 18.0, 4, 0.5);
    float micro = owFbm01(p * 26.0, P * 26.0, 3, 0.5);

    vec3 cLight = owSRGB(vec3(0.520, 0.512, 0.492));
    vec3 cMid = owSRGB(vec3(0.395, 0.392, 0.385));
    vec3 cDark = owSRGB(vec3(0.255, 0.253, 0.258));
    vec3 c = mix(cMid, cLight, smoothstep(0.35, 0.85, macro));
    c = mix(c, cDark, smoothstep(0.55, 0.95, mid) * 0.55);
    c *= 0.93 + 0.14 * fine;
    // the 0.1-1 m band: pour blotching and the wash of dirt, contrast-expanded because fbm never spans 0..1
    float pourB = owFbm01(owWarp(p * 1.5 + 8.3, P * 1.5, 0.6, 3), P * 1.5, 4, 0.58);
    pourB = clamp((pourB - 0.5) * 2.5 + 0.5, 0.0, 1.0);
    c *= 0.82 + 0.38 * pourB;
    float wash = owFbm01(p * 7.0 + 2.0, P * 7.0, 4, 0.5);
    wash = clamp((wash - 0.5) * 2.2 + 0.5, 0.0, 1.0);
    c *= 0.925 + 0.155 * wash;

    h = 0.62 + (fine - 0.5) * 0.035 + (mid - 0.5) * 0.05;
    rough = 0.70 + (mid - 0.5) * 0.16 + (micro - 0.5) * 0.07;
    ao = 1.0;
    metal = 0.0;

    // exposed aggregate: stone chips just under the skin
    vec4 agg = owWorley(p * 13.0, P * 13.0, 0.95);
    float aggShape = smoothstep(0.46, 0.10, agg.x);
    float aggRnd = agg.z;
    float aggExposed = aggShape * step(0.74, owFbm01(p * 3.0 + 5.0, P * 3.0, 3, 0.5) + aggRnd * 0.35);
    h += aggExposed * 0.022 * (0.5 + aggRnd);
    c = mix(c, mix(owSRGB(vec3(0.335, 0.320, 0.300)), owSRGB(vec3(0.560, 0.545, 0.505)), aggRnd), aggExposed * 0.7);
    rough += aggExposed * 0.07 * (aggRnd - 0.5);

    // the resolvable 5-8 mm grit; the finer tooth belongs to the shared detail map
    vec4 sand = owWorley(p * 20.0, P * 20.0, 1.0);
    float sandM = smoothstep(0.44, 0.05, sand.x);
    float sandSel = 0.40 + 0.60 * step(0.30, sand.z);
    h += sandM * sandSel * 0.028;
    c *= 1.0 + (sandM * sandSel - 0.20) * 0.15;
    rough += (sand.z - 0.5) * 0.11 + sandM * 0.04;
    ao -= sandM * 0.06;
    float sandTrough = smoothstep(0.52, 0.88, sand.x);
    c = mix(c, c * 0.86, sandTrough * 0.34);

    // air pockets from the pour
    vec4 pores = owWorley(p * 22.0, P * 22.0, 1.0);
    float pore = smoothstep(0.26, 0.0, pores.x) * step(0.84, pores.w);
    h -= pore * 0.055;
    ao -= pore * 0.55;
    rough += pore * 0.10;

    float formAmt = uParam.x;
    float jointAmt = uParam.y;

    // formwork: horizontal board lines and tie-rod holes
    float boards = uv.y * 4.0;
    float bi = floor(boards);
    float bf = fract(boards);
    float seam = (1.0 - smoothstep(0.0, 0.030, bf)) + (1.0 - smoothstep(0.0, 0.030, 1.0 - bf));
    seam = clamp(seam, 0.0, 1.0);
    float boardStep = (owHash11(bi + uSeed) - 0.5) * 0.028 * formAmt;
    h += boardStep;
    h -= seam * 0.055 * formAmt;
    ao -= seam * 0.40 * formAmt;
    c *= 1.0 - seam * 0.16 * formAmt;
    float bleed = (1.0 - smoothstep(0.0, 0.10, abs(bf - 0.02))) * 0.5 * formAmt;
    c = mix(c, cLight * 1.05, bleed * 0.35 * owFbm01(p * 8.0, P * 8.0, 3, 0.5));

    vec2 tf = fract(vec2(uv.x * 3.0, boards * 0.5)) - 0.5;
    float tieRnd = owHash12(floor(vec2(uv.x * 3.0, boards * 0.5)) + uSeed);
    float tie = smoothstep(0.085, 0.05, length(tf * vec2(1.0, 2.0))) * step(0.45, tieRnd) * formAmt;
    h -= tie * 0.10;
    ao -= tie * 0.5;
    c = mix(c, cDark * 0.85, tie * 0.6);

    // saw-cut control joints and power-float polish
    vec2 jd = abs(fract(uv + 0.5) - 0.5);
    float joint = max(1.0 - smoothstep(0.0035, 0.010, jd.x), 1.0 - smoothstep(0.0035, 0.010, jd.y));
    joint *= jointAmt;
    h -= joint * 0.10;
    ao -= joint * 0.55;
    c = mix(c, cDark * 0.62, joint * 0.65);
    float swirl = owFbm01(owWarp(p * 1.1 + 3.0, P * 1.1, 1.4, 3), P * 1.1, 3, 0.6);
    rough -= jointAmt * smoothstep(0.35, 0.85, swirl) * 0.10;
    c *= 1.0 - jointAmt * smoothstep(0.4, 0.9, swirl) * 0.07;

    // structural cracks
    float crk = owCracks(p * 2.6, P * 2.6, 0.85, 0.028, 0.50);
    float crkFine = owCracks(p * 7.0 + 31.0, P * 7.0, 0.9, 0.020, 0.60) * 0.55;
    float crack = clamp(crk + crkFine, 0.0, 1.0);
    h -= crack * 0.12;
    ao -= crack * 0.45;
    c = mix(c, cDark * 0.80, crack * 0.42);
    rough += crack * 0.12;

    // spalling: a chunk of the skin broken off, aggregate showing
    vec4 sp = owWorley(p * 1.1 + 7.3, P * 1.1, 0.9);
    float spallCell = step(0.90, sp.w);
    float spall = spallCell * smoothstep(0.44, 0.16, sp.x) *
                  smoothstep(0.42, 0.62, owFbm01(p * 4.0 + 2.0, P * 4.0, 4, 0.5));
    h -= spall * 0.13;
    ao -= spall * 0.35;
    c = mix(c, mix(cDark, cMid, aggRnd) * 0.88, spall * 0.8);
    rough += spall * 0.10;
    float spallRim = spall * (1.0 - spall) * 4.0;
    c *= 1.0 + spallRim * 0.10;

    // 2-5 cm chips bitten out of the skin
    vec4 ck = owWorley(owWarp(p * 5.6 + 19.0, P * 5.6, 0.6, 3), P * 5.6, 0.95);
    float ckSel = step(0.90, ck.w);
    float ckSize = 0.20 + 0.16 * ck.z;
    float ckShape = smoothstep(ckSize, ckSize * 0.3,
                               ck.x * (0.72 + 0.56 * owFbm01(p * 16.0, P * 16.0, 3, 0.5)));
    float chip = ckSel * ckShape;
    c = mix(c, mix(c * 0.74, mix(cDark, cMid, sand.z), 0.5), chip * 0.85);
    h -= chip * 0.045;
    ao -= chip * 0.24;
    rough += chip * 0.08;
    float ckLip = max(ckSel * (smoothstep(ckSize * 1.25, ckSize, ck.x) - ckShape), 0.0);
    c *= 1.0 + ckLip * 0.10;

    // staining: shallow 3:1 runoff and rust bleed; the long runs come from the runtime weather layer
    float streak = owFbm01(vec2(p.x * 6.0, p.y * 2.0), vec2(P.x * 6.0, P.y * 2.0), 5, 0.55);
    float runoff = smoothstep(0.58, 0.95, streak) * (0.35 + 0.65 * smoothstep(0.2, 0.8, macro));
    c *= 1.0 - runoff * 0.14;
    rough += runoff * 0.05;

    float rustBleed = smoothstep(0.72, 0.98, streak * (0.6 + 0.5 * tieRnd)) * step(0.80, tieRnd);
    c = mix(c, owSRGB(vec3(0.42, 0.24, 0.12)), rustBleed * 0.45);

    float cavity = 1.0 - smoothstep(0.42, 0.66, h);
    c = mix(c, owSRGB(vec3(0.20, 0.19, 0.17)), cavity * 0.35);

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(rough, 0.48, 0.98);
    ao = clamp(ao, 0.15, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
