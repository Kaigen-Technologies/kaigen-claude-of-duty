// Height to tangent normal: the 16-bit height is the albedo alpha plus the ORM alpha residual, read with a wrapping texelFetch so the normals tile.
#include "common.glsl"

@vs
layout(location = 0) out vec2 v_uv;
void main() {
    v_uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(v_uv, 0.0);
}
@end

@fs
layout(set = 0, binding = 0) uniform _Params {
    vec4 head; // x size in texels, y slope over the whole tile (relief / world size)
} params;

layout(set = 1, binding = 0) uniform texture2D albedoTexture;
layout(set = 1, binding = 1) uniform texture2D ormTexture;
layout(set = 1, binding = 2) uniform sampler srcSampler;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec2 frag_normal;

float height_at(ivec2 px, ivec2 o, int mask) {
    ivec2 q = (px + o) & ivec2(mask);
    return texelFetch(sampler2D(albedoTexture, srcSampler), q, 0).a +
           (texelFetch(sampler2D(ormTexture, srcSampler), q, 0).a - 0.5) / 255.0;
}

void main() {
    int size = int(params.head.x);
    int mask = size - 1;
    ivec2 px = ivec2(floor(v_uv * params.head.x));
    float tl = height_at(px, ivec2(-1, 1), mask);
    float t = height_at(px, ivec2(0, 1), mask);
    float tr = height_at(px, ivec2(1, 1), mask);
    float l = height_at(px, ivec2(-1, 0), mask);
    float r = height_at(px, ivec2(1, 0), mask);
    float bl = height_at(px, ivec2(-1, -1), mask);
    float b = height_at(px, ivec2(0, -1), mask);
    float br = height_at(px, ivec2(1, -1), mask);

    float dx = ((tr + 2.0 * r + br) - (tl + 2.0 * l + bl)) * 0.125;
    float dy = ((tl + 2.0 * t + tr) - (bl + 2.0 * b + br)) * 0.125;
    float sx = dx * params.head.x;
    float sy = dy * params.head.x;

    vec3 n = normalize(vec3(-sx * params.head.y, -sy * params.head.y, 1.0));
    frag_normal = n.xy * 0.5 + 0.5;
}
@end
