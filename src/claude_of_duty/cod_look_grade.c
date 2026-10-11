// The procedural 33^3 grade: CDL, split tone, saturation, highlight desat, pivoted S-curve.

#define COD_LUT_SIZE 33u

// composite settings
#define COD_CHROMATIC 0.0011f
#define COD_VIGNETTE 0.24f
#define COD_ADS_VIGNETTE 0.34f
#define COD_GRAIN 0.010f
#define COD_SHARPEN 0.25f
#define COD_LUT_STRENGTH 1.0f
#define COD_AGX_SLOPE 1.0f
#define COD_AGX_POWER 1.0f
#define COD_AGX_SAT 1.08f

typedef struct CodGrade {
  v3 slope, offset, power, shadow_tint, highlight_tint;
  f32 saturation, contrast, pivot, highlight_desat, toe;
  f32 knee, soft, norm; // the shoulder
} CodGrade;

hz_internal CodGrade cod_grade_default(void) {
  CodGrade g = {.slope = {1.0f, 0.995f, 0.985f},
                .offset = {-0.004f, -0.002f, 0.004f},
                .power = {1.0f, 1.005f, 1.02f},
                .shadow_tint = {-0.001f, 0.006f, 0.022f},
                .highlight_tint = {0.030f, 0.014f, -0.006f},
                .saturation = 1.20f,
                .contrast = 1.28f,
                .pivot = 0.50f,
                .highlight_desat = 0.10f,
                .toe = 0.008f};
  f32 shoulder = 0.60f, shoulder_soft = 1.20f;
  g.knee = m_minf(0.98f, m_maxf(0.05f, shoulder));
  g.soft = m_maxf(1e-3f, shoulder_soft);
  f32 c_max = g.pivot * m_powf(1.0f / g.pivot, g.contrast);
  g.norm = 1.0f - m_expf(-m_maxf(c_max - g.knee, 1e-3f) / g.soft);
  return g;
}

hz_internal f32 cod_grade_curve(const CodGrade *g, f32 x) {
  f32 t = m_maxf(0.0f, x);
  f32 c = t <= 0.0f ? 0.0f : g->pivot * m_powf(t / g->pivot, g->contrast);
  if (c > g->knee)
    c = g->knee + (1.0f - g->knee) * ((1.0f - m_expf(-(c - g->knee) / g->soft)) / g->norm);
  return g->toe + (1.0f - g->toe) * m_clampf(c, 0.0f, 1.0f);
}

hz_internal f32 cod_grade_luma(v3 c) { return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f; }

// the grade on one display-referred colour
hz_internal v3 cod_grade_apply(const CodGrade *g, v3 c) {
  c = (v3){m_powf(m_maxf(0.0f, c.x * g->slope.x + g->offset.x), g->power.x),
           m_powf(m_maxf(0.0f, c.y * g->slope.y + g->offset.y), g->power.y),
           m_powf(m_maxf(0.0f, c.z * g->slope.z + g->offset.z), g->power.z)};
  f32 l = cod_grade_luma(c);
  f32 shadow_w = m_powf(1.0f - m_minf(1.0f, l), 2.2f), high_w = m_powf(m_minf(1.0f, l), 2.0f);
  c = v3_add(c, v3_add(v3_scale(g->shadow_tint, shadow_w), v3_scale(g->highlight_tint, high_w)));
  f32 l2 = cod_grade_luma(c);
  v3 grey = {l2, l2, l2};
  c = v3_add(grey, v3_scale(v3_sub(c, grey), g->saturation));
  f32 hd = g->highlight_desat * m_powf(m_clampf(l2, 0.0f, 1.0f), 3.0f);
  c = v3_add(c, v3_scale(v3_sub(grey, c), hd));
  return (v3){cod_grade_curve(g, c.x), cod_grade_curve(g, c.y), cod_grade_curve(g, c.z)};
}

// main lane: the RGBA8 3D grade texture, x fastest
hz_internal GpuTexture cod_grade_lut_make(void) {
  CodGrade g = cod_grade_default();
  u32 n = COD_LUT_SIZE, bytes = n * n * n * 4u;
  f32 step = 1.0f / (f32)(n - 1);
  GpuTexture tex = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    u8 *data = ALLOC_ARRAY(&ta.allocator, u8, bytes);
    u32 p = 0;
    for (u32 z = 0; z < n; z++)
      for (u32 y = 0; y < n; y++)
        for (u32 x = 0; x < n; x++) {
          v3 c = cod_grade_apply(&g, (v3){(f32)x * step, (f32)y * step, (f32)z * step});
          data[p++] = (u8)m_roundf(m_clampf(c.x, 0.0f, 1.0f) * 255.0f);
          data[p++] = (u8)m_roundf(m_clampf(c.y, 0.0f, 1.0f) * 255.0f);
          data[p++] = (u8)m_roundf(m_clampf(c.z, 0.0f, 1.0f) * 255.0f);
          data[p++] = 255;
        }
    GpuTextureMipsDesc d = {.kind = GPU_TEXTURE_KIND_3D, .format = GPU_TEXTURE_FORMAT_RGBA8, .width = n, .height = n,
                            .depth_or_layers = n, .mip_count = 1, .data = data, .label = "cod grade lut"};
    d.data_size = gpu_texture_mips_data_size(&d);
    tex = gpu_make_texture_mips(&d);
  }
  return tex;
}
