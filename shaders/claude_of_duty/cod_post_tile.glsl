// the COD motion blur's tile max over the scene depth attachment
#include "common.glsl"

@cs
@depth
layout(set = 1, binding = 3) uniform texture2D depth_tex;
#include "cod_post_tile_body.glsl"
@end
