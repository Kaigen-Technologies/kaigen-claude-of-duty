// a short ray marched through the depth buffer toward the key light; x = the shadow, y = the view depth the blur weighs by
#include "common.glsl"
#include "hz_noise.glsl"

@cs

layout(set = 0, binding = 0) uniform _Params {
    mat4 view;  // world -> view
    vec4 sun;   // xyz the direction toward the key light, world
    vec4 proj;  // xy the view tangents, zw: view distance = z / (depth + w)
    vec4 size;  // xy the target's size, zw its reciprocal
    vec4 shot;  // x ray length (m), y thickness (m), z frame, w strength
    vec4 band;  // x the projection override's depth band start (2 = none)
} params;

layout(set = 1, binding = 0, rg16f) writeonly uniform image2D contact_out;
@depth
layout(set = 1, binding = 1) uniform texture2D depth_tex;
layout(set = 1, binding = 2) uniform texture2D normal_tex;
@nearest
layout(set = 1, binding = 3) uniform sampler point_sampler;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define COD_CS_STEPS 14

float cod_view_depth(float d) { return params.proj.z / (d + params.proj.w); }

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = ivec2(params.size.xy);
    if (px.x >= size.x || px.y >= size.y) return;
    float d = texelFetch(sampler2D(depth_tex, point_sampler), px, 0).r;
    // reverse-Z: 0 is the far plane, no surface; the viewmodel's band is not a world surface
    if (d <= 0.0 || d >= params.band.x) {
        imageStore(contact_out, px, vec4(1.0, 1e4, 0.0, 1.0));
        return;
    }
    float depth = cod_view_depth(d);
    vec2 uv = (vec2(px) + 0.5) * params.size.zw;
    vec2 ndc = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    // the view space is the GTAO one: Y up, Z the positive distance
    vec3 P = vec3(ndc * params.proj.xy * depth, depth);
    vec3 N = mat3(params.view) * oct_decode_normal(texelFetch(sampler2D(normal_tex, point_sampler), px, 0).rg);
    vec3 L = mat3(params.view) * params.sun.xyz;
    N.z = -N.z;
    L.z = -L.z;

    if (dot(N, L) <= 0.02) {
        imageStore(contact_out, px, vec4(1.0, depth, 0.0, 1.0));
        return;
    }

    float len = params.shot.x * clamp(depth * 0.08 + 0.75, 0.75, 2.5);
    float jitter = hz_ign(vec2(px) + 0.5 + params.shot.z * 3.1717);
    vec3 origin = P + N * (0.012 + depth * 0.0015);
    vec3 step_v = L * (len / float(COD_CS_STEPS));

    float occ = 0.0;
    for (int i = 0; i < COD_CS_STEPS; i++) {
        vec3 sp = origin + step_v * (float(i) + jitter);
        if (sp.z <= 1e-3) break;
        vec2 sndc = sp.xy / (sp.z * params.proj.xy);
        vec2 suv = vec2(sndc.x * 0.5 + 0.5, 0.5 - sndc.y * 0.5);
        if (suv.x <= 0.0 || suv.x >= 1.0 || suv.y <= 0.0 || suv.y >= 1.0) break;
        float sd = texelFetch(sampler2D(depth_tex, point_sampler), min(ivec2(suv * params.size.xy), size - 1), 0).r;
        if (sd <= 0.0 || sd >= params.band.x) continue;
        float scene_depth = cod_view_depth(sd);
        float diff = sp.z - scene_depth;
        float bias = 0.004 + scene_depth * 0.0025;
        if (diff > bias && diff < params.shot.y) {
            // fades with the distance travelled so the shadow dissolves rather than ends
            float t = (float(i) + jitter) / float(COD_CS_STEPS);
            occ = max(occ, 1.0 - t * t);
            break;
        }
    }
    imageStore(contact_out, px, vec4(1.0 - occ * params.shot.w, depth, 0.0, 1.0));
}
@end
