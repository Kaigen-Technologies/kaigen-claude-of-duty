// the COD motion blur over the upscaler's R32F device-depth attachment
#include "common.glsl"
#include "hz_noise.glsl"

@cs
// r32f is unfilterable-float on WebGPU: texelFetch only
@unfilterable
layout(set = 1, binding = 3) uniform texture2D depth_tex;
#include "cod_post_blur_body.glsl"
@end
