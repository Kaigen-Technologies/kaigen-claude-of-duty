// Corrugated galvanised sheet bake: 12 ridges, lapped panels, spangle, rust, fixings.
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
    const float RIDGES = 12.0;
    vec2 p = uv * P + uSeed * 6.1;

    // sinusoidal ridges with a flat-ish crown; panel joints every 4 ridges, one sheet lapping the next
    float t = uv.x * RIDGES * 6.28318530718;
    float wave = sin(t);
    float profile = sign(wave) * pow(abs(wave), 0.72) * 0.5 + 0.5;
    float panel = uv.x * RIDGES / 4.0;
    float panelId = floor(panel);
    float lap = smoothstep(0.0, 0.06, fract(panel)) * smoothstep(0.0, 0.06, 1.0 - fract(panel));
    float panelStep = (owHash11(panelId + uSeed) - 0.5) * 0.05;

    float dents = owFbm01(p * 2.2, P * 2.2, 4, 0.6);
    float fine = owFbm01(p * 11.0, P * 11.0, 4, 0.5);

    h = 0.18 + profile * 0.62 + panelStep + (dents - 0.5) * 0.07 + (fine - 0.5) * 0.012;
    h -= (1.0 - lap) * 0.06;

    // galvanised zinc: crystalline spangle
    vec4 sp = owWorley(p * 7.0, P * 7.0, 1.0);
    float spangle = smoothstep(0.55, 0.05, sp.x);
    vec3 zinc = owSRGB(vec3(0.520, 0.535, 0.545));
    vec3 c = mix(zinc * 0.86, zinc * 1.12, spangle * (0.3 + 0.7 * sp.z));
    c *= 0.94 + 0.12 * fine;
    metal = 1.0;
    rough = 0.34 + (1.0 - spangle) * 0.16 + (fine - 0.5) * 0.08;
    ao = 1.0;

    // rust, heavier in the valleys and at the bottom of the sheet
    float valley = 1.0 - profile;
    float rustField = smoothstep(0.62, 0.98,
        (1.0 - owBillow(owWarp(p * 1.6, P * 1.6, 1.0, 4), P * 1.6, 5, 0.6)) *
        (0.58 + 0.40 * valley) + (1.0 - uv.y) * 0.16);
    float rustGrain = owFbm01(p * 22.0, P * 22.0, 4, 0.55);
    vec3 rustCol = owRustColour(rustField, rustGrain);
    c = mix(c, rustCol, rustField);
    metal = mix(metal, 0.0, smoothstep(0.15, 0.6, rustField));
    rough = mix(rough, 0.88 + 0.08 * rustGrain, smoothstep(0.1, 0.6, rustField));
    h += rustField * 0.02 * rustGrain;

    // holes rusted right through
    vec4 hole = owWorley(p * 5.0 + 31.0, P * 5.0, 0.95);
    float perf = smoothstep(0.10, 0.02, hole.x) * step(0.94, hole.w) * smoothstep(0.5, 0.9, rustField);
    h -= perf * 0.5;
    ao -= perf * 0.7;
    c = mix(c, rustCol * 0.25, perf);

    // hex screws with a rubber washer, two rows, on the crowns
    float crown = smoothstep(0.72, 0.95, profile);
    vec2 fx = vec2(fract(uv.x * RIDGES) - 0.5, fract(uv.y * 3.0) - 0.5);
    float fd = length(fx * vec2(1.0, RIDGES / 3.0));
    float screwRnd = owHash12(floor(vec2(uv.x * RIDGES, uv.y * 3.0)) + uSeed);
    float screw = smoothstep(0.16, 0.11, fd) * crown * step(0.25, screwRnd);
    float washer = smoothstep(0.24, 0.18, fd) * crown * step(0.25, screwRnd);
    h += washer * 0.02 + screw * 0.035;
    c = mix(c, owSRGB(vec3(0.120, 0.115, 0.110)), washer * 0.8);
    c = mix(c, mix(owSRGB(vec3(0.400, 0.405, 0.410)), rustCol, rustField), screw);
    rough = mix(rough, 0.85, washer * 0.8);
    rough = mix(rough, 0.42 + rustField * 0.4, screw);
    metal = mix(metal, 0.0, washer * 0.9);
    metal = mix(metal, 1.0 - rustField, screw);
    ao -= (washer - screw) * 0.35;
    float weep = smoothstep(0.34, 0.20, fd) * step(0.25, screwRnd) * crown *
                 smoothstep(0.0, 0.5, fract(uv.y * 3.0) - 0.5);
    c = mix(c, owSRGB(vec3(0.330, 0.170, 0.080)), clamp(weep, 0.0, 1.0) * 0.5);

    // dirt collecting in the valleys
    float dirt = valley * smoothstep(0.35, 0.8, owFbm01(p * 3.0, P * 3.0, 4, 0.55));
    c = mix(c, owSRGB(vec3(0.200, 0.185, 0.160)), dirt * 0.40);
    rough += dirt * 0.14;
    metal *= 1.0 - dirt * 0.5;
    ao -= valley * 0.18;

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(rough, 0.14, 0.99);
    metal = clamp(metal, 0.0, 1.0);
    ao = clamp(ao, 0.15, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
