// the COD motion blur's tile max over the upscaler's R32F device-depth attachment
#include "common.glsl"

@cs
// r32f is unfilterable-float on WebGPU: texelFetch only
@unfilterable
layout(set = 1, binding = 3) uniform texture2D depth_tex;
#include "cod_post_tile_body.glsl"
@end
