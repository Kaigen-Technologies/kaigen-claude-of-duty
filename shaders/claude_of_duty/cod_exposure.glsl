// the COD auto-exposure meter over the scene depth attachment
#include "common.glsl"

@cs
@depth
layout(set = 2, binding = 2) uniform texture2D depth_tex;
#include "cod_exposure_body.glsl"
@end
