// the exposure meter in one workgroup (included inside a kernel's @cs after its depth_tex): the 64x64 log-luminance meter (4 taps, tap clamp, centre weight, sky de-weight), its mean and the adaptation
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer _State { float data[]; } state;

layout(set = 1, binding = 0) uniform _Params {
    vec4 meter;  // x sky weight, y far distance (m), z tap clamp, w scene units per pre-exposed value
    vec4 knee;   // xy the sky de-weight's luminance ramp, zw the depth linearization (distance = z / (depth + w))
    vec4 adapt;  // x dt, y speed up, z speed down, w EV bias
    vec4 limits; // x min EV, y max EV, z reset, w key
    vec4 src;    // xy the colour's active size, z log2 of the engine units per scene unit
} params;

layout(set = 2, binding = 0) uniform texture2D color_tex;
@clamp
layout(set = 2, binding = 1) uniform sampler color_sampler;
@nearest
layout(set = 2, binding = 3) uniform sampler depth_sampler;

shared vec2 s_acc[256];

float cod_meter_tap(vec2 uv) {
    vec3 c = max(textureLod(sampler2D(color_tex, color_sampler), uv, 0.0).rgb, vec3(0.0));
    return min(dot(c, vec3(0.2126, 0.7152, 0.0722)) * params.meter.w, params.meter.z);
}

vec2 cod_meter_sample(uint s) {
    vec2 uv = (vec2(float(s & 63u), float(s >> 6u)) + 0.5) / 64.0;
    vec2 tex = vec2(textureSize(sampler2D(color_tex, color_sampler), 0));
    vec2 scale = params.src.xy / tex;
    vec2 texel = 1.0 / tex;
    vec2 at = uv * scale;
    float lum = cod_meter_tap(at + vec2(-1.0, -1.0) * texel);
    lum += cod_meter_tap(at + vec2(1.0, -1.0) * texel);
    lum += cod_meter_tap(at + vec2(-1.0, 1.0) * texel);
    lum += cod_meter_tap(at + vec2(1.0, 1.0) * texel);
    lum = max(lum * 0.25, 1e-5);
    vec2 d = (uv - 0.5) * 2.0;
    float w = exp(-dot(d, d) * 1.1);
    ivec2 dsize = textureSize(sampler2D(depth_tex, depth_sampler), 0);
    ivec2 dpx = min(ivec2(uv * vec2(dsize)), dsize - 1);
    float z = texelFetch(sampler2D(depth_tex, depth_sampler), dpx, 0).r;
    float dist = z > 0.0 ? params.knee.z / (z + params.knee.w) : 1e30;
    if (dist > params.meter.y)
        w *= mix(1.0, params.meter.x, smoothstep(params.knee.x, params.knee.y, lum));
    return vec2(log2(lum) * w, w);
}

void main() {
    uint t = gl_LocalInvocationIndex;
    vec2 acc = vec2(0.0);
    for (uint s = t; s < 4096u; s += 256u)
        acc += cod_meter_sample(s);
    s_acc[t] = acc;
    barrier();
    for (uint i = 128u; i > 0u; i >>= 1u) {
        if (t < i)
            s_acc[t] += s_acc[t + i];
        barrier();
    }
    if (t != 0u)
        return;
    float lum = max(exp2(s_acc[0].x / max(s_acc[0].y, 1e-4)), 1e-5);
    float target = clamp(log2(lum * 100.0 / 12.5) + params.adapt.w, params.limits.x, params.limits.y);
    // the state keeps engine units: [1] the adapted EV100, [2] this frame's target
    float prev = params.limits.z > 0.5 ? target : state.data[1] - params.src.z;
    float speed = target > prev ? params.adapt.z : params.adapt.y;
    float ev = mix(prev, target, clamp(1.0 - exp(-params.adapt.x * speed), 0.0, 1.0));
    state.data[0] = params.limits.w / (1.2 * exp2(ev + params.src.z));
    state.data[1] = ev + params.src.z;
    state.data[2] = target + params.src.z;
}
