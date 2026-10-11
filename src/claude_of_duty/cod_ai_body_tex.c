// Claude of Duty soldier bodies: the runtime texture bakes: camouflage cloth, cordura, plate laminate, skin, polymer, steel, rubber and the two detail tiles.

// ----------------------------------------------------------- tile noise ---

void cab_tile_noise_init(CabTileNoise *n, CodRng *rng, Allocator *alloc) {
  n->tab = ALLOC_ARRAY_NO_ZERO(alloc, f32, 4096);
  n->perm = ALLOC_ARRAY_NO_ZERO(alloc, u16, 4096);
  for (u32 i = 0; i < 4096; i++)
    n->tab[i] = cod_rng_float(rng);
  for (u32 i = 0; i < 4096; i++)
    n->perm[i] = (u16)cod_rng_int(rng, 0, 4095);
}

hz_internal f32 cab_tn_h(const CabTileNoise *n, i32 ix, i32 iy, i32 p) {
  i32 x = ((ix % p) + p) % p;
  i32 y = ((iy % p) + p) % p;
  return n->tab[(n->perm[(x * 73 + y * 151) & 4095] + x * 31 + y * 17) & 4095];
}

// value noise on a lattice of `period` cells over the unit tile
hz_internal f32 cab_n2(const CabTileNoise *n, f32 u, f32 v, i32 period) {
  f32 x = u * (f32)period, y = v * (f32)period;
  f32 fx0 = m_floorf(x), fy0 = m_floorf(y);
  i32 ix = (i32)fx0, iy = (i32)fy0;
  f32 fx = x - fx0, fy = y - fy0;
  f32 sx = fx * fx * (3.0f - 2.0f * fx);
  f32 sy = fy * fy * (3.0f - 2.0f * fy);
  f32 a = cab_tn_h(n, ix, iy, period), b = cab_tn_h(n, ix + 1, iy, period);
  f32 c = cab_tn_h(n, ix, iy + 1, period), d = cab_tn_h(n, ix + 1, iy + 1, period);
  return (a + (b - a) * sx) * (1.0f - sy) + (c + (d - c) * sx) * sy;
}

hz_internal f32 cab_fbm2(const CabTileNoise *n, f32 u, f32 v, i32 period, u32 oct, f32 gain) {
  f32 a = 1.0f, s = 0.0f, norm = 0.0f;
  i32 p = period;
  for (u32 i = 0; i < oct; i++) {
    s += a * cab_n2(n, u, v, p);
    norm += a;
    a *= gain;
    p *= 2;
  }
  return s / norm;
}

hz_internal f32 cab_fbm(const CabTileNoise *n, f32 u, f32 v, i32 period, u32 oct) { return cab_fbm2(n, u, v, period, oct, 0.5f); }

// ridged noise for fibre and scratch structure
hz_internal f32 cab_ridge(const CabTileNoise *n, f32 u, f32 v, i32 period, u32 oct) {
  f32 a = 1.0f, s = 0.0f, norm = 0.0f;
  i32 p = period;
  for (u32 i = 0; i < oct; i++) {
    s += a * (1.0f - m_fabsf(cab_n2(n, u, v, p) * 2.0f - 1.0f));
    norm += a;
    a *= 0.55f;
    p *= 2;
  }
  return s / norm;
}

// ------------------------------------------------------------- helpers ---

hz_internal f32 cab_smooth(f32 e0, f32 e1, f32 x) {
  f32 t = m_minf(1.0f, m_maxf(0.0f, (x - e0) / (e1 - e0)));
  return t * t * (3.0f - 2.0f * t);
}

