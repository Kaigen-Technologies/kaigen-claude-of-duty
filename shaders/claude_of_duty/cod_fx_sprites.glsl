// The particle sprite atlas: 16 tiles in a 4x4 sheet, painted in one fullscreen pass at load. RGB is a detail/tint field, A is coverage; the particle supplies the colour.
#include "common.glsl"

@vs
layout(location = 0) out vec2 v_uv;

void main() {
    v_uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(v_uv, 0.0);
}
@end

@fs
#include "cod_fx_paint.glsl"

layout(set = 0, binding = 0) uniform _Params {
    float cols;
    float cell_px; // texels a tile spans: the outermost texel of each tile is cleared
    float seed;
    float _pad;
} params;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

// 0 SMOKE_A: billowing puff, wispy eroded rim
vec4 cf_smoke_a(vec2 p, float r, uint s) {
    float w = cf_warped(p * 2.05 + vec2(3.7, -1.4), 0.8, 5, s);
    float lump = cf_worley(p * 2.4 + vec2(7.1, 2.3), s);
    float tear = cf_fbm(p * 7.4 + vec2(-6.2, 3.9), 3, s);
    float a = cf_sm(0.0, 0.58, 1.0 - r + (w - 0.5) * 0.8 + (lump - 0.45) * 0.26);
    // erosion only near the rim keeps the core dense while the silhouette tears
    a *= 1.0 - cf_sm(0.35, 0.95, r) * (1.0 - tear) * 0.85;
    a *= cf_sm(1.02, 0.66, r);
    float det = cf_fbm(p * 4.6 + vec2(-2.1, 8.4), 4, s);
    float l = 0.42 + 0.66 * det * det;
    return vec4(l, l * 0.995, l * 0.99, a);
}

// 1 SMOKE_B: cauliflower lobes, denser core
vec4 cf_smoke_b(vec2 p, float r, uint s) {
    float cw = 1.0 - cf_worley(p * 3.05 + vec2(-4.3, 6.7), s);
    float f = cf_fbm(p * 1.9 + vec2(12.2, -3.3), 4, s);
    float tear = cf_fbm(p * 8.8 + vec2(1.7, -4.4), 3, s);
    float a = cf_sm(0.0, 0.52, 1.0 - r * 1.02 + (cw - 0.42) * 0.68 + (f - 0.5) * 0.46);
    a *= 1.0 - cf_sm(0.3, 0.92, r) * (1.0 - tear) * 0.9;
    a *= cf_sm(1.0, 0.6, r);
    float det = cf_fbm(p * 6.1 + vec2(4.4, -5.2), 4, s);
    float l = 0.38 + 0.7 * (0.35 * det + 0.65 * cw);
    return vec4(l, l * 0.99, l * 0.975, a);
}

// 2 WISP: torn filaments, drifts and dissipates
vec4 cf_wisp(vec2 p, float r, uint s) {
    float fil = cf_ridged(vec2(p.x * 2.6 + 1.3, p.y * 1.05 - 6.0), 4, s);
    float body = cf_sm(0.0, 0.75, 1.0 - length(vec2(p.x * 1.05, p.y * 0.78)));
    float a = cf_clamp01(cf_sm(0.28, 0.85, fil) * 1.15 + 0.16) * body * body;
    a *= cf_sm(1.05, 0.55, r);
    float l = 0.5 + 0.55 * fil;
    return vec4(l, l * 0.99, l * 0.985, a * 0.9);
}

// 3 DUST: fine grained, sharp-ish falloff, grit specks
vec4 cf_dust(vec2 p, float r, uint s) {
    float w = cf_warped(p * 3.3 + vec2(21.1, 5.9), 0.55, 5, s);
    float grit = cf_fbm(p * 15.5 + vec2(3.3, -7.7), 2, s);
    float a = cf_sm(0.0, 0.5, 1.0 - r * 1.04 + (w - 0.5) * 0.86);
    a *= 1.0 - cf_sm(0.25, 0.9, r) * (1.0 - grit) * 0.8;
    a *= cf_sm(1.0, 0.62, r);
    // grain, not snow: a soft ramp instead of a threshold
    float l = 0.48 + 0.42 * w + 0.16 * cf_sm(0.6, 0.92, grit);
    return vec4(min(1.0, l), min(1.0, l * 0.985), min(1.0, l * 0.96), a);
}

