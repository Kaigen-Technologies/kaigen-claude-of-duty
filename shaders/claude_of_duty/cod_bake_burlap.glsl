// Hessian bake: coarse irregular weave, sun rot, loose fibres, sand caught in the weave.
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
    const float THREADS = 34.0;
    vec2 p = uv * P + uSeed * 4.7;

    vec2 t = uv * THREADS;
    vec2 cell = floor(t);
    vec2 f = fract(t) - 0.5;
    float over = mod(cell.x + cell.y, 2.0);

    // each hessian thread has its own thickness
    float twx = 0.62 + 0.30 * owHash12(vec2(cell.x, 0.0) + uSeed);
    float twy = 0.62 + 0.30 * owHash12(vec2(0.0, cell.y) + uSeed * 1.7);
    float warpP = cos(clamp(f.x / twx, -0.5, 0.5) * 3.14159);
    float weftP = cos(clamp(f.y / twy, -0.5, 0.5) * 3.14159);
    float top = mix(warpP, weftP, over);
    float bot = mix(weftP, warpP, over) * 0.40;
    float weave = max(top, bot);

    float fibre = owFbm01(owShear(p * 12.0, 0.0, 8.0), owShearPer(P * 12.0, 8.0), 3, 0.5);
    float macro = owFbm01(p * 1.0, P * 1.0, 4, 0.62);
    float dirt = owFbm01(owWarp(p * 2.5, P * 2.5, 0.8, 3), P * 2.5, 5, 0.55);

    vec3 cJute = owSRGB(vec3(0.520, 0.430, 0.275));
    vec3 cPale = owSRGB(vec3(0.640, 0.560, 0.400));
    vec3 cSoil = owSRGB(vec3(0.230, 0.180, 0.120));
    vec3 c = mix(cJute, cPale, owHash12(cell + 3.0) * 0.5 + fibre * 0.15);
    c *= 0.855 + 0.235 * (weave * 0.5 + 0.5);
    c *= 0.90 + 0.18 * macro;
    c = mix(c, cSoil, smoothstep(0.42, 0.85, dirt) * 0.60);

    h = 0.50 + weave * 0.38 + (fibre - 0.5) * 0.05;
    rough = 0.90 + (1.0 - weave) * 0.06;
    metal = 0.0;
    ao = mix(0.74, 1.0, smoothstep(-0.4, 0.9, weave));

    float rot = smoothstep(0.55, 0.9, owFbm01(p * 0.7 + 11.0, P * 0.7, 3, 0.6));
    c = mix(c, cPale * 1.15, rot * 0.4);
    rough += rot * 0.05;

    float loose = owScratches(p * 4.0, P * 4.0, 10.0, 2.0, 0.70);
    h += loose * 0.06;
    c = mix(c, cPale, loose * 0.3);

    float sandC = smoothstep(0.5, 0.85, owFbm01(p * 12.0, P * 12.0, 4, 0.5)) * (1.0 - smoothstep(0.2, 0.7, weave));
    c = mix(c, owSRGB(vec3(0.640, 0.545, 0.390)), sandC * 0.45);

    alb = clamp(c, vec3(0.02), vec3(0.80));
    rough = clamp(rough, 0.6, 0.99);
    ao = clamp(ao, 0.2, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