hz_internal f32 cab_mix(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

hz_internal v3 cab_mix3(v3 a, v3 b, f32 t) { return (v3){cab_mix(a.x, b.x, t), cab_mix(a.y, b.y, t), cab_mix(a.z, b.z, t)}; }

// distance from the centre of a repeating cell, in cell units
hz_internal f32 cab_cell_dist(f32 x) { return m_fabsf((x - m_floorf(x)) - 0.5f); }

hz_internal f32 cab_ridge_line(f32 d, f32 w) { return cab_smooth(w, 0.0f, d); }

hz_internal f32 cab_lum3(f32 r, f32 g, f32 b) { return 0.2126f * r + 0.7152f * g + 0.0722f * b; }

hz_internal f32 cab_srgb255(f32 v) {
  f32 c = v <= 0.0f ? 0.0f : v >= 1.0f ? 1.0f : v;
  return (c <= 0.0031308f ? c * 12.92f : 1.055f * m_powf(c, 1.0f / 2.4f) - 0.055f) * 255.0f;
}

// ---------------------------------------------------------------- camo ---

typedef struct CabCamo {
  f32 budget;
  v3 pale, base, mid, dark, olive;
  i32 macro;
  f32 warp;
} CabCamo;

hz_internal const CabCamo cab_camo[3] = {
    {.budget = 0.104f, .pale = {0.382f, 0.318f, 0.212f}, .base = {0.320f, 0.256f, 0.163f}, .mid = {0.241f, 0.186f, 0.116f},
     .dark = {0.146f, 0.111f, 0.072f}, .olive = {0.176f, 0.190f, 0.116f}, .macro = 2, .warp = 0.15f},
    {.budget = 0.092f, .pale = {0.354f, 0.340f, 0.230f}, .base = {0.246f, 0.259f, 0.170f}, .mid = {0.174f, 0.190f, 0.126f},
     .dark = {0.104f, 0.110f, 0.083f}, .olive = {0.210f, 0.196f, 0.132f}, .macro = 3, .warp = 0.17f},
    {.budget = 0.083f, .pale = {0.330f, 0.334f, 0.342f}, .base = {0.226f, 0.230f, 0.239f}, .mid = {0.150f, 0.154f, 0.163f},
     .dark = {0.078f, 0.079f, 0.088f}, .olive = {0.190f, 0.188f, 0.182f}, .macro = 2, .warp = 0.14f},
};

// CLOTH_BUDGET: the mean is forced onto `mean`, the macro spread stretched, every texel clamped, the hue pushed off neutral
#define CAB_BUDGET_MIN 0.040f
#define CAB_BUDGET_MAX 0.152f
#define CAB_BUDGET_CONTRAST 1.5f
#define CAB_BUDGET_SAT 1.35f
#define CAB_KIT_CAL 0.51f

// garment-scale relief for the base cloth tile: felled seams, stitch beads, pocket creases, wrinkles
hz_internal f32 cab_garment_relief(const CabTileNoise *nz, f32 u, f32 v) {
  f32 drift = (cab_fbm(nz, u, v, 3, 2) - 0.5f) * 0.22f;
  f32 sa = cab_cell_dist(v * 4.0f + drift);
  f32 h = -cab_ridge_line(sa, 0.013f) * 0.62f + (cab_ridge_line(sa, 0.030f) - cab_ridge_line(sa, 0.016f)) * 0.34f;
  f32 drift2 = (cab_fbm(nz, v + 4.1f, u, 3, 2) - 0.5f) * 0.26f;
  f32 sb = cab_cell_dist(u * 2.5f + drift2);
  h += -cab_ridge_line(sb, 0.009f) * 0.46f + (cab_ridge_line(sb, 0.022f) - cab_ridge_line(sb, 0.011f)) * 0.22f;
  f32 on_seam = m_maxf(cab_ridge_line(sa, 0.020f), cab_ridge_line(sb, 0.014f));
  h += on_seam * (0.5f + 0.5f * m_sinf((u + v) * 520.0f)) * 0.26f;
  f32 gate = cab_smooth(0.55f, 0.72f, cab_fbm(nz, u + 1.7f, v + 2.3f, 3, 2));
  f32 pu = cab_cell_dist(u * 3.5f + 0.31f);
  f32 pv = cab_cell_dist(v * 3.0f + 0.17f);
  h -= gate * m_maxf(cab_ridge_line(pu, 0.012f), cab_ridge_line(pv, 0.014f)) * 0.55f;
  h += (cab_fbm(nz, u, v, 10, 3) - 0.5f) * 0.95f;
  h += (cab_fbm(nz, u + 5.3f, v + 1.9f, 26, 2) - 0.5f) * 0.34f;
  f32 crease = cab_ridge(nz, u + 3.1f, v - 2.2f, 52, 2);
  h += (crease - 0.55f) * 0.46f;
  h += (cab_ridge(nz, v * 0.7f + 8.4f, u * 0.7f + 1.1f, 74, 2) - 0.55f) * 0.22f;
  return h;
}

hz_internal void cab_camo_texel(const CabTileNoise *nz, const CabCamo *cfg, f32 u, f32 v, CabTexel *out) {
  i32 M = cfg->macro;
  f32 wx = cab_fbm(nz, u + 0.31f, v + 0.17f, M * 2, 2) - 0.5f;
  f32 wy = cab_fbm(nz, u + 0.73f, v + 0.59f, M * 2, 2) - 0.5f;
  f32 mu = u + wx * cfg->warp;
  f32 mv = v + wy * cfg->warp;
  f32 a = cab_fbm2(nz, mu + 0.11f, mv, M, 2, 0.40f);
  f32 b = cab_fbm2(nz, mu, mv + 0.37f, M, 2, 0.40f);
  f32 c = cab_fbm2(nz, mu + 0.61f, mv + 0.23f, M + 1, 2, 0.44f);
  f32 d = cab_fbm2(nz, mu + 0.29f, mv + 0.83f, M + 2, 2, 0.44f);
  v3 col = cfg->base;
  col = cab_mix3(col, cfg->pale, cab_smooth(0.535f, 0.585f, a));
  col = cab_mix3(col, cfg->olive, cab_smooth(0.555f, 0.605f, b) * 0.9f);
  col = cab_mix3(col, cfg->mid, cab_smooth(0.515f, 0.565f, c));
  col = cab_mix3(col, cfg->dark, cab_smooth(0.605f, 0.655f, d));
  f32 f1 = cab_smooth(0.40f, 0.60f, cab_fbm2(nz, u + 3.7f, v + 1.3f, 24, 2, 0.35f));
  f32 f2 = cab_smooth(0.52f, 0.70f, cab_n2(nz, u + 7.1f, v + 2.9f, 48));
  f32 fine = 0.88f + 0.26f * f1 - 0.12f * f2;
  f32 h = cab_garment_relief(nz, u, v);
  out->h = h;
  f32 bleach = 1.0f + 0.05f * cab_smooth(-0.2f, 0.9f, h);
  out->r = col.x * fine * bleach;
  out->g = col.y * fine * bleach;
  out->b = col.z * fine * bleach * 0.99f;
  out->rough = 0.905f - 0.045f * cab_smooth(-0.6f, 0.8f, h) + 0.035f * (cab_fbm(nz, u, v, 9, 3) - 0.5f);
  out->metal = 0.0f;
  out->ao = 0.82f + 0.18f * cab_smooth(-0.7f, 0.7f, h);
}

hz_internal void cab_apply_budget(CabTexel *out, f32 src_mean, f32 budget_mean) {
  f32 l = cab_lum3(out->r, out->g, out->b);
  if (l < 1e-6f)
    return;
  f32 t = budget_mean + (l - src_mean) * CAB_BUDGET_CONTRAST;
  t = t < CAB_BUDGET_MIN ? CAB_BUDGET_MIN : t > CAB_BUDGET_MAX ? CAB_BUDGET_MAX : t;
  f32 k = t / l;
  f32 r = l + (out->r - l) * CAB_BUDGET_SAT;
  f32 g = l + (out->g - l) * CAB_BUDGET_SAT;
  f32 b = l + (out->b - l) * CAB_BUDGET_SAT;
  f32 l2 = m_maxf(1e-6f, cab_lum3(r, g, b));
  f32 k2 = (l * k) / l2;
  out->r = r * k2;
  out->g = g * k2;
  out->b = b * k2;
}

// ---------------------------------------------------------- the other sets ---

hz_internal void cab_nylon_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 tu = u * 26.0f, tv = v * 26.0f;
  f32 cell = m_sinf(tu * (f32)M_PI) * m_sinf(tv * (f32)M_PI);
  f32 h = cell * 0.34f;
  h += (cab_fbm(nz, u, v, 120, 2) - 0.5f) * 0.30f;
  f32 rib = cab_cell_dist(v * 44.0f);
  f32 rib_gate = cab_smooth(0.52f, 0.70f, cab_fbm(nz, u + 8.3f, v, 5, 2));
  h += rib_gate * (cab_ridge_line(rib, 0.30f) - 0.5f) * 0.30f;
  f32 st = cab_cell_dist(v * 3.0f);
  f32 tape = cab_ridge_line(st, 0.045f);
  h += tape * 0.14f;
  h += cab_ridge_line(st, 0.020f) * (0.5f + 0.5f * m_sinf(u * 300.0f)) * 0.30f;
  out->h = h;
  f32 shade = 0.84f + 0.16f * cab_fbm(nz, u, v, 7, 3);
  f32 base = 0.300f * CAB_KIT_CAL * shade;
  out->r = base * 1.05f;
  out->g = base * 1.0f;
  out->b = base * 0.90f;
  f32 thr = cab_ridge_line(st, 0.020f) * (0.25f + 0.25f * m_sinf(u * 300.0f));
  out->r = cab_mix(out->r, 0.335f * CAB_KIT_CAL, thr);
  out->g = cab_mix(out->g, 0.320f * CAB_KIT_CAL, thr);
  out->b = cab_mix(out->b, 0.278f * CAB_KIT_CAL, thr);
  out->rough = 0.79f - 0.13f * cab_smooth(-0.4f, 0.9f, h) + 0.05f * (cab_fbm(nz, u + 2.0f, v, 11, 2) - 0.5f);
  out->metal = 0.0f;
  out->ao = 0.80f + 0.20f * cab_smooth(-0.5f, 1.0f, h);
}

