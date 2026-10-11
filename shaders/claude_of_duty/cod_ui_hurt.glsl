// The HUD's hurt layers: the warped blood vignette with its multiplied corner blobs (mode 0), the
// screen-blended hit flash (mode 1) and the low-health beat vignette (mode 2). One shader, two pipelines (alpha, screen).
#include "common.glsl"
#include "ui/common_ui.glsl"

@vs
layout(set = 0, binding = 0) uniform _VSUniforms {
    vec2 resolution;
    vec2 _pad0;
    vec4 rect; // x, y, w, h in framebuffer px
} vs_uniforms;

layout(location = 0) out vec2 v_uv;

void main() {
    vec2 uv = ui_quad_uv(uint(gl_VertexIndex));
    v_uv = uv;
    gl_Position = ui_rect_to_clip(vs_uniforms.rect, uv, vs_uniforms.resolution);
}
@end

@fs
#include "cod_ui_common.glsl"

layout(set = 0, binding = 1) uniform _HurtParams {
    vec4 box;  // x box width, y box height (UI px), z mode, w opacity
    vec4 warp; // x displacement amplitude (px), y, z noise frequency per px, w seed
} hurt;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

// a css gradient colour stop pair interpolated premultiplied: returns straight rgb, alpha
vec4 premul_mix(vec4 c0, vec4 c1, float u) {
    vec3 p = mix(c0.rgb * c0.a, c1.rgb * c1.a, u);
    float a = mix(c0.a, c1.a, u);
    return vec4(a > 1e-5 ? p / a : c1.rgb, a);
}

// the radial gradient of ow-blood-a: ellipse 78% 74% at the centre, transparent to 62 %, rgba(122,14,10,.30) at 86 %, rgba(74,8,5,.60) at 100 %
vec4 blood_a(vec2 p, vec2 size) {
    vec2 r = (p - 0.5 * size) / (vec2(0.78, 0.74) * size);
    float t = length(r);
    vec4 c0 = vec4(122.0, 14.0, 10.0, 0.30 * 255.0) / 255.0;
    vec4 c1 = vec4(74.0, 8.0, 5.0, 0.60 * 255.0) / 255.0;
    if (t <= 0.62)
        return vec4(c0.rgb, 0.0);
    if (t <= 0.86)
        return premul_mix(vec4(c0.rgb, 0.0), c0, (t - 0.62) / 0.24);
    if (t <= 1.0)
        return premul_mix(c0, c1, (t - 0.86) / 0.14);
    return c1;
}

// one radial blob: a circle at (cx, cy) of the box whose colour fades to nothing at `reach` of the farthest-corner distance
vec4 blob(vec2 p, vec2 size, vec2 at, float reach, vec4 col) {
    vec2 c = at * size;
    float far = max(max(length(c), length(vec2(size.x, 0.0) - c)), max(length(vec2(0.0, size.y) - c), length(size - c)));
    float t = length(p - c) / (far * reach);
    return vec4(col.rgb, col.a * max(0.0, 1.0 - t));
}

// source-over of straight colours
vec4 over(vec4 top, vec4 bottom) {
    float a = top.a + bottom.a * (1.0 - top.a);
    vec3 c = a > 1e-5 ? (top.rgb * top.a + bottom.rgb * bottom.a * (1.0 - top.a)) / a : vec3(0.0);
    return vec4(c, a);
}

// ow-blood-b: four blobs, the first listed on top
vec4 blood_b(vec2 p, vec2 size) {
    vec4 c0 = vec4(96.0, 10.0, 8.0, 0.75 * 255.0) / 255.0;
    vec4 c1 = vec4(96.0, 10.0, 8.0, 0.70 * 255.0) / 255.0;
    vec4 c2 = vec4(88.0, 10.0, 8.0, 0.75 * 255.0) / 255.0;
    vec4 c3 = vec4(88.0, 10.0, 8.0, 0.70 * 255.0) / 255.0;
    vec4 acc = blob(p, size, vec2(0.74, -0.02), 0.18, c3);
    acc = over(blob(p, size, vec2(0.26, 1.01), 0.19, c2), acc);
    acc = over(blob(p, size, vec2(0.99, 0.58), 0.15, c1), acc);
    acc = over(blob(p, size, vec2(0.02, 0.22), 0.17, c0), acc);
    return acc;
}

void main() {
    vec2 size = hurt.box.xy;
    vec2 p = v_uv * size;
    int mode = int(hurt.box.z + 0.5);
    float opacity = hurt.box.w;
    if (mode == 0) {
        // feTurbulence + feDisplacementMap: the source is read at p + amplitude * (noise - 0.5), per axis
        vec2 q = p * hurt.warp.yz;
        vec2 d = vec2(hz_fbm(q + vec2(hurt.warp.w, 0.0), 4), hz_fbm(q + vec2(0.0, hurt.warp.w) + 31.7, 4)) - 0.5;
        vec2 pw = p + d * hurt.warp.x;
        vec4 a = vec4(0.0);
        vec4 b = vec4(0.0);
        if (pw.x >= 0.0 && pw.y >= 0.0 && pw.x <= size.x && pw.y <= size.y) {
            a = blood_a(pw, size);
            b = blood_b(pw, size);
            b.a *= 0.5;
        }
        // blood-b multiplies over blood-a inside the wrapper's own stacking context
        float ar = b.a + a.a * (1.0 - b.a);
        vec3 cr = ar > 1e-5 ? ((1.0 - b.a) * a.a * a.rgb + b.a * (1.0 - a.a) * b.rgb + b.a * a.a * a.rgb * b.rgb) / ar : vec3(0.0);
        frag_color = cod_ui_out(vec4(cr, ar * opacity));
    } else if (mode == 1) {
        // ow-hitflash: ellipse 90% 86%, rgba(150,16,10,.22) to 40 %, rgba(160,18,12,.62) at 100 %, mix-blend-mode screen
        float t = length((p - 0.5 * size) / (vec2(0.90, 0.86) * size));
        vec4 c0 = vec4(150.0, 16.0, 10.0, 0.22 * 255.0) / 255.0;
        vec4 c1 = vec4(160.0, 18.0, 12.0, 0.62 * 255.0) / 255.0;
        vec4 c = t <= 0.4 ? c0 : premul_mix(c0, c1, min((t - 0.4) / 0.6, 1.0));
        float a = c.a * opacity;
        // screen blend: out = src + dst * (1 - src) with the premultiplied linear colour as src
        frag_color = vec4(srgb_to_linear_exact(c.rgb) * a, a);
    } else {
        // ow-lowbeat: ellipse 76% 70%, transparent to 64 %, rgba(150,14,10,.34) at 100 %
        float t = length((p - 0.5 * size) / (vec2(0.76, 0.70) * size));
        vec4 c1 = vec4(150.0, 14.0, 10.0, 0.34 * 255.0) / 255.0;
        vec4 c = t <= 0.64 ? vec4(c1.rgb, 0.0) : premul_mix(vec4(c1.rgb, 0.0), c1, min((t - 0.64) / 0.36, 1.0));
        frag_color = cod_ui_out(vec4(c.rgb, c.a * opacity));
    }
}
@end
