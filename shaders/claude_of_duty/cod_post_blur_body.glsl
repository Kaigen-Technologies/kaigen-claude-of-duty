// the motion blur (included inside a kernel's @cs after its depth_tex): tile-dilated, depth-weighted, frame-time-scaled per-object motion blur

layout(set = 0, binding = 0) uniform _Params {
    vec4 shot;  // x shutter (already scaled by 1/60/dt), y max radius px, z frame, w intensity
    vec4 size;  // xy the output texture's size, zw the colour's active size
    vec4 depth; // x, y: distance = x / (depth + y); z the projection override's depth band start (2 = none)
} params;

layout(set = 1, binding = 0, rgba16f) writeonly uniform image2D color_out;
layout(set = 1, binding = 1) uniform texture2D color_tex;
@clamp
layout(set = 1, binding = 2) uniform sampler linear_clamp;
layout(set = 1, binding = 4) uniform texture2D velocity_tex;
@nearest
layout(set = 1, binding = 5) uniform sampler point_sampler;
layout(set = 1, binding = 6) uniform texture2D tile_tex;
@nearest
layout(set = 1, binding = 7) uniform sampler depth_sampler;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#define COD_MB_TAPS 12
#define COD_FAR 1e5

vec3 cod_mb_color(vec2 uv) {
    vec2 tex = vec2(textureSize(sampler2D(color_tex, linear_clamp), 0));
    return textureLod(sampler2D(color_tex, linear_clamp), uv * (params.size.zw / tex), 0.0).rgb;
}

// view distance of the depth texel under uv: a far sentinel for the sky (reverse-Z: the far plane is 0), -1 on the viewmodel's depth band
float cod_mb_depth(vec2 uv) {
    ivec2 dsize = textureSize(sampler2D(depth_tex, depth_sampler), 0);
    float d = texelFetch(sampler2D(depth_tex, depth_sampler), min(ivec2(uv * vec2(dsize)), dsize - 1), 0).r;
    if (d >= params.depth.z) return -1.0;
    return d > 0.0 ? params.depth.x / (d + params.depth.y) : COD_FAR;
}

vec2 cod_mb_velocity(vec2 uv) {
    ivec2 vsize = textureSize(sampler2D(velocity_tex, point_sampler), 0);
    // the gbuffer stores NDC deltas (Y up); the units are UV deltas
    return texelFetch(sampler2D(velocity_tex, point_sampler), min(ivec2(uv * vec2(vsize)), vsize - 1), 0).rg * vec2(0.5, -0.5);
}

void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    if (px.x >= int(params.size.x) || px.y >= int(params.size.y)) return;
    vec2 uv = (vec2(px) + 0.5) / params.size.xy;
    vec2 tex = vec2(textureSize(sampler2D(color_tex, linear_clamp), 0));
    vec4 centre = textureLod(sampler2D(color_tex, linear_clamp), uv * (params.size.zw / tex), 0.0);

    float centre_depth = cod_mb_depth(uv);
    // the viewmodel stays sharp
    if (centre_depth < 0.0) {
        imageStore(color_out, px, centre);
        return;
    }
    vec2 tile_vel = textureLod(sampler2D(tile_tex, linear_clamp), uv, 0.0).rg;
    vec2 own_vel = cod_mb_velocity(uv);
    vec2 vel = (dot(tile_vel, tile_vel) > dot(own_vel, own_vel) ? tile_vel : own_vel) * params.shot.x;

    float pixels = length(vel * params.size.xy);
    if (pixels < 1.0) {
        imageStore(color_out, px, centre);
        return;
    }
    if (pixels > params.shot.y) vel *= params.shot.y / pixels;

    float jitter = hz_ign(vec2(px) + 0.5 + params.shot.z * 2.717) - 0.5;

    vec3 sum = centre.rgb;
    float wsum = 1.0;
    for (int i = 1; i <= COD_MB_TAPS; i++) {
        float t = (float(i) + jitter) / float(COD_MB_TAPS);
        vec2 o = vel * (t - 0.5);
        for (int s = 0; s < 2; s++) {
            vec2 suv = uv + (s == 0 ? o : -o);
            if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
            float d = cod_mb_depth(suv);
            // the gun is not in the colour at this stage: it never smears over the world
            if (d < 0.0) continue;
            // a sample much further away than the centre is background leaking in
            float w = 1.0 - smoothstep(0.0, 1.5, (d - centre_depth) / max(1.0, centre_depth));
            w = mix(0.15, 1.0, clamp(w, 0.0, 1.0)) * (1.0 - t * 0.35);
            sum += cod_mb_color(suv) * w;
            wsum += w;
        }
    }
    imageStore(color_out, px, vec4(mix(centre.rgb, sum / wsum, params.shot.w), centre.a));
}