hz_internal void cab_plate_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 qu = cab_cell_dist(u * 5.0f + v * 2.5f);
  f32 qv = cab_cell_dist(u * -2.5f + v * 5.0f);
  f32 h = -(cab_ridge_line(qu, 0.045f) + cab_ridge_line(qv, 0.045f)) * 0.42f;
  h += (1.0f - m_maxf(cab_ridge_line(qu, 0.30f), cab_ridge_line(qv, 0.30f))) * 0.26f;
  f32 grain = cab_fbm(nz, u, v, 90, 3);
  h += (grain - 0.5f) * 0.24f;
  f32 scuff = cab_smooth(0.62f, 0.86f, cab_ridge(nz, u * 0.7f, v * 2.4f, 22, 3));
  h -= scuff * 0.18f;
  out->h = h;
  f32 fade = cab_fbm(nz, u + 3.3f, v, 3, 3);
  f32 soil = cab_fbm(nz, u + 7.7f, v + 2.1f, 8, 3);
  f32 shade = 0.74f + 0.40f * fade;
  f32 base = 0.212f * CAB_KIT_CAL * shade;
  out->r = base * 1.04f;
  out->g = base * 1.0f;
  out->b = base * 0.93f;
  out->r = cab_mix(out->r, out->r * 0.66f, cab_smooth(0.44f, 0.68f, soil));
  out->g = cab_mix(out->g, out->g * 0.64f, cab_smooth(0.44f, 0.68f, soil));
  out->b = cab_mix(out->b, out->b * 0.60f, cab_smooth(0.44f, 0.68f, soil));
  out->r = cab_mix(out->r, 0.283f * CAB_KIT_CAL, scuff * 0.7f);
  out->g = cab_mix(out->g, 0.274f * CAB_KIT_CAL, scuff * 0.7f);
  out->b = cab_mix(out->b, 0.258f * CAB_KIT_CAL, scuff * 0.7f);
  out->rough = 0.590f + 0.060f * cab_smooth(-0.5f, 0.7f, -h) + 0.09f * scuff + 0.05f * cab_smooth(0.44f, 0.68f, soil) +
               0.025f * (cab_fbm(nz, u, v + 5.1f, 13, 2) - 0.5f);
  out->metal = 0.0f;
  out->ao = 0.82f + 0.18f * cab_smooth(-0.7f, 0.7f, h);
}

