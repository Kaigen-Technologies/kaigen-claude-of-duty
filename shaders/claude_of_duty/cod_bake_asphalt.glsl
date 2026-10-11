// Asphalt bake. Every band stays at K <= 28 so a 1024 bake never goes sub-texel; the finer tooth is the shared detail map.
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
    vec2 p = uv * P + uSeed * 6.9;

    // binder: dark, slightly blue-grey, sun-bleached in patches
    float macro = owFbm01(p * 0.55, P * 0.5, 4, 0.6);
    float mid = owFbm01(p * 3.0, P * 3.0, 5, 0.5);
    float fine = owFbm01(p * 16.0, P * 16.0, 4, 0.5);

    vec3 cFresh = owSRGB(vec3(0.115, 0.115, 0.122));
    vec3 cWorn = owSRGB(vec3(0.300, 0.298, 0.295));
    vec3 c = mix(cFresh, cWorn, smoothstep(0.25, 0.85, macro) * 0.85);
    c *= 0.94 + 0.12 * fine;

    h = 0.60 + (mid - 0.5) * 0.06;
    rough = 0.78 + (mid - 0.5) * 0.10 + (fine - 0.5) * 0.14;
    metal = 0.0;
    ao = 1.0;

    // dense angular chippings in three grades; warping the Worley domain faceting the cells
    vec2 ap = owWarp(p, P, 0.10, 3);
    vec4 big = owWorley(ap * 12.0, P * 12.0, 1.0);
    float bigM = smoothstep(0.40, 0.16, big.x);
    float bigExposed = bigM * smoothstep(0.30, 0.62, owFbm01(p * 2.2 + 3.0, P * 2.0, 4, 0.5) + big.w * 0.5);
    vec4 smallG = owWorley(ap * 22.0 + 7.0, P * 22.0, 1.0);
    float smallM = smoothstep(0.36, 0.10, smallG.x);
    float smallExposed = smallM * step(0.30, smallG.w);
    vec4 grit = owWorley(ap * 28.0 + 3.0, P * 28.0, 1.0);
    float gritM = smoothstep(0.32, 0.06, grit.x) * step(0.45, grit.z);

    vec3 stoneA = owSRGB(vec3(0.400, 0.392, 0.378));
    vec3 stoneB = owSRGB(vec3(0.210, 0.200, 0.192));
    vec3 stoneC = owSRGB(vec3(0.560, 0.520, 0.470));
    vec3 stone = mix(stoneA, stoneB, big.z);
    stone = mix(stone, stoneC, step(0.90, big.w));

    // stones read by relief and specular, not tint
    c = mix(c, stone, bigExposed * 0.52);
    c = mix(c, mix(stoneA, stoneC, smallG.z), smallExposed * 0.22);
    c = mix(c, mix(stoneB, stoneA, grit.z), gritM * 0.14);
    h += bigExposed * 0.15 * (0.6 + 0.6 * big.z) + smallExposed * 0.065 + gritM * 0.022;
    rough += bigExposed * (0.10 - 0.22 * big.z) + smallExposed * (0.06 - 0.14 * smallG.z);

    // voids where the binder has ravelled out
    float voidM = smoothstep(0.50, 0.85, big.x) * smoothstep(0.28, 0.6, smallG.x);
    h -= voidM * 0.10;
    ao -= voidM * 0.14;

    // tyre polish: two smooth bands where wheels track
    float lane = abs(fract(uv.x * 1.0 + 0.25) - 0.5) * 2.0;
    float polish = (1.0 - smoothstep(0.10, 0.62, lane)) *
                   smoothstep(0.25, 0.65, owFbm01(vec2(p.x * 0.7, p.y * 5.0), vec2(P.x, P.y * 5.0), 4, 0.5));
    rough -= polish * 0.16;
    h -= polish * 0.012;
    c = mix(c, c * 0.78 + owSRGB(vec3(0.045, 0.045, 0.048)), polish * 0.45);

    // patch repairs with a seam
    vec4 rep = owWorley(owWarp(p * 0.5 + 13.0, P * 0.5, 1.6, 3), P * 0.5, 0.9);
    float inPatch = step(0.72, rep.w);
    float patchEdge = (1.0 - smoothstep(0.0, 0.06, rep.y - rep.x)) * inPatch;
    c = mix(c, cFresh * (0.85 + 0.35 * fine), inPatch * 0.20);
    rough = mix(rough, 0.84, inPatch * 0.22);
    h -= patchEdge * 0.07;
    ao -= patchEdge * 0.20;
    c = mix(c, cFresh * 0.5, patchEdge * 0.35);
    float tar = patchEdge * smoothstep(0.4, 0.7, owFbm01(p * 6.0, P * 6.0, 3, 0.5));
    rough -= tar * 0.35;
    c = mix(c, owSRGB(vec3(0.055, 0.055, 0.058)), tar * 0.7);

    // alligator cracking and long thermal cracks
    float gator = owCracks(p * 3.4, P * 3.4, 0.9, 0.032, 0.56);
    float thermal = owCracks(p * 0.9 + 41.0, P * 0.9, 0.75, 0.05, 0.70);
    float crack = clamp(gator + thermal, 0.0, 1.0);
    h -= crack * 0.16;
    ao -= crack * 0.30;
    c = mix(c, owSRGB(vec3(0.045, 0.043, 0.042)), crack * 0.85);
    rough += crack * 0.12;

    // oil stains, dark and slightly glossy
    float oil = smoothstep(0.68, 0.90, owFbm01(owWarp(p * 1.8 + 31.0, P * 1.8, 0.9, 3), P * 1.8, 4, 0.55));
    c = mix(c, owSRGB(vec3(0.045, 0.043, 0.046)), oil * 0.6);
    rough -= oil * 0.16;

    // dust settled in the low spots
    float dust = smoothstep(0.55, 0.30, h) * smoothstep(0.35, 0.75, macro);
    c = mix(c, owSRGB(vec3(0.420, 0.390, 0.340)), dust * 0.35);
    rough += dust * 0.10;

    alb = clamp(c, vec3(0.02), vec3(0.75));
    rough = clamp(rough, 0.44, 0.99);
    // on a ground plane this channel is the shading
    ao = clamp(ao, 0.68, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