// 4 SPARK: point with a tight bloom halo
vec4 cf_spark(vec2 p, float r, uint s) {
    float core = exp(-r * r * 26.0);
    float glow = exp(-r * r * 4.5) * 0.4;
    float a = cf_clamp01(core + glow) * cf_sm(1.0, 0.85, r);
    return vec4(1.0, 1.0, 1.0, a);
}

// 5 STREAK: capsule along +Y, bright head, fading tail
vec4 cf_streak(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float half_w = 0.26 * (0.55 + 0.45 * cf_sm(-1.0, 0.6, y));
    float q = abs(x) / half_w;
    float core = exp(-q * q * 3.0);
    float along = cf_sm(-1.02, -0.55, y) * cf_sm(1.02, 0.72, y);
    float taper = 0.1 + 0.9 * cf_sm(-1.0, 0.85, y);
    float jag = 0.85 + 0.3 * cf_fbm(vec2(x * 6.0 + 2.2, y * 3.1 - 8.8), 3, s);
    float a = core * along * taper * jag;
    a += exp(-(x * x * 90.0 + (y - 0.72) * (y - 0.72) * 30.0)) * 0.9; // hot head
    return vec4(1.0, 1.0, 1.0, cf_clamp01(a));
}

// 6 FLASH_LOBE: ONE tongue of burning gas, root at the -X edge, tip toward +X; deliberately one-sided
vec4 cf_flash_lobe(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float u = cf_clamp01((x + 1.0) * 0.5); // 0 at the root, 1 at the tip
    // swells just past the crown where the gas is still choked, then tapers to a torn point
    float swell = pow(u + 0.02, 0.5) * pow(max(1.0 - u, 0.0), 0.7);
    // the tongue leans and wanders instead of running straight down +X
    float lean = 0.26 * u * u - 0.07 * u + 0.2 * (cf_fbm(vec2(u * 2.4 + 4.1, 8.3), 3, s) - 0.5) * u;
    float w = 0.04 + 1.5 * swell;
    // shear only the +Y flank: a smooth pressure face on one side, a shredded shear layer on the other
    float shear = cf_fbm(vec2(u * 4.6 - 3.3, y * 2.2 + 1.9), 4, s);
    float flank = cf_sm(0.0, 0.5, (y - lean) / max(w, 1e-3));
    w *= 1.0 - flank * (1.0 - shear) * 0.5;
    float q = (y - lean) / max(w, 1e-3);
    float a = exp(-q * q * 2.0);
    // the tip has burnt out and is tearing into filaments
    float frag = cf_fbm(vec2(u * 6.8 + 12.7, y * 4.4 - 6.4), 4, s);
    a *= 1.0 - cf_sm(0.46, 1.0, u) * (1.0 - frag) * 1.5;
    a *= 0.72 + 0.46 * cf_fbm(vec2(u * 7.9 - 8.1, y * 3.6 + 15.2), 4, s);
    // a rounded root cap, and the tip never touches the tile edge
    a *= cf_sm(-1.0, -0.86, x) * cf_sm(1.0, 0.86, u);
    a += exp(-((x + 0.8) * (x + 0.8) * 13.0 + y * y * 30.0)) * 0.85;
    // white-hot at the choke, orange through the body, sooty at the shredding tip and the cool edges
    float edge = cf_clamp01(abs(q) * 0.72);
    float t = cf_clamp01(cf_sm(0.02, 0.72, u) * 0.85 + edge * 0.4);
    float soot = cf_sm(0.62, 1.0, u) * 0.34;
    float den = 0.82 + 0.34 * shear;
    return vec4(cf_clamp01((1.0 - soot) * den), cf_clamp01((1.0 - 0.52 * t) * (1.0 - soot * 1.3) * den),
                cf_clamp01((1.0 - 0.84 * t) * (1.0 - soot * 1.6) * den), cf_clamp01(a));
}