hz_internal void cab_skin_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 pores = cab_fbm(nz, u, v, 150, 3);
  f32 macro = cab_fbm(nz, u, v, 11, 3);
  f32 fine = cab_fbm(nz, u, v, 320, 2);
  out->h = (pores - 0.5f) * 0.5f + (fine - 0.5f) * 0.25f;
  v3 base = {0.295f, 0.199f, 0.148f};
  v3 flush = {0.330f, 0.186f, 0.142f};
  v3 col = cab_mix3(base, flush, cab_smooth(0.4f, 0.75f, macro));
  f32 st = cab_fbm(nz, u * 1.3f, v * 1.3f, 110, 2);
  col = cab_mix3(col, (v3){0.115f, 0.086f, 0.074f}, cab_smooth(0.62f, 0.72f, st) * 0.5f);
  out->r = col.x;
  out->g = col.y;
  out->b = col.z;
  out->rough = 0.50f + 0.16f * macro - 0.10f * pores;
  out->metal = 0.0f;
  out->ao = 0.9f + 0.1f * pores;
}

hz_internal void cab_polymer_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 stip = cab_fbm(nz, u, v, 128, 3);
  f32 peb = cab_smooth(0.45f, 0.62f, cab_fbm(nz, u, v, 64, 2));
  out->h = (stip - 0.5f) * 0.6f + peb * 0.35f;
  f32 scr = cab_smooth(0.86f, 1.0f, cab_ridge(nz, u * 0.6f, v * 3.0f, 26, 2));
  f32 v0 = 0.052f * (0.9f + 0.2f * cab_fbm(nz, u, v, 8, 2));
  out->r = cab_mix(v0 * 1.02f, 0.20f, scr * 0.5f);
  out->g = cab_mix(v0, 0.195f, scr * 0.5f);
  out->b = cab_mix(v0 * 0.98f, 0.19f, scr * 0.5f);
  out->rough = 0.55f - 0.18f * peb + 0.10f * stip - 0.25f * scr;
  out->metal = 0.0f;
  out->ao = 0.88f + 0.12f * stip;
}

