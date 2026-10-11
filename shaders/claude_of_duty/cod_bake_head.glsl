// The shared head of every surface bake program, included at the top of its @fs block: the parameter block, the outputs and the noise library.
layout(set = 0, binding = 0) uniform _Params {
    vec4 head;   // x seed
    vec4 tint_a; // rgb linear
    vec4 tint_b;
    vec4 param;
} params;
#define uSeed params.head.x
#define uTintA params.tint_a.rgb
#define uTintB params.tint_b.rgb
#define uParam params.param

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_albedo; // rgb albedo (linear in, sRGB stored), a height to 8 bits
layout(location = 1) out vec4 frag_orm;    // occlusion, roughness, metalness, a the height residual

#include "cod_bake_noise.glsl"
