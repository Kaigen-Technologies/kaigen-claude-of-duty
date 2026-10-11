// The HUD's masked scrims: one colour whose alpha is a vertical profile times
// two horizontal profiles (the element's own gradient and a css mask), each piecewise linear over up to 5 stops.
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

layout(set = 0, binding = 1) uniform _BandParams {
    vec4 color;  // rgb sRGB 0..1, a the peak alpha times every fade above the element
    vec4 vt0;    // vertical profile stop positions (0..3), the fifth in vt1.x
    vec4 vt1;
    vec4 va0;    // and their alphas
    vec4 va1;
    vec4 ht0;    // horizontal profile
    vec4 ht1;
    vec4 ha0;
    vec4 ha1;
    vec4 mt0;    // horizontal mask
    vec4 mt1;
    vec4 ma0;
    vec4 ma1;
    vec4 counts; // x vertical stops, y horizontal stops, z mask stops
} band;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

void main() {
    float a = band.color.a;
    a *= cod_profile(band.vt0, band.vt1, band.va0, band.va1, band.counts.x, v_uv.y);
    a *= cod_profile(band.ht0, band.ht1, band.ha0, band.ha1, band.counts.y, v_uv.x);
    a *= cod_profile(band.mt0, band.mt1, band.ma0, band.ma1, band.counts.z, v_uv.x);
    frag_color = cod_ui_out(vec4(band.color.rgb, a));
}
@end
