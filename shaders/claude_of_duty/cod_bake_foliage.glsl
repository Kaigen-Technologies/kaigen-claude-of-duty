// Foliage bake: one rotated, serrated leaf per cell over a 3x3 neighbourhood; the height channel is the cutout mask.
#include "common.glsl"

@vs
layout(location = 0) out vec2 v_uv;
void main() {
    v_uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(v_uv, 0.0);
}
@end

@fs
#include "cod_bake_head.glsl"

void owSurface(vec2 uv, out vec3 alb, out float h, out float rough, out float metal, out float ao) {
    const vec2 P = vec2(8.0);
    const float CELLS = 5.0;
    vec2 p = uv * P + uSeed * 5.9;

    vec2 lp = uv * CELLS;
    vec2 ip = floor(lp), fp = fract(lp);

    float bestCover = 0.0;
    float bestDepth = -1.0;
    vec3 bestCol = vec3(0.0);
    float bestVein = 0.0;

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 g = vec2(float(x), float(y));
            vec2 cell = mod(ip + g, vec2(CELLS));
            vec4 r = owHash42(cell + uSeed * 2.0);
            vec4 r2 = owHash42(cell * 1.7 + 9.0 + uSeed);
            vec2 centre = g + 0.15 + r.xy * 0.7 - fp;
            float ang = r.z * 6.28318;
            vec2 q = owRot(centre, ang);
            // an ellipse pinched at both ends
            vec2 s = vec2(0.30 + r.w * 0.16, 0.13 + r2.x * 0.07);
            vec2 e = q / s;
            float d = length(e);
            float pinch = 1.0 - 0.55 * abs(e.x) * 0.5;
            float serr = sin(atan(e.y, e.x) * 26.0) * 0.03;
            float cover = smoothstep(1.02 + serr, 0.88 + serr, d / max(pinch, 0.3));
            if (cover > 0.01) {
                float depth = r2.y;
                if (depth > bestDepth) {
                    float vein = 1.0 - smoothstep(0.0, 0.05, abs(e.y * s.y));
                    float sideV = smoothstep(0.75, 1.0, abs(fract(e.x * 5.0 + e.y * 2.0) * 2.0 - 1.0));
                    vein = clamp(vein + sideV * 0.45 * cover, 0.0, 1.0);
                    vec3 cYoung = owSRGB(vec3(0.180, 0.330, 0.090));
                    vec3 cOld = owSRGB(vec3(0.095, 0.185, 0.060));
                    vec3 cDry = owSRGB(vec3(0.390, 0.320, 0.110));
                    vec3 lc = mix(cYoung, cOld, r2.z);
                    lc = mix(lc, cDry, smoothstep(0.55, 1.0, r2.w) * 0.8);
                    // blotches and mildew spots
                    float spots = owFbm01(p * 22.0, P * 22.0, 3, 0.5);
                    lc *= 0.85 + 0.30 * spots;
                    lc = mix(lc, cDry * 0.7, smoothstep(0.78, 0.95, spots) * 0.5);
                    lc = mix(lc, lc * 1.35, vein * 0.5);
                    bestDepth = depth;
                    bestCover = cover;
                    bestCol = lc;
                    bestVein = vein;
                }
            }
        }
    }

    float fine = owFbm01(p * 12.0, P * 12.0, 3, 0.5);
    alb = clamp(bestCol * (0.955 + 0.085 * fine), vec3(0.02), vec3(0.7));
    h = bestCover;
    rough = clamp(0.62 + (1.0 - bestVein) * 0.14 + (fine - 0.5) * 0.10, 0.35, 0.95);
    metal = 0.0;
    ao = clamp(0.55 + bestDepth * 0.45, 0.3, 1.0);
}

#include "cod_bake_tail.glsl"
@end
