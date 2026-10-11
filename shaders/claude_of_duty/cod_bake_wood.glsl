// Weathered planks bake: five rows with staggered butt joints, ring grain, knots, splits, nails.
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
    const float PLANKS = 5.0;
    vec2 p = uv * P + uSeed * 12.9;

    // plank layout: rows along X, staggered butt joints, two boards per row lengthwise
    float rowF = uv.y * PLANKS;
    float row = floor(rowF);
    float rf = fract(rowF);
    float stagger = owHash11(row + uSeed * 2.0);
    float lenF = uv.x * 2.0 + stagger;
    float board = floor(lenF);
    float lf = fract(lenF);
    vec4 rnd = owHash42(vec2(board, row) + uSeed);

    const float GY = 0.035, GX = 0.010;
    float ey = min(smoothstep(0.0, GY, rf), smoothstep(0.0, GY, 1.0 - rf));
    float ex = min(smoothstep(0.0, GX, lf), smoothstep(0.0, GX, 1.0 - lf));
    float face = min(ex, ey);

    // grain: rings stretched along the board, warped, with knots
    vec2 gp = vec2(lf * 2.0 + rnd.x * 13.0, rf + rnd.y * 7.0);
    vec2 GP = vec2(16.0, 8.0);
    float warp = owFbm(vec2(gp.x * 3.0, gp.y * 12.0), vec2(GP.x * 3.0, GP.y * 12.0), 4, 0.55);
    float ringCoord = gp.y * (14.0 + rnd.z * 12.0) + warp * 2.2 + rnd.w * 5.0;

    // knots pull the rings into a tight radial swirl
    vec2 knotP = vec2(0.25 + rnd.x * 0.5, 0.35 + rnd.y * 0.3);
    float kd = length((vec2(lf, rf) - knotP) * vec2(2.2, 1.0));
    float hasKnot = step(0.68, rnd.z);
    float knotPull = hasKnot * exp(-kd * 9.0);
    ringCoord = mix(ringCoord, kd * 42.0, clamp(knotPull * 1.6, 0.0, 1.0));

    float rings = fract(ringCoord);
    float ringDark = smoothstep(0.42, 0.5, rings) * (1.0 - smoothstep(0.5, 0.62, rings));
    float latewood = smoothstep(0.30, 0.52, rings);

    float fibre = owFbm01(owShear(p * 6.0, 0.0, 40.0), owShearPer(P * 6.0, 40.0), 4, 0.5);
    float micro = owFbm01(p * 22.0, P * 22.0, 3, 0.5);

    vec3 wLight = owSRGB(vec3(0.505, 0.408, 0.290));
    vec3 wMid = owSRGB(vec3(0.362, 0.272, 0.180));
    vec3 wDark = owSRGB(vec3(0.205, 0.142, 0.092));
    vec3 wGrey = owSRGB(vec3(0.372, 0.355, 0.328)); // weathered silver-grey
    vec3 c = mix(wLight, wMid, rnd.w * 0.8 + latewood * 0.5);
    c = mix(c, wDark, ringDark * 0.65);
    c *= 0.90 + 0.18 * fibre;
    c = mix(c, wDark * 0.7, clamp(knotPull * 2.2, 0.0, 1.0) * 0.8);

    // UV-bleached and silvered, worst on the exposed boards
    float weather = smoothstep(0.20, 0.85, owFbm01(p * 0.8, P * 0.8, 3, 0.6)) * (0.4 + 0.6 * rnd.x);
    c = mix(c, wGrey, weather * 0.68);

    float faceH = 0.74 - ringDark * 0.02 - latewood * 0.012 + (fibre - 0.5) * 0.03 + (micro - 0.5) * 0.008;
    faceH += (rnd.y - 0.5) * 0.035; // boards cup and sit at different heights
    faceH -= clamp(knotPull * 1.5, 0.0, 1.0) * 0.03;

    // splits and checks running along the grain
    float split = owScratches(vec2(p.x, p.y) * 2.0, P * 2.0, 30.0, 0.0, 0.66) * weather;
    faceH -= split * 0.10;
    c = mix(c, wDark * 0.45, split * 0.7);

    // saw marks across the board
    float saw = owFbm01(owShear(p * 3.0, 0.0, 1.0) * vec2(30.0, 1.0), vec2(P.x * 90.0, P.y * 3.0), 3, 0.5);
    faceH += (saw - 0.5) * 0.012;

    // rounded and bashed board edges
    float edgeD = min(min(rf, 1.0 - rf) / GY, min(lf, 1.0 - lf) / GX);
    float bevel = 1.0 - smoothstep(0.0, 2.4, edgeD);
    faceH -= bevel * 0.035;
    c *= 1.0 - bevel * 0.10;
    c = mix(c, wLight * 1.15, bevel * smoothstep(0.5, 0.9, owFbm01(p * 20.0, P * 20.0, 3, 0.5)) * 0.35);

    // the gap between boards: dark and deep
    float m = smoothstep(0.05, 0.7, face);
    h = mix(0.44, faceH, m);
    c = mix(wDark * 0.25, c, m);
    rough = mix(0.95, 0.62 + 0.22 * fibre + weather * 0.20 + split * 0.15, m);
    ao = mix(0.25, 1.0, smoothstep(0.0, 0.5, face)) - bevel * 0.12 * m;
    metal = 0.0;

    // nails, with rust weeping below
    float nd = length(vec2(fract(lf * 3.0 + 0.5) - 0.5, rf - 0.22) * vec2(1.4, 1.0));
    float nail = smoothstep(0.055, 0.030, nd) * m * step(0.3, rnd.w);
    h -= nail * 0.02;
    c = mix(c, owSRGB(vec3(0.230, 0.200, 0.170)), nail * 0.85);
    rough = mix(rough, 0.55, nail);
    metal = mix(metal, 0.85, nail * 0.7);
    ao -= nail * 0.25;
    float weep = smoothstep(0.11, 0.05, nd) * step(0.3, rnd.w) * smoothstep(0.0, 0.6, rf - 0.22) * m;
    c = mix(c, owSRGB(vec3(0.330, 0.185, 0.095)), clamp(weep, 0.0, 1.0) * 0.4);

    float cavity = 1.0 - smoothstep(0.55, 0.78, h);
    c = mix(c, owSRGB(vec3(0.120, 0.106, 0.088)), cavity * 0.45);
    // ground-in dirt over the whole board
    float soil = smoothstep(0.40, 0.88, owFbm01(owWarp(p * 2.2 + 5.0, P * 2.2, 0.9, 3), P * 2.2, 5, 0.6));
    c = mix(c, owSRGB(vec3(0.185, 0.160, 0.128)), soil * 0.40);
    rough += soil * 0.08;

    alb = clamp(c, vec3(0.02), vec3(0.80));
    rough = clamp(rough, 0.25, 0.99);
    ao = clamp(ao, 0.12, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
