// the COD motion blur over the scene depth attachment
#include "common.glsl"
#include "hz_noise.glsl"

@cs
@depth
layout(set = 1, binding = 3) uniform texture2D depth_tex;
#include "cod_post_blur_body.glsl"
@end
