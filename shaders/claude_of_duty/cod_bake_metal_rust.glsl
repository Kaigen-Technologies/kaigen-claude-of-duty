// Rusted steel bake: bare metal is metalness 1, every oxide layer on top is 0.
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
#include "cod_bake_rust.glsl"

void owSurface(vec2 uv, out vec3 alb, out float h, out float rough, out float metal, out float ao) {
    const vec2 P = vec2(8.0);
    vec2 p = uv * P + uSeed * 7.7;

    // base steel
    float mill = owFbm01(owShear(p * 4.0, 1.0, 6.0), owShearPer(P * 4.0, 6.0), 4, 0.5);
    float fine = owFbm01(p * 22.0, P * 22.0, 4, 0.5);
    vec3 steel = owSRGB(vec3(0.330, 0.335, 0.345)) * (0.90 + 0.18 * mill);
    vec3 c = steel;
    h = 0.72 + (mill - 0.5) * 0.02 + (fine - 0.5) * 0.01;
    rough = 0.40 + (mill - 0.5) * 0.16 + (fine - 0.5) * 0.08;
    metal = 1.0;
    ao = 1.0;

    // rust blooms: warped billow clusters, hard-edged where they flake
    vec2 wp = owWarp(p * 1.4, P * 1.4, 1.2, 4);
    float bloom = owBillow(wp, P * 1.4, 5, 0.6);
    bloom = 1.0 - bloom;
    float spread = owFbm01(p * 0.7 + 12.0, P * 0.7, 3, 0.6);
    float rust = smoothstep(0.36, 0.72, bloom * (0.55 + 0.85 * spread));
    float rustGrain = owFbm01(p * 26.0, P * 26.0, 4, 0.55);
    float pit = owFbm01(p * 24.0, P * 24.0, 3, 0.5);

    // flaking scale: the rust lifts in plates near the edge of a bloom
    float scl = owWorley(p * 16.0, P * 16.0, 1.0).x;
    float flake = smoothstep(0.30, 0.10, scl) * smoothstep(0.25, 0.55, rust) * (1.0 - smoothstep(0.8, 1.0, rust));

    // rust colour follows the age of the patch, not how much of it there is
    float rustAge = owFbm01(p * 0.85 + 21.0, P * 0.85, 4, 0.62);
    vec3 rustCol = owRustColour(rustAge * 0.8 + rust * 0.3, rustGrain);
    c = mix(c, rustCol, rust);
    metal = mix(1.0, 0.0, smoothstep(0.15, 0.55, rust));
    rough = mix(rough, 0.86 + 0.10 * rustGrain, smoothstep(0.1, 0.6, rust));
    h += rust * 0.11 * (0.4 + rustGrain) + flake * 0.13;
    h -= smoothstep(0.5, 0.95, rust) * pit * 0.14;
    ao -= flake * 0.30 + smoothstep(0.6, 1.0, rust) * 0.15;

    // pitting straight into the steel where rust has eaten through
    vec4 pits = owWorley(p * 22.0, P * 22.0, 1.0);
    float deep = smoothstep(0.22, 0.0, pits.x) * step(0.72, pits.w) * smoothstep(0.3, 0.8, rust);
    h -= deep * 0.22;
    ao -= deep * 0.45;
    c = mix(c, rustCol * 0.35, deep * 0.7);

    // scratches through everything, exposing bright metal
    float scr = owScratches(p * 3.0, P * 3.0, 12.0, 1.0, 0.60);
    scr += owScratches(p * 5.0 + 8.0, P * 5.0, 9.0, -2.0, 0.66) * 0.7;
    scr = clamp(scr, 0.0, 1.0) * 0.6;
    c = mix(c, owSRGB(vec3(0.480, 0.485, 0.495)), scr * 0.8);
    metal = mix(metal, 1.0, scr * 0.85);
    rough = mix(rough, 0.24, scr * 0.7);
    h -= scr * 0.010;

    float grime = smoothstep(0.55, 0.9, owFbm01(vec2(p.x * 5.0, p.y * 0.8), vec2(P.x * 5.0, max(P.y, 1.0)), 5, 0.55));
    c *= 1.0 - grime * 0.25;
    rough += grime * 0.08;

    alb = clamp(c, vec3(0.02), vec3(0.80));
    rough = clamp(rough, 0.12, 0.99);
    ao = clamp(ao, 0.15, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