hz_internal void cab_steel_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 grain = cab_fbm(nz, u * 0.25f, v * 3.0f, 90, 3);
  f32 scratch = cab_smooth(0.80f, 1.0f, cab_ridge(nz, u * 0.3f, v * 6.0f, 40, 3));
  f32 pits = cab_smooth(0.72f, 0.9f, cab_fbm(nz, u, v, 190, 2));
  out->h = (grain - 0.5f) * 0.35f + scratch * 0.5f - pits * 0.45f;
  f32 base = 0.055f + 0.02f * grain;
  f32 bare = scratch * 0.85f;
  out->r = cab_mix(base, 0.52f, bare);
  out->g = cab_mix(base, 0.53f, bare);
  out->b = cab_mix(base * 1.02f, 0.55f, bare);
  out->rough = cab_mix(0.46f + 0.14f * grain + 0.2f * pits, 0.20f, bare);
  out->metal = 1.0f;
  out->ao = 0.9f + 0.1f * grain - 0.2f * pits;
}

hz_internal void cab_rubber_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  f32 lug = m_maxf(cab_ridge_line(cab_cell_dist(u * 9.0f), 0.085f), cab_ridge_line(cab_cell_dist(v * 5.5f), 0.095f));
  f32 grain = cab_fbm(nz, u, v, 160, 3);
  out->h = -lug * 1.1f + (grain - 0.5f) * 0.4f;
  f32 c = 0.036f + 0.016f * grain;
  out->r = c;
  out->g = c * 0.99f;
  out->b = c * 0.97f;
  out->rough = 0.82f - 0.1f * grain + 0.08f * lug;
  out->metal = 0.0f;
  out->ao = 0.72f + 0.28f * (1.0f - lug);
}

// detail tiles: out->h and out->rough are the only outputs
hz_internal void cab_detail_cloth_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  const f32 threads = 33.0f;
  f32 tu = u * threads, tv = v * threads;
  f32 wu = m_sinf(tu * (f32)M_PI * 2.0f);
  f32 wv = m_sinf(tv * (f32)M_PI * 2.0f);
  b32 over = m_sinf((tu + tv) * (f32)M_PI) > 0.0f;
  f32 h = (over ? wu * 0.62f + wv * 0.22f : wv * 0.62f + wu * 0.22f) * 0.5f;
  h += (cab_ridge_line(cab_cell_dist(u * 6.0f), 0.055f) + cab_ridge_line(cab_cell_dist(v * 6.0f), 0.055f)) * 0.30f;
  h += (cab_fbm(nz, u, v, 160, 2) - 0.5f) * 0.26f;
  out->h = h;
  out->rough = 0.32f * h - 0.18f * (cab_fbm(nz, u + 2.7f, v, 90, 2) - 0.5f);
}

hz_internal void cab_detail_nylon_texel(const CabTileNoise *nz, f32 u, f32 v, CabTexel *out) {
  const f32 cells = 25.0f;
  f32 cu = m_fabsf((u * cells - m_floorf(u * cells)) - 0.5f);
  f32 cv = m_fabsf((v * cells - m_floorf(v * cells)) - 0.5f);
  b32 over = m_sinf((u * cells + v * cells) * (f32)M_PI) > 0.0f;
  f32 h = (over ? cab_smooth(0.5f, 0.1f, cu) : cab_smooth(0.5f, 0.1f, cv)) * 0.7f - 0.25f;
  h += (cab_fbm(nz, u, v, 140, 2) - 0.5f) * 0.22f;
  out->h = h;
  out->rough = -0.42f * h + 0.10f * (cab_fbm(nz, u + 4.1f, v, 70, 2) - 0.5f);
}