// 7 FLASH_CORE: the choked, boiling gas ball at the crown; the silhouette is warped per direction and the interior churns
vec4 cf_flash_core(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float ang = atan(y, x);
    float lump = cf_fbm(vec2(cos(ang) * 1.7 + 30.1, sin(ang) * 1.7 + 9.4), 3, s);
    float churn = cf_fbm(vec2(x * 3.1 - 5.5, y * 3.1 + 2.2), 4, s);
    float rr = r * (0.84 + 0.32 * lump) * (0.94 + 0.14 * churn);
    float a = exp(-rr * rr * 15.0) * (0.74 + 0.48 * churn) + exp(-rr * rr * 44.0) * 0.95;
    a *= cf_sm(1.02, 0.72, r);
    float t = cf_sm(0.0, 0.7, rr);
    return vec4(1.0, 1.0 - 0.2 * t, 1.0 - 0.5 * t, cf_clamp01(a));
}

// 8 CHIP: solid angular fleck of masonry
vec4 cf_chip(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float ang = atan(y, x);
    float shape = 0.52 + 0.34 * cf_fbm(vec2(cos(ang) * 1.8 + 5.5, sin(ang) * 1.8 + 2.9), 2, s) - 0.1 * abs(sin(ang * 2.5));
    float a = cf_sm(shape, shape - 0.09, r);
    float facet = cf_fbm(vec2(x * 3.4 - 4.2, y * 3.4 + 1.1), 3, s);
    float l = 0.34 + 0.5 * facet + 0.22 * cf_clamp01(0.5 - y);
    return vec4(l, l * 0.985, l * 0.955, a);
}

// 9 SPLINTER: tapered sliver with visible grain
vec4 cf_splinter(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float bend = 0.16 * sin(y * 2.1 + 1.3);
    float w = 0.155 * (1.0 - 0.72 * abs(y)) * (0.7 + 0.6 * cf_fbm(vec2(y * 4.4 + 2.2, 7.7), 2, s));
    float d = abs(x - bend) / max(0.02, w);
    float a = cf_sm(1.0, 0.72, d) * cf_sm(1.0, 0.9, abs(y));
    float grain = cf_fbm(vec2(x * 9.0 + 1.1, y * 2.2 - 3.3), 3, s);
    float l = 0.3 + 0.5 * grain;
    return vec4(l, l * 0.82, l * 0.6, a);
}

// 10 DROPLET: teardrop with a specular highlight (blood, water)
vec4 cf_droplet(vec2 p, float r, uint s) {
    float x = p.x;
    float yy = p.y * 0.92;
    float head = length(vec2(x, (yy - 0.16) * 1.05)) / 0.5;
    float tail_w = 0.42 * cf_clamp01(0.9 - yy * 0.75);
    float tail = length(vec2(x / max(0.03, tail_w), (yy + 0.5) * 0.85));
    float a = cf_clamp01(cf_sm(1.0, 0.82, head) + cf_sm(1.0, 0.55, tail) * 0.9);
    float spec = exp(-((x + 0.16) * (x + 0.16) + (yy - 0.3) * (yy - 0.3)) * 26.0);
    float l = 0.55 + 0.45 * spec + 0.16 * cf_clamp01(0.4 - yy);
    return vec4(min(1.0, l), min(1.0, l * 0.9), min(1.0, l * 0.86), a);
}

// 11 MIST: very soft aerosol cloud (blood mist, fine spray)
vec4 cf_mist(vec2 p, float r, uint s) {
    float f = cf_fbm(p * 5.2 + vec2(31.4, -12.6), 5, s);
    float a = cf_sm(0.04, 0.9, (1.0 - r) * 0.95 + (f - 0.5) * 0.95) * 0.78;
    a *= cf_sm(1.02, 0.5, r);
    float l = 0.5 + 0.5 * f;
    return vec4(l, l * 0.97, l * 0.96, a);
}

// 12 SPLASH: sheet of water, dense at the base, shredding at the crown
vec4 cf_splash(vec2 p, float r, uint s) {
    float x = p.x, y = p.y;
    float width = 0.66 - 0.3 * cf_sm(-1.0, 1.0, y);
    float col = 1.0 - abs(x) / max(0.05, width);
    float body = cf_warped(vec2(x * 2.4 + 2.4, y * 1.5 - 9.1), 0.7, 4, s);
    float shred = cf_fbm(vec2(x * 5.5 - 1.2, y * 3.0 + 4.4), 4, s);
    float a = cf_sm(0.0, 0.5, col + (body - 0.5) * 0.55);
    // the higher up the sheet, the more it has broken into ligaments
    a *= 1.0 - cf_sm(-0.2, 1.0, y) * (1.0 - shred) * 0.95;
    a *= cf_sm(1.04, 0.84, abs(y));
    float drop = exp(-((x - 0.12) * (x - 0.12) * 55.0 + (y - 0.74) * (y - 0.74) * 45.0));
    a = cf_clamp01(a + drop * 0.5);
    float l = 0.58 + 0.42 * body + 0.12 * shred;
    return vec4(min(1.0, l * 0.94), min(1.0, l * 0.98), min(1.0, l), a);
}

