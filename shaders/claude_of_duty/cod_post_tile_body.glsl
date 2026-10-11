// the blur tiles (included inside a kernel's @cs after its depth_tex): the velocity buffer's max-length vector per 16x16 tile (8x8 taps over the tile), as a UV delta
layout(set = 0, binding = 0) uniform _Params {
    vec4 size; // xy the tile texture's size, z the projection override's depth band start (2 = none)
} params;

layout(set = 1, binding = 0, rg16f) writeonly uniform image2D tile_out;
layout(set = 1, binding = 1) uniform texture2D velocity_tex;
@nearest
layout(set = 1, binding = 2) uniform sampler point_sampler;
@nearest
layout(set = 1, binding = 4) uniform sampler depth_sampler;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

void main() {
    ivec2 tile = ivec2(gl_GlobalInvocationID.xy);
    if (tile.x >= int(params.size.x) || tile.y >= int(params.size.y)) return;
    ivec2 vsize = textureSize(sampler2D(velocity_tex, point_sampler), 0);
    ivec2 dsize = textureSize(sampler2D(depth_tex, depth_sampler), 0);
    vec2 uv = (vec2(tile) + 0.5) / params.size.xy;
    vec2 best = vec2(0.0);
    float best_len = 0.0;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            vec2 o = (vec2(float(x), float(y)) - 3.5) * 2.0;
            ivec2 p = clamp(ivec2(floor(uv * vec2(vsize) + o)), ivec2(0), vsize - 1);
            // the viewmodel is not in the velocity buffer: its motion never dilates over the world
            ivec2 dp = min(ivec2((vec2(p) + 0.5) / vec2(vsize) * vec2(dsize)), dsize - 1);
            if (texelFetch(sampler2D(depth_tex, depth_sampler), dp, 0).r >= params.size.z) continue;
            // the gbuffer stores NDC deltas (Y up); the units are UV deltas
            vec2 v = texelFetch(sampler2D(velocity_tex, point_sampler), p, 0).rg * vec2(0.5, -0.5);
            float l = dot(v, v);
            if (l > best_len) { best_len = l; best = v; }
        }
    }
    imageStore(tile_out, tile, vec4(best, 0.0, 1.0));
}
