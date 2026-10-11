// Ceramic tile bake: 6x6 tiles, flat grout with a hard arris, broken and cracked tiles, traffic wear.
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
    const float N = 6.0;
    vec2 p = uv * P + uSeed * 4.4;

    vec2 tp = uv * N;
    vec2 id = floor(tp);
    vec2 f = fract(tp);
    vec4 rnd = owHash42(id + uSeed);

    const float J = 0.045;
    float dxj = min(f.x, 1.0 - f.x);
    float dyj = min(f.y, 1.0 - f.y);
    float ex = smoothstep(J * 0.70, J * 1.02, dxj);
    float ey = smoothstep(J * 0.70, J * 1.02, dyj);
    float face = min(ex, ey);

    float glaze = owFbm01(f * 6.0 + rnd.xy * 21.0, vec2(48.0), 4, 0.5);
    vec3 cTile = mix(owSRGB(vec3(0.700, 0.690, 0.660)), owSRGB(vec3(0.470, 0.500, 0.505)), rnd.z * 0.7);
    cTile *= 0.93 + 0.13 * glaze;
    cTile *= 0.92 + 0.16 * rnd.y;

    float grout = owFbm01(p * 20.0, P * 20.0, 4, 0.5);
    vec3 cGrout = owSRGB(vec3(0.400, 0.385, 0.360)) * (0.85 + 0.3 * grout);
    cGrout = mix(cGrout, owSRGB(vec3(0.13, 0.13, 0.12)), 0.45);

    float m = face;
    h = mix(0.76 - (grout - 0.5) * 0.02, 0.82 + (rnd.w - 0.5) * 0.04, m);
    vec3 c = mix(cGrout, cTile, m);
    // glazed tile stays glossy enough to catch a highlight
    rough = mix(0.92, 0.20 + 0.22 * glaze + (rnd.z - 0.5) * 0.14, m);
    ao = mix(0.40, 1.0, smoothstep(0.0, 0.8, face));
    metal = 0.0;

    // chipped, cracked and missing tiles
    float broken = step(0.90, rnd.x);
    float crack = owCracks(f * 3.0 + rnd.yz * 9.0, vec2(24.0), 0.85, 0.04, 0.45) * m;
    c = mix(c, c * 0.3, crack * 0.8);
    h -= crack * 0.08;
    ao -= crack * 0.5;
    vec3 sub = owSRGB(vec3(0.330, 0.300, 0.270));
    c = mix(c, sub, broken * m * 0.9);
    h -= broken * m * 0.14;
    rough = mix(rough, 0.95, broken * m);

    float wear = smoothstep(0.45, 0.95, owFbm01(p * 2.0, P * 2.0, 4, 0.55));
    rough += wear * 0.20 * m;
    c *= 1.0 - wear * 0.12;

    float cavity = 1.0 - smoothstep(0.68, 0.80, h);
    c = mix(c, owSRGB(vec3(0.14, 0.13, 0.12)), cavity * 0.35);

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(rough, 0.12, 0.95);
    ao = clamp(ao, 0.15, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
