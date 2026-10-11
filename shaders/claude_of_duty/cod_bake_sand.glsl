// Sand bake.
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
    vec2 p = uv * P + uSeed * 8.2;

    // wind ripples: sheared sine, warped so the crests meander; gentle windward slope, sharp lee crest
    vec2 rp = owShear(p * 1.0, 1.0, 1.0);
    float warp = owFbm(p * 0.9, P * 0.9, 3, 0.55);
    float ripple = sin((rp.y * 1.0 + warp * 0.55) * 6.28318);
    ripple = ripple * 0.5 + 0.5;
    ripple = pow(ripple, 1.7) * 0.75 + ripple * 0.25;
    float rippleAmp = smoothstep(0.20, 0.70, owFbm01(p * 0.7, P * 0.7, 3, 0.6));
    float secondary = sin((p.y * 3.0 + p.x * 1.0 + warp * 0.8) * 6.28318) * 0.5 + 0.5;

    float dune = owFbm01(p * 0.5, P * 0.5, 4, 0.6);
    float mid = owFbm01(p * 5.0, P * 5.0, 5, 0.5);
    float grain = owFbm01(p * 18.0, P * 18.0, 4, 0.55);
    vec4 gcell = owWorley(p * 24.0, P * 24.0, 1.0);

    h = 0.50 + (dune - 0.5) * 0.16 + (mid - 0.5) * 0.05
        + (ripple - 0.5) * 0.26 * rippleAmp + (secondary - 0.5) * 0.06 * rippleAmp
        + (grain - 0.5) * 0.018;

    vec3 cLight = owSRGB(vec3(0.760, 0.660, 0.480));
    vec3 cMid = owSRGB(vec3(0.610, 0.510, 0.360));
    vec3 cDamp = owSRGB(vec3(0.360, 0.290, 0.205));
    vec3 c = mix(cMid, cLight, smoothstep(0.3, 0.8, dune));
    c = mix(c, cDamp, smoothstep(0.62, 0.28, h) * 0.55);
    // coarse grains collect on the crests, fines in the troughs
    c = mix(c, cLight * 1.06, smoothstep(0.45, 0.85, ripple) * rippleAmp * 0.35);
    c = mix(c, cMid * 0.88, smoothstep(0.45, 0.10, ripple) * rippleAmp * 0.30);
    c *= 0.90 + 0.18 * grain;
    c += smoothstep(0.22, 0.0, gcell.x) * step(0.86, gcell.z) * 0.10;

    rough = 0.90 + (grain - 0.5) * 0.10 - smoothstep(0.6, 0.3, h) * 0.12;
    metal = 0.0;
    ao = 1.0 - smoothstep(0.55, 0.25, h) * 0.10;

    // pebbles and shell fragments on top
    vec4 peb = owWorley(p * 18.0, P * 18.0, 1.0);
    float pebble = smoothstep(0.30, 0.10, peb.x) * step(0.80, peb.w);
    vec3 pcol = mix(owSRGB(vec3(0.400, 0.370, 0.330)), owSRGB(vec3(0.690, 0.660, 0.620)), peb.z);
    c = mix(c, pcol, pebble * 0.85);
    h += pebble * 0.05;
    rough = mix(rough, 0.55 + 0.25 * peb.z, pebble * 0.8);
    ao -= smoothstep(0.40, 0.30, peb.x) * step(0.80, peb.w) * 0.08;

    // dark mineral streaks
    float streak = smoothstep(0.62, 0.88, owFbm01(owShear(p * 2.5, 2.0, 4.0), owShearPer(P * 2.5, 4.0), 4, 0.5));
    c = mix(c, cDamp * 1.1, streak * 0.22);

    alb = clamp(c, vec3(0.02), vec3(0.82));
    rough = clamp(rough, 0.35, 0.99);
    ao = clamp(ao, 0.80, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