// one texel of a bake: u,v in [0,1)
hz_internal void cab_bake_texel(const CabTexState *t, u32 bake, f32 u, f32 v, CabTexel *out) {
  const CabTileNoise *nz = &t->noise;
  out->r = out->g = out->b = 0.5f;
  out->h = 0.0f;
  out->rough = 0.7f;
  out->metal = 0.0f;
  out->ao = 1.0f;
  switch (bake) {
  case CAB_SET_CAMO_ARID:
  case CAB_SET_CAMO_WOODLAND:
  case CAB_SET_CAMO_URBAN:
    cab_camo_texel(nz, &cab_camo[bake], u, v, out);
    cab_apply_budget(out, t->camo_src_mean[bake], cab_camo[bake].budget);
    break;
  case CAB_SET_NYLON: cab_nylon_texel(nz, u, v, out); break;
  case CAB_SET_PLATE: cab_plate_texel(nz, u, v, out); break;
  case CAB_SET_SKIN: cab_skin_texel(nz, u, v, out); break;
  case CAB_SET_POLYMER: cab_polymer_texel(nz, u, v, out); break;
  case CAB_SET_STEEL: cab_steel_texel(nz, u, v, out); break;
  case CAB_SET_RUBBER: cab_rubber_texel(nz, u, v, out); break;
  case CAB_BAKE_DETAIL_CLOTH:
    out->h = 0.0f;
    out->rough = 0.0f;
    cab_detail_cloth_texel(nz, u, v, out);
    break;
  default:
    out->h = 0.0f;
    out->rough = 0.0f;
    cab_detail_nylon_texel(nz, u, v, out);
    break;
  }
}

// Sobel normal strength of each bake
hz_internal f32 cab_bake_normal_scale(u32 bake) {
  switch (bake) {
  case CAB_SET_CAMO_ARID:
  case CAB_SET_CAMO_WOODLAND:
  case CAB_SET_CAMO_URBAN: return 0.9f;
  case CAB_SET_NYLON: return 1.15f;
  case CAB_SET_PLATE: return 1.05f;
  case CAB_SET_SKIN: return 0.75f;
  case CAB_SET_POLYMER: return 1.0f;
  case CAB_SET_STEEL: return 1.1f;
  case CAB_SET_RUBBER: return 1.4f;
  case CAB_BAKE_DETAIL_CLOTH: return 1.05f;
  default: return 1.15f;
  }
}

// ---------------------------------------------------------------- stages ---

#define CAB_MIP_LEVELS 10

// byte offset of a mip level in a map's packed chain (RGBA8, base CAB_TEX_SIZE)
hz_internal u64 cab_mip_offset(u32 level) {
  u64 off = 0;
  for (u32 l = 0; l < level; l++) {
    u64 s = CAB_TEX_SIZE >> l;
    off += s * s * 4u;
  }
  return off;
}

hz_internal u64 cab_chain_bytes(void) { return cab_mip_offset(CAB_MIP_LEVELS); }

// the camo mean the budget remap is centred on, from a coarse grid: one row of it
hz_internal f32 cab_camo_row_mean(const CabTexState *t, u32 camo, u32 row) {
  const u32 n = 96;
  CabTexel texel;
  f32 s = 0.0f;
  for (u32 x = 0; x < n; x++) {
    texel = (CabTexel){0.5f, 0.5f, 0.5f, 0.0f, 0.7f, 0.0f, 1.0f};
    cab_camo_texel(&t->noise, &cab_camo[camo], (f32)x / (f32)n, (f32)row / (f32)n, &texel);
    s += cab_lum3(texel.r, texel.g, texel.b);
  }
  return s;
}

// pass 1 row: every texel of one row into the base maps and the height field
hz_internal void cab_bake_row(CabTexState *t, u32 bake, u32 y) {
  CabBake *b = &t->bake[bake];
  const f32 inv = 1.0f / (f32)CAB_TEX_SIZE;
  const b32 detail = bake >= CAB_SET_COUNT;
  CabTexel texel;
  for (u32 x = 0; x < CAB_TEX_SIZE; x++) {
    u32 i = y * CAB_TEX_SIZE + x;
    cab_bake_texel(t, bake, (f32)x * inv, (f32)y * inv, &texel);
    b->height[i] = texel.h;
    if (detail) {
      b->rough[i] = texel.rough;
    } else {
      u8 *a = b->albedo + (u64)i * 4u;
      a[0] = (u8)cab_srgb255(texel.r);
      a[1] = (u8)cab_srgb255(texel.g);
      a[2] = (u8)cab_srgb255(texel.b);
      a[3] = 255;
      u8 *o = b->orm + (u64)i * 4u;
      o[0] = (u8)(texel.ao * 255.0f);
      o[1] = (u8)(texel.rough * 255.0f);
      o[2] = (u8)(texel.metal * 255.0f);
      o[3] = 255;
    }
  }
}

hz_internal f32 cab_height_at(const CabBake *b, i32 x, i32 y) {
  const i32 s = CAB_TEX_SIZE;
  return b->height[(u32)(((y % s) + s) % s) * CAB_TEX_SIZE + (u32)(((x % s) + s) % s)];
}

