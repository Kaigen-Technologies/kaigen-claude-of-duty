// Painted steel bake: paint over primer over rust over steel; tint_a is the paint, param.z biases the chipping.
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
    vec2 p = uv * P + uSeed * 11.3;

    // substrate: steel with a mill finish
    float mill = owFbm01(owShear(p * 5.0, 1.0, 8.0), owShearPer(P * 5.0, 8.0), 4, 0.5);
    vec3 steel = owSRGB(vec3(0.330, 0.335, 0.345)) * (0.88 + 0.2 * mill);

    // rust that has crept under the paint
    float bloom = 1.0 - owBillow(owWarp(p * 1.8, P * 1.8, 1.1, 4), P * 1.8, 5, 0.6);
    float rustField = smoothstep(0.60, 0.92, bloom);
    float rustGrain = owFbm01(p * 22.0, P * 22.0, 4, 0.55);
    vec3 rustCol = owRustColour(rustField, rustGrain);

    // an industrial coat with roller texture and orange peel, sun-bleached in patches
    float peel = owFbm01(p * 22.0, P * 22.0, 4, 0.5);
    float roller = owFbm01(owShear(p * 2.0, 0.0, 3.0), owShearPer(P * 2.0, 3.0), 4, 0.5);
    vec3 paint = uTintA * (0.90 + 0.16 * roller);
    paint *= 0.96 + 0.08 * peel;
    float bleach = smoothstep(0.35, 0.85, owFbm01(p * 0.8, P * 0.8, 3, 0.6));
    paint = mix(paint, paint * 1.25 + 0.03, bleach * 0.5);

    // chipping: paint mostly holds and fails hardest where rust lifts it from underneath
    float chipField = owFbm01(owWarp(p * 2.6 + 4.0, P * 2.6, 0.9, 3), P * 2.6, 5, 0.55);
    float chipEdge = owFbm01(p * 12.0, P * 12.0, 4, 0.5);
    float chipSrc = chipField * 0.60 + chipEdge * 0.20 + rustField * 0.32 + uParam.z * 0.25;
    float chip = smoothstep(0.66, 0.92, chipSrc);
    vec4 dings = owWorley(p * 20.0, P * 20.0, 1.0);
    float ding = smoothstep(0.14, 0.03, dings.x) * step(0.88, dings.w);
    chip = clamp(chip + ding, 0.0, 1.0);

    // scratches that cut down to bare metal
    float scr = owScratches(p * 2.5, P * 2.5, 14.0, 1.0, 0.62);
    scr += owScratches(p * 4.0 + 21.0, P * 4.0, 10.0, -1.0, 0.66) * 0.8;
    scr = clamp(scr, 0.0, 1.0);

    vec3 primer = owSRGB(vec3(0.470, 0.300, 0.180));
    float primerBand = smoothstep(0.0, 0.35, chip) * (1.0 - smoothstep(0.35, 0.6, chip));

    vec3 c = paint;
    float r = 0.42 + (peel - 0.5) * 0.22 + bleach * 0.16;
    float mtl = 0.0;
    h = 0.74 + (roller - 0.5) * 0.02 + (peel - 0.5) * 0.012;
    ao = 1.0;

    c = mix(c, primer, primerBand * 0.7);
    c = mix(c, rustCol, smoothstep(0.35, 0.75, chip) * (0.55 + 0.45 * rustField));
    c = mix(c, steel, smoothstep(0.75, 0.95, chip) * (1.0 - rustField) * 0.9);
    r = mix(r, 0.88, smoothstep(0.3, 0.8, chip) * (0.4 + 0.6 * rustField));
    r = mix(r, 0.38, smoothstep(0.8, 1.0, chip) * (1.0 - rustField));
    mtl = mix(0.0, 1.0, smoothstep(0.78, 0.96, chip) * (1.0 - smoothstep(0.2, 0.7, rustField)));
    h -= smoothstep(0.4, 0.8, chip) * 0.16; // paint has real thickness
    ao -= smoothstep(0.35, 0.7, chip) * 0.22;
    float lip = smoothstep(0.30, 0.42, chip) * (1.0 - smoothstep(0.42, 0.55, chip));
    c *= 1.0 + lip * 0.15;
    h += lip * 0.05;

    c = mix(c, owSRGB(vec3(0.500, 0.505, 0.515)), scr * 0.55);
    mtl = mix(mtl, 1.0, scr * 0.6);
    r = mix(r, 0.26, scr * 0.55);

    // dirt and rain streaks, with rust bleed running down from the chips
    float streak = owFbm01(vec2(p.x * 6.0, p.y * 0.7), vec2(P.x * 6.0, max(P.y, 1.0)), 5, 0.55);
    float grime = smoothstep(0.52, 0.92, streak);
    c *= 1.0 - grime * 0.30;
    r += grime * 0.10;
    mtl *= 1.0 - grime * 0.5;
    float bleed = smoothstep(0.66, 0.95, streak) * smoothstep(0.2, 0.6, rustField);
    c = mix(c, owSRGB(vec3(0.360, 0.190, 0.090)), bleed * 0.45);

    float cavity = 1.0 - smoothstep(0.62, 0.78, h);
    c *= 1.0 - cavity * 0.18;

    alb = clamp(c, vec3(0.02), vec3(0.85));
    rough = clamp(r, 0.14, 0.99);
    metal = clamp(mtl, 0.0, 1.0);
    ao = clamp(ao, 0.2, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
