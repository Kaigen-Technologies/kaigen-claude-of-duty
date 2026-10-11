// The decal tiles: 16 painters, written as albedo, a normal derived from their height and ORM. A dev bake: the cook turns the dumped tiles into BC7.
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
    float cell_px; // texels a tile spans
    float seed;
    float _pad;
} params;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec4 out_field; // height, roughness, metalness

const float TWO_PI = 6.28318530718;

uint g_salt;

float n_fbm(float x, float y, int oct) { return cf_fbm(vec2(x, y), oct, g_salt); }
float n_worley(float x, float y) { return cf_worley(vec2(x, y), g_salt); }
float n_worley_edge(float x, float y) { return cf_worley_edge(vec2(x, y), g_salt); }
float n_warped(float x, float y, float w, int oct) { return cf_warped(vec2(x, y), w, oct, g_salt); }

float contact_ao(float r, float rb, float peak) {
    if (r <= rb) return peak;
    float t = cf_clamp01((r - rb) / (rb * 0.9));
    return peak * pow(1.0 - t, 2.2);
}

float radial_spall(float ang, float r, float seed_, int count) {
    float c = 0.0;
    for (int k = 0; k < count; k++) {
        float fk = float(k);
        float h = n_fbm(fk * 4.7 + seed_, fk * 2.3 - seed_ * 0.7, 2);
        float h2 = n_fbm(fk * 2.9 - seed_, fk * 5.1 + seed_ * 1.3, 2);
        float len = 0.24 + 0.52 * h2;
        if (r > len) continue;
        float a0 = ((fk + (h - 0.5) * 1.5) / float(count)) * TWO_PI + seed_;
        float d = ang - a0;
        d -= TWO_PI * floor(d / TWO_PI + 0.5);
        float wob = (n_fbm(r * 7.5 + fk * 3.3, seed_ + fk * 1.7, 3) - 0.5) * 0.5;
        float w = 0.03 * (1.0 - r / len) + 0.006;
        float q = (abs(d + wob * r) * r) / w;
        c = max(c, cf_sm(1.0, 0.25, q) * (0.4 + 0.85 * h2) * cf_sm(len, len * 0.55, r));
    }
    return cf_clamp01(c);
}

// the premultiplied accumulator: overlapping layers average, none lightens another
vec4 g_mix;
void mix_reset() { g_mix = vec4(0.0); }
void mix_add(float cov, float r, float g, float b) {
    if (cov <= 0.0) return;
    g_mix += vec4(r * cov, g * cov, b * cov, cov);
}
vec4 mix_into(float edge) {
    float inv = g_mix.w > 1e-6 ? 1.0 / g_mix.w : 0.0;
    return vec4(g_mix.rgb * inv, cf_clamp01(g_mix.w) * edge);
}