// pass 2 row: the Sobel tangent normal of the wrapped height field (detail tiles carry the roughness delta in alpha)
hz_internal void cab_normal_row(CabTexState *t, u32 bake, u32 y) {
  CabBake *b = &t->bake[bake];
  const b32 detail = bake >= CAB_SET_COUNT;
  const f32 k = cab_bake_normal_scale(bake) * 0.17f;
  u8 *dst = detail ? b->albedo : b->normal;
  for (u32 xi = 0; xi < CAB_TEX_SIZE; xi++) {
    i32 x = (i32)xi, yy = (i32)y;
    f32 dx = cab_height_at(b, x + 1, yy - 1) + 2.0f * cab_height_at(b, x + 1, yy) + cab_height_at(b, x + 1, yy + 1) -
             cab_height_at(b, x - 1, yy - 1) - 2.0f * cab_height_at(b, x - 1, yy) - cab_height_at(b, x - 1, yy + 1);
    f32 dy = cab_height_at(b, x - 1, yy + 1) + 2.0f * cab_height_at(b, x, yy + 1) + cab_height_at(b, x + 1, yy + 1) -
             cab_height_at(b, x - 1, yy - 1) - 2.0f * cab_height_at(b, x, yy - 1) - cab_height_at(b, x + 1, yy - 1);
    f32 nx = -dx * k, ny = -dy * k, nz = 1.0f;
    f32 l = m_sqrtf(nx * nx + ny * ny + nz * nz);
    nx /= l;
    ny /= l;
    nz /= l;
    u32 i = y * CAB_TEX_SIZE + xi;
    u8 *p = dst + (u64)i * 4u;
    p[0] = (u8)((nx * 0.5f + 0.5f) * 255.0f);
    p[1] = (u8)((ny * 0.5f + 0.5f) * 255.0f);
    p[2] = (u8)((nz * 0.5f + 0.5f) * 255.0f);
    p[3] = detail ? (u8)m_clampf((b->rough[i] * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f) : 255;
  }
}

// the mip chain of one map: a 2x2 box, in linear light for albedo
hz_internal void cab_mip_chain(const CabTexState *t, u8 *map, b32 srgb) {
  for (u32 l = 1; l < CAB_MIP_LEVELS; l++) {
    u32 sw = CAB_TEX_SIZE >> (l - 1), dw = CAB_TEX_SIZE >> l;
    const u8 *src = map + cab_mip_offset(l - 1);
    u8 *dst = map + cab_mip_offset(l);
    for (u32 y = 0; y < dw; y++)
      for (u32 x = 0; x < dw; x++) {
        const u8 *p00 = src + ((u64)(y * 2) * sw + x * 2) * 4u;
        const u8 *p10 = p00 + 4, *p01 = p00 + (u64)sw * 4u, *p11 = p01 + 4;
        u8 *d = dst + ((u64)y * dw + x) * 4u;
        for (u32 c = 0; c < 3; c++) {
          if (srgb) {
            f32 s = (t->srgb_decode[p00[c]] + t->srgb_decode[p10[c]] + t->srgb_decode[p01[c]] + t->srgb_decode[p11[c]]) * 0.25f;
            d[c] = t->srgb_encode[(u32)(m_clampf(s, 0.0f, 1.0f) * 4095.0f + 0.5f)];
          } else {
            d[c] = (u8)(((u32)p00[c] + p10[c] + p01[c] + p11[c] + 2u) >> 2);
          }
        }
        d[3] = (u8)(((u32)p00[3] + p10[3] + p01[3] + p11[3] + 2u) >> 2);
      }
  }
}

// every lane bakes whole rows; four balanced barriers: camo means, texels, normals, mips
void cab_tex_lanes(CabTexState *t) {
  if (is_main_thread())
    t->t0 = os_time_now();
  // measure the three camo patterns, one row per job
  Range(u32) mr = lane_range(3 * 96);
  for (u32 j = mr.min; j < mr.max; j++)
    t->camo_row[j] = cab_camo_row_mean(t, j / 96, j % 96);
  lane_sync();
  if (is_main_thread()) {
    for (u32 c = 0; c < 3; c++) {
      f32 s = 0.0f;
      for (u32 r = 0; r < 96; r++)
        s += t->camo_row[c * 96 + r];
      t->camo_src_mean[c] = s / (96.0f * 96.0f);
    }
  }
  lane_sync();
  const u32 rows = CAB_BAKE_COUNT * CAB_TEX_SIZE;
  Range(u32) rg = lane_range(rows);
  for (u32 j = rg.min; j < rg.max; j++)
    cab_bake_row(t, j / CAB_TEX_SIZE, j % CAB_TEX_SIZE);
  lane_sync();
  Range(u32) ng = lane_range(rows);
  for (u32 j = ng.min; j < ng.max; j++)
    cab_normal_row(t, j / CAB_TEX_SIZE, j % CAB_TEX_SIZE);
  lane_sync();
  // sets have three maps, detail tiles one
  const u32 maps = CAB_SET_COUNT * 3 + CAB_DETAIL_COUNT;
  Range(u32) pg = lane_range(maps);
  for (u32 j = pg.min; j < pg.max; j++) {
    if (j < CAB_SET_COUNT * 3) {
      CabBake *b = &t->bake[j / 3];
      u32 m = j % 3;
      cab_mip_chain(t, m == 0 ? b->albedo : m == 1 ? b->normal : b->orm, m == 0);
    } else {
      cab_mip_chain(t, t->bake[CAB_SET_COUNT + (j - CAB_SET_COUNT * 3)].albedo, false);
    }
  }
  lane_sync();
  if (is_main_thread())
    t->ms = os_ticks_to_ms(os_time_diff(os_time_now(), (u64)t->t0));
}

// the allocations the lanes write into: main lane, before the lanes start
void cab_tex_init(CabTexState *t, Allocator *alloc, CodRng *rng) {
  cab_tile_noise_init(&t->noise, rng, alloc);
  t->srgb_decode = ALLOC_ARRAY(alloc, f32, 256);
  t->srgb_encode = ALLOC_ARRAY(alloc, u8, 4096);
  for (u32 i = 0; i < 256; i++) {
    f32 c = (f32)i / 255.0f;
    t->srgb_decode[i] = c <= 0.04045f ? c / 12.92f : m_powf((c + 0.055f) / 1.055f, 2.4f);
  }
  for (u32 i = 0; i < 4096; i++) {
    f32 l = (f32)i / 4095.0f;
    f32 c = l <= 0.0031308f ? l * 12.92f : 1.055f * m_powf(l, 1.0f / 2.4f) - 0.055f;
    t->srgb_encode[i] = (u8)m_clampf(c * 255.0f + 0.5f, 0.0f, 255.0f);
  }
  t->camo_row = ALLOC_ARRAY(alloc, f32, 3 * 96);
  const u64 chain = cab_chain_bytes();
  for (u32 b = 0; b < CAB_BAKE_COUNT; b++) {
    CabBake *k = &t->bake[b];
    k->height = ALLOC_ARRAY_NO_ZERO(alloc, f32, CAB_TEX_SIZE * CAB_TEX_SIZE);
    k->albedo = ALLOC_ARRAY_NO_ZERO(alloc, u8, chain);
    if (b < CAB_SET_COUNT) {
      k->normal = ALLOC_ARRAY_NO_ZERO(alloc, u8, chain);
      k->orm = ALLOC_ARRAY_NO_ZERO(alloc, u8, chain);
    } else {
      k->rough = ALLOC_ARRAY_NO_ZERO(alloc, f32, CAB_TEX_SIZE * CAB_TEX_SIZE);
    }
  }
}

// main lane: the baked chains to GPU textures
void cab_tex_upload(CabTexState *t) {
  GpuTextureMipsDesc d = {.kind = GPU_TEXTURE_KIND_2D, .width = CAB_TEX_SIZE, .height = CAB_TEX_SIZE, .mip_count = CAB_MIP_LEVELS};
  d.data_size = 0;
  for (u32 b = 0; b < CAB_BAKE_COUNT; b++) {
    CabBake *k = &t->bake[b];
    d.format = GPU_TEXTURE_FORMAT_RGBA8_SRGB;
    d.data = k->albedo;
    if (b >= CAB_SET_COUNT)
      d.format = GPU_TEXTURE_FORMAT_RGBA8;
    d.data_size = gpu_texture_mips_data_size(&d);
    d.label = "cod soldier albedo";
    k->tex_albedo = gpu_make_texture_mips(&d);
    if (b < CAB_SET_COUNT) {
      d.format = GPU_TEXTURE_FORMAT_RGBA8;
      d.data = k->normal;
      d.label = "cod soldier normal";
      k->tex_normal = gpu_make_texture_mips(&d);
      d.data = k->orm;
      d.label = "cod soldier orm";
      k->tex_orm = gpu_make_texture_mips(&d);
    }
  }
}
