// The composite: CA, shadow denoise, CAS sharpen, exposure, bloom, cos^4 vignette, AgX, sRGB, grade LUT, grain, dither.
#include "renderer.glsl"

@vs
#define HZ_RENDERER_VERTEX_SHADER
#include "renderer_shaders.glsl"
out vec2 v_uv;

void main() {
    v_uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(v_uv, 0.0);
}
@end

@fs
#undef HZ_RENDERER_VERTEX_SHADER
#define HZ_RENDERER_FRAGMENT_SHADER
#define HZ_RENDERER_FINAL
#include "renderer_shaders.glsl"
#include "hz_noise.glsl"
in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

@material
struct CodFinalMaterial {
    vec4 lens;  // x chromatic, y cos^4 vignette, z grain, w scene units per cd/m2
    vec4 grade; // x LUT strength, y sharpen, z LUT size
    vec4 look;  // x AgX slope, y AgX power, z AgX saturation
    @texture3D @white
    uint look_lut; // 33^3 display-referred grade
};

float cod_lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

vec3 cod_agx_contrast(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x + 0.4298 * x2 + 0.1191 * x - 0.00232;
}

// Rec.2020 inset, log2 over [-12.47, 4.03] EV, the look, the contrast polynomial, outset
vec3 cod_agx(vec3 color, float slope, float power, float sat) {
    const mat3 rec2020_from_srgb = mat3(vec3(0.6274, 0.0691, 0.0164), vec3(0.3293, 0.9195, 0.0880), vec3(0.0433, 0.0113, 0.8956));
    const mat3 srgb_from_rec2020 = mat3(vec3(1.6605, -0.1246, -0.0182), vec3(-0.5876, 1.1329, -0.1006), vec3(-0.0728, -0.0083, 1.1187));
    const mat3 inset = mat3(vec3(0.856627153315983, 0.137318972929847, 0.11189821299995),
                            vec3(0.0951212405381588, 0.761241990602591, 0.0767994186031903),
                            vec3(0.0482516061458583, 0.101439036467562, 0.811302368396859));
    const mat3 outset = mat3(vec3(1.1271005818144368, -0.1413297634984383, -0.14132976349843826),
                             vec3(-0.11060664309660323, 1.157823702216272, -0.11060664309660294),
                             vec3(-0.016493938717834573, -0.016493938717834257, 1.2519364065950405));
    const float min_ev = -12.47393;
    const float max_ev = 4.026069;
    color = inset * (rec2020_from_srgb * color);
    color = clamp((log2(max(color, vec3(1e-10))) - min_ev) / (max_ev - min_ev), 0.0, 1.0);
    color = pow(max(color * slope, vec3(0.0)), vec3(power));
    float l = cod_lum(color);
    color = l + sat * (color - l);
    color = outset * cod_agx_contrast(clamp(color, 0.0, 1.0));
    color = pow(max(color, vec3(0.0)), vec3(2.2));
    return clamp(srgb_from_rec2020 * color, 0.0, 1.0);
}

void main() {
    CodFinalMaterial mat = CodFinalMaterial_unpack(hz_final_in.material_offset);
    if (hz_final_in.debug_passthrough != 0) {
        frag_color = vec4(hz_final_color(v_uv), 1.0);
        return;
    }
    vec2 uv = v_uv;
    vec2 texel = 1.0 / max(hz_final_color_size(), vec2(1.0));
    // the denoise and sharpen thresholds are the framebuffer units
    float unit = mat.lens.w * (hz_exposure_input.enabled != 0u ? hz_exposure_input.inv_pre_exposure : 1.0);

    vec2 d = uv - 0.5;
    float r2 = dot(d, d);
    vec3 centre = max(hz_final_color(uv), vec3(0.0));
    vec3 hdr = centre;
    float ca = mat.lens.x * r2;
    if (ca > 0.00002) {
        vec2 o = d * ca;
        hdr = max(vec3(hz_final_color(uv + o).r, centre.g, hz_final_color(uv - o).b), vec3(0.0));
    }

    vec3 n1 = max(hz_final_color(uv + vec2(texel.x, 0.0)), vec3(0.0));
    vec3 n2 = max(hz_final_color(uv - vec2(texel.x, 0.0)), vec3(0.0));
    vec3 n3 = max(hz_final_color(uv + vec2(0.0, texel.y)), vec3(0.0));
    vec3 n4 = max(hz_final_color(uv - vec2(0.0, texel.y)), vec3(0.0));

    // deep shadows take the neighbourhood's chroma at their own luminance: the low-light denoise
    {
        vec3 nb = (n1 + n2 + n3 + n4) * 0.25;
        float lh = cod_lum(hdr);
        float ln = cod_lum(nb);
        float w = (1.0 - smoothstep(0.003, 0.030, lh * unit)) * 0.60;
        if (w > 0.005 && ln * unit > 1e-6)
            hdr = mix(hdr, nb * (lh / max(ln, 1e-30)), w);
    }

    // contrast-adaptive sharpen of the TAA-softened image
    if (mat.grade.y > 0.001) {
        float l1 = cod_lum(n1), l2 = cod_lum(n2), l3 = cod_lum(n3), l4 = cod_lum(n4);
        float lc = cod_lum(centre);
        float lmn = min(min(l1, l2), min(l3, l4));
        float lmx = max(max(l1, l2), max(l3, l4));
        float lblur = (l1 + l2 + l3 + l4) * 0.25;
        float contrast = (lmx - lmn) / (lmx + lmn + 0.02 / max(unit, 1e-30));
        float amount = mat.grade.y * (1.0 - clamp(contrast * 1.6, 0.0, 1.0));
        amount *= smoothstep(0.004, 0.03, lc * unit);
        float gain = (lc + (lc - lblur) * amount) / max(lc, 1e-30);
        hdr *= clamp(gain, 0.0, 4.0);
    }

    // the bloom is the same pre-exposed light as the frame: added before the exposure
    if (hz_bloom_input.intensity > 0.0)
        hdr += max(hz_final_bloom(uv), vec3(0.0)) * hz_bloom_input.intensity * hz_bloom_input.tint;
    hdr += hz_final_god_rays(uv);
    hdr += hz_final_lens_flare(uv);
    hdr *= hz_final_exposure_ratio();

    float cos4 = pow(1.0 / (1.0 + r2 * 2.4), 2.0);
    hdr *= mix(1.0, cos4, mat.lens.y);

    vec3 disp = linear_to_srgb(cod_agx(hdr, mat.look.x, mat.look.y, mat.look.z));
    float n = mat.grade.z;
    vec3 uvw = clamp(disp, 0.0, 1.0) * ((n - 1.0) / n) + (0.5 / n);
    disp = mix(disp, texture3d_sample_lod(look_lut, mat.look_lut, uvw, 0.0).rgb, mat.grade.x);

    // luminance-weighted two-octave grain, then a sub-code dither
    if (mat.lens.z > 0.0005) {
        float g = hz_hash12(gl_FragCoord.xy + hz_final_in.time * 137.13) - 0.5;
        float g2 = hz_hash12(gl_FragCoord.xy * 1.7 - hz_final_in.time * 71.3) - 0.5;
        float l = cod_lum(disp);
        disp += (g * 0.65 + g2 * 0.35) * mat.lens.z * (0.35 + 0.65 * smoothstep(0.0, 0.30, l));
    }
    disp += (hz_hash12(gl_FragCoord.xy * 0.5 + hz_final_in.time) - 0.5) * 0.0022;
    frag_color = vec4(srgb_to_linear(clamp(disp, 0.0, 1.0)), 1.0);
}
@end
