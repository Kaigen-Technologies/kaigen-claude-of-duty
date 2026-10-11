// Brick bake: running bond, 6 bricks across and 18 courses up the tile.
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
    const float COLS = 6.0;
    const float ROWS = 18.0;
    vec2 p = uv * P + uSeed * 9.1;

    // brick lattice, running bond
    float rowF = uv.y * ROWS;
    float row = floor(rowF);
    float colF = uv.x * COLS + mod(row, 2.0) * 0.5;
    float col = floor(colF);
    vec2 id = vec2(mod(col, COLS), row);
    vec2 f = vec2(fract(colF), fract(rowF));

    vec4 rnd = owHash42(id + uSeed * 3.0);
    vec4 rnd2 = owHash42(id * 1.37 + 21.0 + uSeed);
    vec4 rnd3 = owHash42(id * 0.73 + 7.7 + uSeed * 1.9);

    // laid by hand: each brick a hair off square
    vec2 jitter = (rnd.xy - 0.5) * vec2(0.012, 0.030);
    vec2 fj = f + jitter;

    // raked joint: a flat mortar bed with a hard arris at the brick edge
    const float JX = 0.048, JY = 0.135;
    float dxj = min(fj.x, 1.0 - fj.x);
    float dyj = min(fj.y, 1.0 - fj.y);
    float shoulder = 0.74 + 0.16 * rnd3.w;
    float ex = smoothstep(JX * shoulder, JX * 1.02, dxj);
    float ey = smoothstep(JY * shoulder, JY * 1.02, dyj);
    float face = min(ex, ey);

    // per-brick surface coords so the face texture never repeats
    vec2 bp = vec2(fj.x, fj.y) * vec2(3.0, 1.0) + rnd.zw * 17.0;
    vec2 BP = vec2(24.0);

    // mortar
    float mSand = owFbm01(p * 20.0, P * 20.0, 4, 0.5);
    vec4 mGrain = owWorley(p * 24.0, P * 24.0, 1.0);
    float mortarRough = owFbm01(p * 20.0, P * 20.0, 4, 0.55);
    vec3 mortarCol = mix(owSRGB(vec3(0.400, 0.388, 0.362)), owSRGB(vec3(0.278, 0.272, 0.260)),
                         smoothstep(0.3, 0.8, mortarRough));
    mortarCol *= 0.84 + 0.32 * mSand;
    mortarCol *= 0.88 + 0.24 * owFbm01(p * 6.0, P * 6.0, 4, 0.6);
    mortarCol = mix(mortarCol, owSRGB(vec3(0.235, 0.228, 0.215)), smoothstep(0.5, 0.06, mGrain.x) * 0.40);
    mortarCol = mix(mortarCol, owSRGB(vec3(0.520, 0.505, 0.470)), smoothstep(0.30, 0.02, owWorley(p * 25.0 + 4.0, P * 25.0, 1.0).x) * 0.35);

    // some joints struck flush, some raked deep, some crumbled out
    float jointDepth = 0.10 + 0.05 * owFbm01(p * 1.2, P * 1.2, 3, 0.5);
    float crumble = smoothstep(0.62, 0.86, owFbm01(p * 9.0 + 4.0, P * 9.0, 4, 0.5));
    jointDepth += crumble * 0.09;
    float mortarH = -(mSand - 0.5) * 0.018 - smoothstep(0.5, 0.0, mGrain.x) * 0.012;

    // brick face
    float faceN = owFbm01(bp * 2.2, BP, 5, 0.5);
    float faceFine = owFbm01(bp * 5.0, BP * 2.0, 4, 0.5);
    vec4 facePore = owWorley(bp * 7.0, BP * 3.5, 1.0);
    float poreCluster = smoothstep(0.42, 0.78, owFbm01(bp * 3.0 + 8.0, BP * 1.5, 4, 0.55));
    float pore = smoothstep(0.26 + 0.16 * facePore.z, 0.0, facePore.x) * step(0.55, facePore.w) * poreCluster;

    vec3 cA = owSRGB(vec3(0.430, 0.238, 0.183));
    vec3 cB = owSRGB(vec3(0.318, 0.183, 0.150));
    vec3 cC = owSRGB(vec3(0.196, 0.132, 0.120));
    vec3 cD = owSRGB(vec3(0.492, 0.392, 0.300));
    vec3 cE = owSRGB(vec3(0.372, 0.288, 0.218));

    vec3 brick = mix(cA, cB, rnd.z);
    brick = mix(brick, cC, step(0.90, rnd.w) * 0.70);
    brick = mix(brick, cD, step(0.94, rnd2.x) * 0.62);
    brick = mix(brick, cE, step(0.55, rnd2.y) * 0.50);
    brick *= 0.88 + 0.24 * rnd3.x;
    brick *= 0.86 + 0.28 * faceN;
    // sand grain authored at 7 texels so it is still there at 0.5 m
    float faceGrain = owFbm01(bp * 8.0, BP * 4.0, 4, 0.55);
    brick *= 0.87 + 0.26 * faceGrain;
    brick = mix(brick, brick * 1.22, smoothstep(0.55, 0.9, faceFine) * 0.5);
    brick = mix(brick, brick * 0.62, pore * 0.85);
    brick = mix(brick, brick * 0.72, smoothstep(0.34, 0.0, facePore.x) * step(0.86, facePore.z));
    brick = mix(brick, owSRGB(vec3(0.62, 0.58, 0.50)), smoothstep(0.86, 0.98, faceFine) * 0.35);

    float faceH = 0.72 + (faceN - 0.5) * 0.05 + (faceFine - 0.5) * 0.025 + (rnd2.z - 0.5) * 0.05;
    faceH -= pore * 0.075;

    // broken arrises showing pale raw clay
    float edgeD = min(dxj / JX, dyj / JY);
    float chipNoise = owFbm01(bp * 6.0 + 3.0, BP * 3.0, 4, 0.5);
    float chip = smoothstep(1.7, 0.30, edgeD) * smoothstep(0.60, 0.80, chipNoise) * step(0.66, rnd3.z);
    faceH -= chip * 0.17;
    brick = mix(brick, brick * 0.72 + owSRGB(vec3(0.20, 0.13, 0.09)), chip * 0.65);

    // combine face and mortar; the face is already a shaped profile
    float m = face;
    h = mix(0.72 - jointDepth + mortarH, faceH, m);
    vec3 c = mix(mortarCol, brick, m);
    float brickRough = 0.58 + 0.32 * rnd2.z + (rnd3.y - 0.5) * 0.20;
    rough = mix(0.88 + 0.10 * mSand + 0.06 * (mortarRough - 0.5),
                brickRough + 0.14 * faceN + 0.10 * (faceGrain - 0.5) + chip * 0.14, m);
    ao = mix(0.34, 1.0, smoothstep(0.0, 0.75, face));
    ao -= chip * 0.30;
    metal = 0.0;

    float smear = smoothstep(0.5, 1.0, 1.0 - face) * smoothstep(0.55, 0.9, owFbm01(p * 14.0, P * 14.0, 4, 0.5));
    c = mix(c, mortarCol * 1.05, smear * 0.5);

    // weathering over the whole wall (the 0.1-1 m band)
    float soilB = owFbm01(owWarp(p * 1.8 + 27.0, P * 1.8, 0.6, 3), P * 1.8, 4, 0.58);
    soilB = clamp((soilB - 0.5) * 2.5 + 0.5, 0.0, 1.0);
    c *= 0.845 + 0.33 * soilB;

    float efflo = smoothstep(0.62, 0.96, owFbm01(owWarp(p * 2.6, P * 2.6, 0.8, 3), P * 2.6, 4, 0.5));
    efflo *= mix(1.0, 0.35, m);
    c = mix(c, owSRGB(vec3(0.66, 0.652, 0.632)), efflo * 0.5);
    rough += efflo * 0.10;

    float streak = owFbm01(vec2(p.x * 7.0, p.y * 2.3), vec2(P.x * 7.0, P.y * 2.0), 5, 0.55);
    float runoff = smoothstep(0.50, 0.92, streak);
    c *= 1.0 - runoff * 0.16;

    float crack = owCracks(p * 2.2, P * 2.2, 0.85, 0.038, 0.58);
    h -= crack * 0.10;
    ao -= crack * 0.45;
    c = mix(c, c * 0.35, crack * 0.7);

    float cavity = 1.0 - smoothstep(0.50, 0.74, h);
    c = mix(c, owSRGB(vec3(0.16, 0.15, 0.14)), cavity * 0.32);

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(rough, 0.35, 0.99);
    ao = clamp(ao, 0.12, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
