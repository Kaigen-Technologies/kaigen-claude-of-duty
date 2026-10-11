// The decal tiles' normal and ORM maps from the painters' height field.
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
    float cols;
    float cell_px;
    float _pad0;
    float _pad1;
} params;

layout(set = 1, binding = 0) uniform texture2D fieldTexture;
layout(set = 1, binding = 1) uniform sampler fieldSampler;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec4 out_orm;

const float RELIEF[16] = float[16](2.6, 3.0, 2.2, 2.4, 2.3, 1.4, 0.8, 0.7, 0.35, 2.4, 1.9, 1.6, 1.1, 1.5, 0.2, 1.7);

float smooth01(float a, float b, float x) {
    float t = clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

vec4 field(vec2 px) { return texelFetch(sampler2D(fieldTexture, fieldSampler), ivec2(px), 0); }

void main() {
    vec2 frag = v_uv * (params.cols * params.cell_px);
    vec2 cell = floor(frag / params.cell_px);
    vec2 px = floor(frag - cell * params.cell_px);
    int id = int(cell.y * params.cols + cell.x);
    vec2 org = cell * params.cell_px;
    // sampled inside the tile only; the image runs down while the decal's y runs up
    vec2 cp = clamp(px, vec2(1.0), vec2(params.cell_px - 2.0));
    float relief = RELIEF[id] * (params.cell_px / 256.0);
    float gx = (field(org + cp + vec2(1.0, 0.0)).x - field(org + cp - vec2(1.0, 0.0)).x) * relief * 8.0;
    float gy = (field(org + cp - vec2(0.0, 1.0)).x - field(org + cp + vec2(0.0, 1.0)).x) * relief * 8.0;
    // the decal shader's bitangent runs down the image, so green is the slope up the decal
    vec3 nrm = normalize(vec3(-gx, gy, 1.0));
    out_normal = vec4(nrm * 0.5 + 0.5, 1.0);
    vec4 f = field(org + px);
    float ao = clamp(0.35 + 0.65 * smooth01(0.05, 0.55, f.x), 0.0, 1.0);
    out_orm = vec4(ao, clamp(f.y, 0.0, 1.0), clamp(f.z, 0.0, 1.0), 1.0);
}
@end