// rgba linear, then height (0.5 flush), roughness, metalness
void paint(int id, float x, float y, float r, out vec4 rgba, out float hgt, out float rough, out float metal) {
    rgba = vec4(0.0);
    hgt = 0.5;
    rough = 1.0;
    metal = 0.0;
    float ang = atan(y, x);
    if (id == 0) {
        float jit = n_fbm(cos(ang) * 2.6 + 3.1, sin(ang) * 2.6 - 7.4, 3) - 0.5;
        float rb = 0.185 * (1.0 + jit * 0.36);
        float bore = cf_sm(rb + 0.028, rb - 0.012, r);
        float crumb = cf_pow(n_fbm(x * 5.2 + 2.2, y * 5.2 - 1.1, 4), 1.4);
        float rim_t = (r - rb * 1.3) / (0.11 + 0.05 * (jit + 0.5));
        float rim = cf_clamp01(exp(-rim_t * rim_t * 1.4) * (0.3 + 1.25 * crumb)) * (1.0 - bore);
        float ao = contact_ao(r, rb, 0.40 + 0.12 * crumb) * (1.0 - bore);
        float crack = radial_spall(ang, r, 3.7, 4) * cf_sm(rb * 0.8, rb * 1.25, r);
        float grit = pow(cf_clamp01(1.0 - n_worley(x * 12.5 + 8.8, y * 12.5 - 3.3) * 2.1), 2.0) * cf_sm(0.85, 0.2, r);
        mix_reset();
        mix_add(bore, 0.021, 0.0195, 0.018);
        mix_add(rim * 0.92, 0.235, 0.222, 0.198);
        mix_add(grit * 0.4, 0.19, 0.18, 0.163);
        mix_add(crack * 0.8, 0.045, 0.042, 0.038);
        mix_add(ao, 0.0, 0.0, 0.0);
        rgba = mix_into(cf_sm(1.0, 0.74, r));
        hgt = 0.5 - bore * 0.5 + rim * 0.1 - crack * 0.22 - ao * 0.1;
        rough = cf_clamp01(0.99 - rim * 0.05);
    } else if (id == 1) {
        float lobes = 0.26 + 0.13 * n_fbm(cos(ang) * 1.7 + 20.5, sin(ang) * 1.7 + 6.6, 3);
        float erode = n_fbm(x * 6.6 + 14.2, y * 6.6 - 8.1, 4);
        float crater = cf_sm(lobes + 0.1, lobes - 0.08, r + (erode - 0.5) * 0.14);
        float rb = 0.105 + 0.028 * n_fbm(cos(ang) * 3.0 + 1.5, sin(ang) * 3.0 - 2.5, 2);
        float bore = cf_sm(rb + 0.03, rb - 0.014, r);
        float chips = pow(1.0 - n_worley(x * 13.5 + 8.8, y * 13.5 - 3.3), 2.6) * crater * (0.4 + 1.1 * n_fbm(x * 5.5 - 1.1, y * 5.5 + 6.2, 3));
        float crack = radial_spall(ang, r, 8.2, 5) * cf_sm(rb * 0.9, rb * 1.4, r);
        float floor_ao = crater * 0.34;
        float ao = contact_ao(r, lobes * 0.92, 0.34 + 0.14 * chips) * (1.0 - bore) * (1.0 - crater * 0.6);
        mix_reset();
        mix_add(bore, 0.020, 0.0185, 0.017);
        mix_add(crater * 0.64, 0.205 + 0.05 * chips, 0.195 + 0.045 * chips, 0.175 + 0.04 * chips);
        mix_add(crack * 0.78, 0.042, 0.039, 0.035);
        mix_add(cf_clamp01(floor_ao + ao), 0.0, 0.0, 0.0);
        rgba = mix_into(cf_sm(1.0, 0.76, r));
        hgt = 0.5 - bore * 0.5 - crater * 0.24 + chips * 0.12 - crack * 0.2;
        rough = 0.99;
    } else if (id == 2) {
        float warp = n_fbm(cos(ang) * 2.2 + 4.4, sin(ang) * 2.2 + 9.1, 3);
        float petal = 0.45 * abs(sin(ang * 2.5 + warp * 5.5)) + 0.55 * warp;
        float rh = 0.14 + 0.035 * petal;
        float hole = cf_sm(rh + 0.03, rh - 0.015, r);
        float lip_t = (r - rh * (1.35 + 0.5 * petal)) / (0.07 + 0.09 * petal);
        float grain = n_fbm(x * 9.5 + 1.3, y * 9.5 - 5.6, 3);
        float lip = exp(-lip_t * lip_t * 2.4) * (0.45 + 1.05 * grain);
        float scuff = cf_sm(0.9, 0.2, r) * n_fbm(x * 7.5 + 6.1, y * 7.5 - 2.2, 3);
        float scratch = pow(cf_clamp01(1.0 - n_worley_edge(x * 6.0 + 2.0, y * 6.0 - 8.0) * 9.0), 2.0) * cf_sm(0.85, 0.1, r);
        float ao = contact_ao(r, rh, 0.34) * (1.0 - hole);
        float a = cf_clamp01(hole + lip * 0.7 + scuff * 0.5 + scratch * 0.3 + ao) * cf_sm(1.0, 0.75, r);
        float bare = cf_clamp01(lip * 0.85 + scratch * 0.45);
        float l = hole > 0.5 ? 0.018 : cf_clamp01((0.09 + 0.24 * bare + 0.07 * scuff) * (1.0 - ao));
        rgba = vec4(l, l * 0.985, l * 0.96, a);
        hgt = 0.5 - hole * 0.5 + lip * 0.26;
        rough = cf_clamp01(0.62 - bare * 0.42);
        metal = cf_clamp01(bare * 0.85);
    } else if (id == 3) {
        float fib = n_fbm(x * 1.6 + 5.5, y * 11.0 - 2.2, 4);
        float rb = 0.16 * (1.0 + (fib - 0.5) * 0.34);
        float bore = cf_sm(rb + 0.03, rb - 0.014, r);
        float splinter = pow(cf_clamp01(1.0 - abs(sin(ang * 6.5 + fib * 5.0)) * 1.05), 3.2) * cf_sm(0.52, rb * 0.9, r) * (0.45 + 1.0 * n_fbm(x * 7.7 - 2.2, y * 7.7 + 5.5, 3));
        float lip_t = (r - rb * 1.25) / 0.1;
        float lip = cf_clamp01(exp(-lip_t * lip_t * 1.5) * (0.4 + 1.1 * fib)) * (1.0 - bore);
        float ao = contact_ao(r, rb, 0.44 + 0.1 * fib) * (1.0 - bore);
        float crack = radial_spall(ang, r, 5.9, 3) * cf_sm(rb * 0.8, rb * 1.3, r);
        mix_reset();
        mix_add(bore, 0.015, 0.0125, 0.010);
        mix_add(cf_clamp01(lip * 0.85 + splinter * 0.6), 0.125, 0.088, 0.052);
        mix_add(crack * 0.7, 0.03, 0.022, 0.014);
        mix_add(ao, 0.0, 0.0, 0.0);
        rgba = mix_into(cf_sm(1.0, 0.72, r));
        hgt = 0.5 - bore * 0.5 + splinter * 0.22 - crack * 0.18 - ao * 0.08;
        rough = cf_clamp01(0.9 - splinter * 0.1);
    } else if (id == 4) {
        float jit = n_fbm(cos(ang) * 2.2 + 14.4, sin(ang) * 2.2 + 3.3, 3) - 0.5;
        float rb = 0.195 * (1.0 + jit * 0.4);
        float bore = cf_sm(rb + 0.032, rb - 0.014, r);
        float crumb = pow(1.0 - n_worley(x * 11.5 - 3.3, y * 11.5 + 7.7), 2.4) * (0.35 + 1.2 * n_fbm(x * 4.4 + 9.9, y * 4.4 - 2.2, 3));
        float rim_t = (r - rb * 1.26) / (0.12 + 0.05 * (jit + 0.5));
        float rim = cf_clamp01(exp(-rim_t * rim_t * 1.35) * (0.32 + 1.2 * crumb)) * (1.0 - bore);
        float powder = cf_sm(0.9, 0.18, r) * n_fbm(x * 3.1 + 8.2, y * 3.1 - 4.4, 4) * 0.3;
        float ao = contact_ao(r, rb, 0.40 + 0.12 * crumb) * (1.0 - bore);
        float crack = radial_spall(ang, r, 1.9, 4) * cf_sm(rb * 0.8, rb * 1.25, r);
        mix_reset();
        mix_add(bore, 0.018, 0.0165, 0.015);
        mix_add(rim * 0.94, 0.305, 0.278, 0.234);
        mix_add(powder, 0.26, 0.238, 0.20);
        mix_add(crack * 0.8, 0.04, 0.036, 0.03);
        mix_add(ao, 0.0, 0.0, 0.0);
        rgba = mix_into(cf_sm(1.0, 0.75, r));
        hgt = 0.5 - bore * 0.5 + rim * 0.12 - crack * 0.2 - ao * 0.1;
        rough = 1.0;
    } else if (id == 5) {
        float radial = pow(cf_clamp01(1.0 - abs(sin(ang * 5.5 + n_fbm(r * 3.0 + 2.0, ang + 4.0, 3) * 6.0)) * 1.6), 6.0);
        float conc = pow(cf_clamp01(1.0 - abs(sin(r * 17.0 + n_fbm(x * 2.0, y * 2.0, 2) * 4.0)) * 1.9), 5.0);
        float web = cf_clamp01(radial * cf_sm(0.98, 0.05, r) + conc * cf_sm(0.95, 0.12, r) * 0.7);
        float rh = 0.055;
        float hole = cf_sm(rh + 0.02, rh - 0.01, r);
        float shatter = pow(cf_clamp01(1.0 - n_worley_edge(x * 4.4 + 1.1, y * 4.4 - 6.6) * 8.0), 3.0) * cf_sm(0.7, 0.1, r);
        float a = cf_clamp01(web * 0.95 + hole + shatter * 0.8);
        float l = hole > 0.5 ? 0.02 : cf_clamp01(0.5 + 0.45 * web);
        rgba = vec4(l * 0.94, l * 0.98, l, a);
        hgt = 0.5 - hole * 0.4 + web * 0.2;
        rough = cf_clamp01(0.42 - web * 0.3);
    } else if (id == 6) {
        float edge = 0.44 + 0.3 * n_fbm(cos(ang) * 2.1 + 6.5, sin(ang) * 2.1 - 9.9, 3);
        float wob = n_fbm(x * 5.5 - 3.3, y * 5.5 + 1.7, 4);
        float a = cf_sm(edge, edge - 0.16, r + (wob - 0.5) * 0.16);
        float s = n_worley(x * 4.2 + 3.7, y * 4.2 - 5.5);
        a = cf_clamp01(a + pow(cf_clamp01(1.0 - s * 3.4), 5.0) * cf_sm(1.0, 0.42, r) * 0.95);
        float thick = a * (0.25 + 1.35 * pow(n_warped(x * 4.4 - 2.2, y * 4.4 + 4.4, 0.7, 4), 1.5));
        float rim = cf_sm(edge - 0.16, edge, r);
        rgba = vec4(cf_clamp01(0.075 + 0.075 * thick - rim * 0.035), cf_clamp01(0.009 + 0.017 * thick), cf_clamp01(0.008 + 0.014 * thick), a);
        hgt = 0.5 + thick * 0.12;
        rough = cf_clamp01(0.5 - thick * 0.26 + rim * 0.24);
    } else if (id == 7) {
        float edge = 0.4 + 0.22 * n_fbm(cos(ang) * 1.8 + 16.5, sin(ang) * 1.8 + 2.2, 3);
        float a = cf_sm(edge, edge - 0.12, length(vec2(x, (y + 0.12) * 1.18)));
        float lane = n_fbm(x * 9.5 + 1.7, 3.3, 2);
        float in_lane = pow(cf_clamp01(1.0 - abs(sin(x * 11.5 + lane * 4.0)) * 1.5), 5.0);
        float under = cf_sm(edge, edge * 0.25, abs(x));
        float y0 = -edge * 0.8;
        float run_len = (0.3 + 0.55 * lane) * under;
        float t = run_len > 0.02 ? (y0 - y) / run_len : -1.0;
        float run = (t > 0.0 && t < 1.0) ? in_lane * under * (1.0 - t * 0.65) * cf_sm(0.0, 0.06, t) * cf_sm(1.0, 0.86, t) : 0.0;
        a = cf_clamp01(a + run * 0.9);
        float bead = exp(-(pow((x - (lane - 0.5) * 0.12) * 26.0, 2.0) + pow((y - (y0 - run_len)) * 22.0, 2.0)));
        a = cf_clamp01(a + bead * in_lane * under * 0.85);
        float thick = a * (0.5 + 0.8 * n_fbm(x * 4.4 + 7.7, y * 4.4 - 1.1, 3));
        rgba = vec4(cf_clamp01(0.07 + 0.08 * thick), cf_clamp01(0.008 + 0.016 * thick), cf_clamp01(0.007 + 0.013 * thick), a);
        hgt = 0.5 + thick * 0.1;
        rough = cf_clamp01(0.46 - thick * 0.24);
    } else if (id == 8) {
        float streak = 0.55 + 0.55 * n_fbm(cos(ang) * 3.3 + 4.4, sin(ang) * 3.3 - 8.8, 4);
        float body = cf_sm(0.98 * streak, 0.05, r);
        float soot = n_fbm(x * 3.4 + 2.4, y * 3.4 + 12.2, 5);
        float a = cf_clamp01(body * (0.45 + 0.85 * soot));
        float l = cf_clamp01(0.035 + 0.07 * (1.0 - body) + 0.03 * soot);
        rgba = vec4(l, l * 0.96, l * 0.92, a * 0.92);
        hgt = 0.5 - body * 0.04;
        rough = 1.0;
    } else if (id == 9) {
        float rh = 0.26 + 0.09 * (n_fbm(cos(ang) * 1.9 + 9.1, sin(ang) * 1.9 - 3.7, 3) - 0.5);
        float crater = cf_sm(rh + 0.09, rh - 0.05, r);
        float collar = exp(-pow((r - rh * 1.5) / 0.24, 2.0) * 1.7);
        float clods = pow(1.0 - n_worley(x * 3.9 + 5.2, y * 3.9 + 1.4), 2.4);
        float a = cf_clamp01(crater + collar * 0.85 * (0.4 + clods) + cf_sm(1.0, 0.3, r) * 0.3);
        float l = cf_clamp01(0.045 + 0.06 * (1.0 - crater) + 0.07 * collar * clods);
        rgba = vec4(l, l * 0.82, l * 0.62, a);
        hgt = 0.5 - crater * 0.4 + collar * clods * 0.2;
        rough = 0.96;
    } else if (id == 10) {
        float rh = 0.3 + 0.07 * (n_fbm(cos(ang) * 2.4 + 1.1, sin(ang) * 2.4 + 8.3, 3) - 0.5);
        float crater = cf_sm(rh + 0.13, rh - 0.06, r);
        float collar = exp(-pow((r - rh * 1.42) / 0.26, 2.0) * 1.5);
        float grain = n_fbm(x * 11.0 + 3.3, y * 11.0 - 6.6, 3);
        float rays = pow(cf_clamp01(1.0 - abs(sin(ang * 4.5 + n_fbm(cos(ang) * 2.0, sin(ang) * 2.0, 3) * 6.0)) * 1.15), 2.5) * collar;
        float a = cf_clamp01(crater * 0.8 + collar * 0.45 + rays * 0.45 + cf_sm(1.0, 0.3, r) * 0.3);
        float l = cf_clamp01(0.3 + 0.1 * collar + 0.1 * rays - 0.16 * crater + 0.08 * grain);
        rgba = vec4(l, l * 0.9, l * 0.72, a);
        hgt = 0.5 - crater * 0.3 + collar * 0.12;
        rough = 1.0;
    } else if (id == 11) {
        float wander = (n_fbm(x * 2.6 + 2.2, 1.7, 3) - 0.5) * 0.22;
        float taper = cf_sm(-0.95, -0.55, x) * cf_sm(1.0, 0.35, x);
        float w = 0.19 * taper * (0.55 + 0.8 * n_fbm(x * 4.4 - 6.6, 3.3, 3));
        float d = abs(y - wander) / max(0.02, w);
        float gouge = cf_sm(1.15, 0.15, d) * taper;
        float striae = pow(cf_clamp01(1.0 - abs(sin(y * 34.0 + n_fbm(x * 3.2, 5.5, 2) * 4.0)) * 1.25), 4.0) * gouge * (0.3 + 1.2 * n_fbm(x * 9.0 + 1.1, y * 2.0, 3));
        float a = cf_clamp01(gouge * 0.85 + striae * 0.4);
        float l = cf_clamp01(0.18 + 0.34 * striae + 0.16 * gouge);
        rgba = vec4(l, l * 0.98, l * 0.95, a);
        hgt = 0.5 - gouge * 0.22 + striae * 0.1;
        rough = cf_clamp01(0.55 - striae * 0.35);
        metal = cf_clamp01(gouge * 0.85);
    } else if (id == 12) {
        float wob = 1.0 + 0.06 * (n_fbm(x * 3.0 + 6.0, y * 3.0 - 2.0, 3) - 0.5);
        float rings = sin((r * wob) * 22.0) * exp(-r * r * 2.6);
        float a = cf_clamp01(exp(-r * r * 2.2) * 0.6) * cf_sm(1.0, 0.85, r);
        rgba = vec4(0.42, 0.46, 0.5, a);
        hgt = 0.5 + rings * 0.28;
        rough = 0.06;
    } else if (id == 13) {
        float rad = pow(cf_clamp01(1.0 - abs(sin(ang * 4.5 + n_fbm(r * 4.0 + 1.0, ang * 2.0, 3) * 5.0)) * 1.5), 7.0);
        float web = rad * cf_sm(0.72, 0.04, r);
        float hole = cf_sm(0.08, 0.05, r);
        float frost = pow(1.0 - n_worley(x * 7.0 + 2.2, y * 7.0 - 3.3), 3.0) * cf_sm(0.34, 0.04, r);
        float a = cf_clamp01(web * 0.9 + hole + frost * 0.85);
        float l = hole > 0.5 ? 0.02 : cf_clamp01(0.55 + 0.4 * (web + frost));
        rgba = vec4(l * 0.95, l * 0.99, l, a);
        hgt = 0.5 - hole * 0.4 + (web + frost) * 0.18;
        rough = cf_clamp01(0.36 - frost * 0.24);
    } else if (id == 14) {
        float w = n_warped(x * 2.2 + 18.8, y * 2.2 - 5.5, 0.8, 5);
        float a = cf_clamp01(cf_sm(0.05, 0.75, (1.0 - r) + (w - 0.5) * 0.9)) * 0.7;
        float l = cf_clamp01(0.22 + 0.22 * w);
        rgba = vec4(l, l * 0.96, l * 0.9, a);
        hgt = 0.5;
        rough = 1.0;
    } else {
        float rip = pow(cf_clamp01(1.0 - abs(sin(ang * 2.5 + n_fbm(cos(ang) * 2.0 + 3.0, sin(ang) * 2.0, 2) * 4.0)) * 1.2), 3.0);
        float rh = 0.1 + 0.14 * rip;
        float hole = cf_sm(rh + 0.05, rh - 0.02, r);
        float fray = pow(cf_clamp01(1.0 - n_worley_edge(x * 8.0 + 4.4, y * 8.0 - 1.1) * 9.0), 2.0) * cf_sm(0.5, 0.1, r);
        float a = cf_clamp01(hole + fray * 0.9);
        float l = hole > 0.5 ? 0.02 : cf_clamp01(0.18 + 0.3 * fray);
        rgba = vec4(l, l * 0.94, l * 0.88, a);
        hgt = 0.5 - hole * 0.45 + fray * 0.12;
        rough = 0.98;
    }
}