// 13 RING: shockwave / ripple annulus
vec4 cf_ring(vec2 p, float r, uint s) {
    float ang = atan(p.y, p.x);
    float wob = 1.0 + 0.055 * (cf_fbm(vec2(cos(ang) * 3.1 + 6.6, sin(ang) * 3.1 - 2.2), 3, s) - 0.5);
    float t = (r * wob - 0.74) / 0.16;
    float a = exp(-t * t * 3.2);
    a += cf_sm(0.78, 0.2, r) * 0.1;
    return vec4(1.0, 1.0, 1.0, cf_clamp01(a) * cf_sm(1.0, 0.94, r));
}

// 14 FIRE: dense billow, white-hot core to deep orange rim
vec4 cf_fire(vec2 p, float r, uint s) {
    float w = cf_warped(p * 2.5 + vec2(9.2, 17.5), 0.95, 5, s);
    float a = cf_sm(0.08, 0.6, 1.0 - r * 0.98 + (w - 0.5) * 0.8);
    a *= cf_sm(1.02, 0.6, r);
    float heat = cf_clamp01(1.15 - r * 1.35 + (w - 0.5) * 0.65);
    return vec4(1.0, 0.28 + 0.7 * heat, 0.06 + 0.82 * cf_pow(heat, 2.6), a);
}

// 15 MOTE: airborne speck, slightly irregular
vec4 cf_mote(vec2 p, float r, uint s) {
    float j = 1.0 + 0.3 * (cf_fbm(p * 5.0 + vec2(44.2, -17.3), 2, s) - 0.5);
    float a = exp(-(r * j) * (r * j) * 11.0) * 0.95;
    return vec4(1.0, 0.99, 0.96, a * cf_sm(1.0, 0.8, r));
}

vec4 cf_paint(uint id, vec2 p, float r, uint s) {
    vec4 c;
    switch (id) {
    case 0u: c = cf_smoke_a(p, r, s); break;
    case 1u: c = cf_smoke_b(p, r, s); break;
    case 2u: c = cf_wisp(p, r, s); break;
    case 3u: c = cf_dust(p, r, s); break;
    case 4u: c = cf_spark(p, r, s); break;
    case 5u: c = cf_streak(p, r, s); break;
    case 6u: c = cf_flash_lobe(p, r, s); break;
    case 7u: c = cf_flash_core(p, r, s); break;
    case 8u: c = cf_chip(p, r, s); break;
    case 9u: c = cf_splinter(p, r, s); break;
    case 10u: c = cf_droplet(p, r, s); break;
    case 11u: c = cf_mist(p, r, s); break;
    case 12u: c = cf_splash(p, r, s); break;
    case 13u: c = cf_ring(p, r, s); break;
    case 14u: c = cf_fire(p, r, s); break;
    default: c = cf_mote(p, r, s); break;
    }
    return c;
}

void main() {
    float cols = max(params.cols, 1.0);
    vec2 cell = floor(v_uv * cols);
    uint id = uint(cell.y * cols + cell.x);
    vec2 f = fract(v_uv * cols);
    // +y is up the sprite: the texture's first row is the top of the quad
    vec2 p = vec2(f.x * 2.0 - 1.0, 1.0 - f.y * 2.0);
    float r = length(p);
    // the 1 px gutter: bilinear and mip bleed never cross a tile
    float edge = 1.0 - 2.0 / max(params.cell_px, 4.0);
    vec4 c = vec4(0.0);
    if (r < 1.45 && max(abs(p.x), abs(p.y)) <= edge) {
        uint s = uint(params.seed) * 2654435761u ^ (id * 0x9E3779B9u + 0x85EBCA6Bu);
        c = cf_paint(id, p, r, s);
    }
    frag_color = vec4(c.rgb, cf_clamp01(c.a));
}
@end
