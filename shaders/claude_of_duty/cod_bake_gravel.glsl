// Gravel bake: compacted dust and grit with aggregate part-buried in it, three grades at 34/19/9 mm.
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
    vec2 p = uv * P + uSeed * 2.7;

    float bed = owFbm01(p * 1.3, P * 1.3, 4, 0.55);

    vec4 a = owWorley(p * 5.5, P * 5.5, 1.0);
    vec4 b = owWorley(p * 10.0 + 5.0, P * 10.0, 1.0);
    vec4 cSm = owWorley(p * 21.0 + 11.0, P * 21.0, 1.0);

    // sparse: most of what shows is the compacted bed with stones in it
    float sA = smoothstep(0.36, 0.10, a.x) * step(0.44, a.w);
    float sB = smoothstep(0.30, 0.08, b.x) * step(0.62, b.w);
    float sC = smoothstep(0.24, 0.06, cSm.x) * step(0.74, cSm.w);

    // stones live in the height field, half-buried
    float ha = sA * 0.15 * (0.5 + a.z);
    float hb = sB * 0.09 * (0.5 + b.z);
    float hc = sC * 0.025;
    h = 0.54 + (bed - 0.5) * 0.11 + max(max(ha, hb), hc) + 0.22 * (ha + hb);

    // the stone palette straddles the bed value
    vec3 s1 = owSRGB(vec3(0.372, 0.356, 0.332));
    vec3 s2 = owSRGB(vec3(0.232, 0.220, 0.208));
    vec3 s3 = owSRGB(vec3(0.462, 0.438, 0.400));
    vec3 s4 = owSRGB(vec3(0.352, 0.276, 0.220));
    vec3 top = mix(s1, s2, a.z);
    top = mix(top, s3, step(0.78, a.w));
    top = mix(top, s4, step(0.90, b.w) * 0.7);

    vec3 cBed = owSRGB(vec3(0.362, 0.336, 0.294));
    vec3 c = mix(cBed, top, clamp(sA * 0.70 + sB * 0.42 + sC * 0.16, 0.0, 1.0));
    float grain = owFbm01(p * 13.0, P * 13.0, 4, 0.5);
    c *= 0.965 + 0.07 * grain;

    // per-stone gloss kept small: a wide roughness spread at the aggregate period is salt-and-pepper
    rough = 0.82 + 0.05 * grain + (1.0 - clamp(sA + sB, 0.0, 1.0)) * 0.06
            - sA * (0.06 + 0.07 * a.z) - sB * 0.05 * b.z;
    metal = 0.0;
    // on a ground plane AO is nearly the only shading term: baked cavity stays low-frequency and low-contrast
    ao = mix(0.87, 1.0, smoothstep(0.42, 0.66, h));

    float dust = 1.0 - smoothstep(0.44, 0.62, h);
    c = mix(c, cBed * 1.04, dust * 0.5);
    rough += dust * 0.08;
    ao = mix(ao, 1.0, dust * 0.3);

    // traffic sweeps the grit into drifts and polishes bare lanes
    float drift = owFbm01(owWarp(p * 0.9 + 17.0, P * 0.9, 0.8, 3), P * 0.9, 4, 0.6);
    h += (drift - 0.5) * 0.10;
    c *= 0.86 + 0.28 * drift;
    rough += (drift - 0.5) * 0.10;
    c = mix(c, cBed * (0.92 + 0.22 * drift), smoothstep(0.55, 0.88, drift) * 0.72);

    // dried tyre tracks and dragged-heel scuffs
    float scuff = owFbm01(owShear(p * 2.2, 0.0, 6.0), owShearPer(P * 2.2, 6.0), 4, 0.5);
    c *= 1.0 - smoothstep(0.55, 0.92, scuff) * 0.10;
    rough -= smoothstep(0.6, 0.95, scuff) * 0.08;

    alb = clamp(c, vec3(0.02), vec3(0.78));
    rough = clamp(rough, 0.62, 0.99);
    ao = clamp(ao, 0.72, 1.0);
    h = clamp(h, 0.0, 1.0);
}

#include "cod_bake_tail.glsl"
@end
