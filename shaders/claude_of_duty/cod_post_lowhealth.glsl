// the low-health treatment in the pre-exposed HDR frame before the meter: desaturation, edge darkening, the arterial rim and the hit flash
#include "common.glsl"

@cs
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) readonly buffer _State { float data[]; } state;

layout(set = 1, binding = 0) uniform _Params {
    vec4 hurt;   // x amount, y pulse, z hit flash, w critical
    vec4 size;   // xy the output texture's size, zw the colour's active size
    vec4 shape;  // xy the square-correcting aspect, z the pre-exposure
} params;

layout(set = 2, binding = 0, rgba16f) writeonly uniform image2D color_out;
layout(set = 2, binding = 1) uniform texture2D color_tex;
@clamp
layout(set = 2, binding = 2) uniform sampler linear_clamp;

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= int(params.size.x) || px.y >= int(params.size.y)) return;
    vec2 uv = (vec2(px) + 0.5) / params.size.xy;
    vec2 tex = vec2(textureSize(sampler2D(color_tex, linear_clamp), 0));
    vec4 src = textureLod(sampler2D(color_tex, linear_clamp), uv * (params.size.zw / tex), 0.0);
    vec3 c = src.rgb;
    float amount = params.hurt.x;
    float pulse = params.hurt.y;
    float flash = params.hurt.z;

    // the frame is pre-exposed: a display-referred overlay is scaled by pre / exposure so the final pass's exposure ratio cancels it
    float exposure = state.data[0];
    float overlay = exposure > 1e-12 ? params.shape.z / exposure : 1.0;

    vec2 d = (uv - 0.5) * params.shape.xy;
    float r = length(d) * 1.414;
    float wide = smoothstep(0.18, 1.0, r);
    float rim = smoothstep(0.34, 1.1, r);
    float beat = amount * (0.32 + 0.68 * pulse);

    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float sat = amount * (0.74 + 0.16 * pulse);
    c = mix(c, vec3(luma) * vec3(0.93, 0.97, 1.06), clamp(sat, 0.0, 0.94));

    c *= 1.0 - wide * (0.40 + 0.28 * beat) * amount;

    // subtractive first so the rim survives the film curve, then a small display-referred glow for the black corners
    float k = rim * beat;
    c *= mix(vec3(1.0), vec3(1.16, 0.26, 0.22), clamp(k * 0.98, 0.0, 1.0));
    c += vec3(0.115, 0.008, 0.005) * k * overlay;

    if (flash > 0.001) {
        float ring = 0.3 + 0.7 * smoothstep(0.05, 0.95, r);
        float f = clamp(flash * ring, 0.0, 1.0);
        c *= mix(vec3(1.0), vec3(1.3, 0.4, 0.34), f);
        c += vec3(0.16, 0.012, 0.008) * f * overlay;
    }

    imageStore(color_out, px, vec4(c, src.a));
}
@end
