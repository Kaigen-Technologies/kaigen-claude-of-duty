// one separable 5-tap pass over the contact texture, weighed by view depth
#include "common.glsl"

@cs

layout(set = 0, binding = 0) uniform _Params {
    vec4 size; // xy the target's size, zw the direction in texels
} params;

layout(set = 1, binding = 0, rg16f) writeonly uniform image2D blur_out;
layout(set = 1, binding = 1) uniform texture2D src_tex;
@nearest
layout(set = 1, binding = 2) uniform sampler point_sampler;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

vec2 cod_tap(ivec2 p) {
    ivec2 last = ivec2(params.size.xy) - 1;
    return texelFetch(sampler2D(src_tex, point_sampler), clamp(p, ivec2(0), last), 0).rg;
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= int(params.size.x) || px.y >= int(params.size.y)) return;
    ivec2 dir = ivec2(params.size.zw);
    vec2 c = cod_tap(px);
    float sum = c.r * 0.5;
    float wsum = 0.5;
    for (int i = 1; i <= 2; i++) {
        vec2 a = cod_tap(px + dir * i);
        vec2 b = cod_tap(px - dir * i);
        float w = 0.3 / float(i);
        float wa = w * exp(-abs(a.g - c.g) * 40.0 / max(0.1, c.g));
        float wb = w * exp(-abs(b.g - c.g) * 40.0 / max(0.1, c.g));
        sum += a.r * wa + b.r * wb;
        wsum += wa + wb;
    }
    imageStore(blur_out, px, vec4(sum / wsum, c.g, 0.0, 1.0));
}
@end