// a tile point: x right, y up the decal (the first texture row is the decal's top)
void tile_point(vec2 px, out float x, out float y) {
    x = (px.x + 0.5) / params.cell_px * 2.0 - 1.0;
    y = 1.0 - (px.y + 0.5) / params.cell_px * 2.0;
}

void main() {
    vec2 frag = v_uv * (params.cols * params.cell_px);
    vec2 cell = floor(frag / params.cell_px);
    vec2 px = floor(frag - cell * params.cell_px);
    int id = int(cell.y * params.cols + cell.x);
    g_salt = uint(params.seed) * 2654435761u ^ (uint(id) * 0x9E3779B9u + 0x85EBCA6Bu);

    float x, y;
    tile_point(px, x, y);
    float r = length(vec2(x, y));
    vec4 c = vec4(0.0);
    float h = 0.5, ro = 1.0, me = 0.0;
    if (r < 1.45) paint(id, x, y, r, c, h, ro, me);
    float gutter = (px.x < 1.0 || px.y < 1.0 || px.x > params.cell_px - 2.0 || px.y > params.cell_px - 2.0) ? 0.0 : 1.0;
    out_albedo = vec4(c.rgb, cf_clamp01(c.a) * gutter);
    out_field = vec4(h, ro, me, 1.0);
}
@end
