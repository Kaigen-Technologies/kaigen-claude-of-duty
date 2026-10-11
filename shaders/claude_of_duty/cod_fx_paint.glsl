// The shared painter kit of the FX bakes: the noise vocabulary over the engine's procgen lattice. Included inside an @fs block; every painter evaluates a point (x, y) in -1..1 with radius r.
#include "procgen/pg_noise.glsl"

// the lattice never wraps inside a tile: the period is far beyond any coordinate a painter reaches
#define CF_PERIOD ivec2(8192, 8192)

float cf_clamp01(float v) { return clamp(v, 0.0, 1.0); }

// the smoothstep: the edges may run backwards
float cf_sm(float a, float b, float x) {
    float d = b - a;
    float t = cf_clamp01((x - a) / (abs(d) < 1e-6 ? 1e-6 : d));
    return t * t * (3.0 - 2.0 * t);
}

// pow of a value that may brush zero from below
float cf_pow(float v, float e) { return pow(max(v, 0.0), e); }

float cf_perlin(vec2 p, uint s) { return pg_perlin(p, CF_PERIOD, s); }
float cf_fbm(vec2 p, int oct, uint s) { return pg_fbm01(p, CF_PERIOD, oct, 0.5, s); }
float cf_ridged(vec2 p, int oct, uint s) { return pg_ridged(p, CF_PERIOD, oct, 0.5, s); }
float cf_worley(vec2 p, uint s) { return min(1.0, pg_worley(p, CF_PERIOD, 1.0, s).x); }
float cf_worley_edge(vec2 p, uint s) {
    vec4 w = pg_worley(p, CF_PERIOD, 1.0, s);
    return min(1.0, w.y - w.x);
}

// domain-warped fbm
float cf_warped(vec2 p, float warp, int oct, uint s) {
    float wx = cf_perlin(p * 0.7 + vec2(13.1, -4.2), s) * warp;
    float wy = cf_perlin(p * 0.7 + vec2(-8.6, 21.5), s) * warp;
    return cf_fbm(p + vec2(wx, wy), oct, s);
}

// the painter's frame: x, y in -1..1 with +y up the sprite
struct CfPt {
    vec2 p;
    float r;
    uint s;
};
