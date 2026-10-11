// Shared by the Claude of Duty HUD shaders (masked bands, minimap, hurt layers): the UI quad's partners.
#include "hz_color.glsl"
#include "hz_noise.glsl"

// the UI blends in linear light: css sRGB colours are decoded on the way out
vec4 cod_ui_out(vec4 c) {
    return vec4(srgb_to_linear_exact(c.rgb), c.a);
}

// piecewise-linear profile over up to 5 stops (t in x of t0/t1, value in a0/a1); no stops = constant 1
float cod_stop(vec4 a, vec4 b, int i) {
    return i < 4 ? a[i] : b.x;
}

float cod_profile(vec4 t0, vec4 t1, vec4 a0, vec4 a1, float n, float x) {
    int count = int(n);
    if (count == 0)
        return 1.0;
    if (x <= cod_stop(t0, t1, 0))
        return cod_stop(a0, a1, 0);
    for (int i = 1; i < 5; i++) {
        if (i >= count)
            break;
        float ta = cod_stop(t0, t1, i - 1), tb = cod_stop(t0, t1, i);
        if (x <= tb)
            return mix(cod_stop(a0, a1, i - 1), cod_stop(a0, a1, i), (x - ta) / max(tb - ta, 1e-6));
    }
    return cod_stop(a0, a1, count - 1);
}
