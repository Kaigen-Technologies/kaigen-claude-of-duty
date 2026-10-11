// Claude of Duty materials: the GPU bake driver. One fullscreen pass evaluates a surface into albedo+height and ORM (and height residual), a Sobel pass derives the normal, the mip chain is built from the read-back base level.

#define COD_BAKE_VARIANTS 32
#define COD_SLOT_DETAIL COD_MAT_COUNT
#define COD_SLOT_MACRO (COD_MAT_COUNT + 1)
#define COD_SLOT_FIRST_VARIANT (COD_MAT_COUNT + 2)
#define COD_SLOT_COUNT (COD_SLOT_FIRST_VARIANT + COD_BAKE_VARIANTS)
#define COD_BAKE_SCRATCH 6u
// texels one frame may record before the rest waits for the next (a 1K bake is 1M)
#define COD_BAKE_FRAME_TEXELS (2u * 1024u * 1024u + 1u)
#define COD_MAP_ALBEDO 0u
#define COD_MAP_ORM 1u
#define COD_MAP_NORMAL 2u

typedef struct CodBakeUniforms {
  v4 head; // x seed
  v4 tint_a;
  v4 tint_b;
  v4 param;
} CodBakeUniforms;

typedef enum CodSlotState {
  COD_SLOT_FREE = 0,
  COD_SLOT_QUEUED,
  COD_SLOT_READBACK, // baked on the GPU, waiting for the base level to come back
  COD_SLOT_BAKED,
  COD_SLOT_FAILED,
} CodSlotState;

// reusable single-level targets of one size, and the CPU mip chains the base levels are read back into
typedef struct CodScratch {
  u32 size;
  b32 data;
  b32 busy;
  b32 has_normal;
  GpuRenderTarget surf;
  GpuRenderTarget normal;
  u8 *chain[3];
  u64 chain_bytes[3];
} CodScratch;

// one texture set's GPU objects and where it is in the queue
typedef struct CodBakeSlot {
  CodBakeDef def;
  b32 data;        // linear data maps (detail, macro) rather than sRGB albedo
  b32 want_normal;
  u32 state;       // CodSlotState
  CodTextureSet set; // the persistent textures, valid once BAKED (the detail slot: albedo and normal live in `layers`)
  GpuTexture layers; // the detail slot: albedo (layer 0) and normal (layer 1) as one RGBA8 array, one material binding
  u32 scratch;
  u32 got;         // bit per map read back
  f32 record_ms;   // CPU time recording the passes
  GpuTimer timer;
  u32 timer_frames; // frames since recorded, until the GPU time is read
} CodBakeSlot;

// the bake pipelines and slots (main-lane state)
typedef struct CodBakeState {
  GpuPipeline prog[COD_PROG_COUNT];
  GpuPipeline normal;
  CodBakeSlot slot[COD_SLOT_COUNT];
  CodScratch scratch[COD_BAKE_SCRATCH];
  Allocator alloc;     // chain buffers and tables, never freed (the scratch is reused)
  f32 *srgb_decode;    // 256 entries
  u8 *srgb_encode;     // 4096 entries
  u32 variant_count;
  u32 queue[COD_SLOT_COUNT];
  u32 queue_head;
  u32 queue_tail;
  u32 library_total;   // library slots + the two shared maps
  u32 library_baked;
  b32 pipelines_made;
  u32 epoch;         // bumps whenever a slot publishes its textures
  u64 first_record;   // os time of the first recorded bake
  u64 last_record;
  f64 cpu_ms_total;
  f64 gpu_ms_total;
  b32 totals_logged;
} CodBakeState;

hz_internal GpuPipelineDesc *cod_bake_program_desc(u32 program) {
  switch (program) {
  case COD_PROG_CONCRETE: return shader_cod_bake_concrete_pipeline_desc();
  case COD_PROG_BRICK: return shader_cod_bake_brick_pipeline_desc();
  case COD_PROG_PLASTER: return shader_cod_bake_plaster_pipeline_desc();
  case COD_PROG_TILE: return shader_cod_bake_tile_pipeline_desc();
  case COD_PROG_ASPHALT: return shader_cod_bake_asphalt_pipeline_desc();
  case COD_PROG_SAND: return shader_cod_bake_sand_pipeline_desc();
  case COD_PROG_DIRT: return shader_cod_bake_dirt_pipeline_desc();
  case COD_PROG_GRAVEL: return shader_cod_bake_gravel_pipeline_desc();
  case COD_PROG_METAL_RUST: return shader_cod_bake_metal_rust_pipeline_desc();
  case COD_PROG_METAL_PAINTED: return shader_cod_bake_metal_painted_pipeline_desc();
  case COD_PROG_METAL_BRUSHED: return shader_cod_bake_metal_brushed_pipeline_desc();
  case COD_PROG_CORRUGATED: return shader_cod_bake_corrugated_pipeline_desc();
  case COD_PROG_WOOD: return shader_cod_bake_wood_pipeline_desc();
  case COD_PROG_FABRIC: return shader_cod_bake_fabric_pipeline_desc();
  case COD_PROG_BURLAP: return shader_cod_bake_burlap_pipeline_desc();
  case COD_PROG_FOLIAGE: return shader_cod_bake_foliage_pipeline_desc();
  case COD_PROG_RUBBER: return shader_cod_bake_rubber_pipeline_desc();
  case COD_PROG_GLASS: return shader_cod_bake_glass_pipeline_desc();
  case COD_PROG_DETAIL: return shader_cod_bake_detail_pipeline_desc();
  default: return shader_cod_bake_macro_pipeline_desc();
  }
}

// a bake pass pipeline: no depth, no cull, the given attachments
hz_internal void cod_bake_finalize(GpuPipelineDesc *d, u32 count, GpuTextureFormat f0, GpuTextureFormat f1) {
  d->cull_mode = GPU_CULL_NONE;
  d->depth_test = false;
  d->depth_write = false;
  d->no_depth = true;
  d->color_attachment_count = count;
  d->color_formats[0] = f0;
  d->color_formats[1] = f1;
}

// main lane: compiles every bake pipeline (asynchronously) and builds the sRGB tables the mip filter uses
hz_internal void cod_bake_pipelines(CodBakeState *b) {
  for (u32 p = 0; p < COD_PROG_COUNT; p++) {
    GpuPipelineDesc *d = cod_bake_program_desc(p);
    b32 data = p == COD_PROG_DETAIL || p == COD_PROG_MACRO;
    cod_bake_finalize(d, 2, data ? GPU_TEXTURE_FORMAT_RGBA8 : GPU_TEXTURE_FORMAT_RGBA8_SRGB, GPU_TEXTURE_FORMAT_RGBA8);
    d->compile_async = true;
    b->prog[p] = gpu_make_pipeline(d);
  }
  GpuPipelineDesc *n = shader_cod_bake_normal_pipeline_desc();
  cod_bake_finalize(n, 1, GPU_TEXTURE_FORMAT_RG8, GPU_TEXTURE_FORMAT_RG8);
  n->compile_async = true;
  b->normal = gpu_make_pipeline(n);

  b->srgb_decode = ALLOC_ARRAY(&b->alloc, f32, 256);
  b->srgb_encode = ALLOC_ARRAY(&b->alloc, u8, 4096);
  for (u32 i = 0; i < 256; i++) {
    f32 c = (f32)i / 255.0f;
    b->srgb_decode[i] = c <= 0.04045f ? c / 12.92f : m_powf((c + 0.055f) / 1.055f, 2.4f);
  }
  for (u32 i = 0; i < 4096; i++) {
    f32 l = (f32)i / 4095.0f;
    f32 c = l <= 0.0031308f ? l * 12.92f : 1.055f * m_powf(l, 1.0f / 2.4f) - 0.055f;
    b->srgb_encode[i] = (u8)m_clampf(c * 255.0f + 0.5f, 0.0f, 255.0f);
  }
  b->pipelines_made = true;
}

hz_internal u32 cod_bake_mip_count(u32 size) {
  u32 n = 1;
  while (size > 1) {
    size >>= 1;
    n++;
  }
  return n;
}

// bytes of a full mip chain of a square map
hz_internal u64 cod_bake_chain_bytes(u32 size, u32 channels) {
  u64 total = 0;
  for (u32 n = size; n >= 1; n >>= 1) {
    total += (u64)n * n * channels;
    if (n == 1)
      break;
  }
  return total;
}

// registers a slot; the bake is recorded when its pipelines are ready
hz_internal void cod_bake_slot_make(CodBakeState *b, u32 index, const CodBakeDef *def, b32 data, b32 want_normal, const char *name) {
  CodBakeSlot *k = &b->slot[index];
  memzero_struct(k);
  k->def = *def;
  k->data = data;
  k->want_normal = want_normal;
  (void)name;
  k->set.size = def->size;
  k->set.world_size = def->world_size;
  k->state = COD_SLOT_QUEUED;
  b->queue[b->queue_tail++ % COD_SLOT_COUNT] = index;
}

hz_internal v4 cod_bake_tint(u32 hex) { return hex ? cod_hex(hex) : (v4){1.0f, 1.0f, 1.0f, 1.0f}; }

// a free scratch entry of this size and colour space, created on first use; -1 while all are in flight
hz_internal i32 cod_bake_scratch_acquire(CodBakeState *b, const CodBakeSlot *k, const char *name) {
  u32 size = k->def.size;
  for (u32 i = 0; i < COD_BAKE_SCRATCH; i++) {
    CodScratch *s = &b->scratch[i];
    if (handle_is_valid(s->surf) && !s->busy && s->size == size && s->data == k->data && (s->has_normal || !k->want_normal)) {
      s->busy = true;
      return (i32)i;
    }
  }
  // an unused entry, else a free one of another size is recycled (targets rebuilt, chain buffers grown only when too small)
  for (u32 pass = 0; pass < 2; pass++) {
   for (u32 i = 0; i < COD_BAKE_SCRATCH; i++) {
    CodScratch *s = &b->scratch[i];
    if (s->busy || (pass == 0) == handle_is_valid(s->surf))
      continue;
    if (handle_is_valid(s->surf)) {
      gpu_destroy_render_target(s->surf);
      gpu_destroy_render_target(s->normal);
    }
    s->size = size;
    s->data = k->data;
    s->busy = true;
    s->has_normal = true;
    s->surf = gpu_make_render_target(&(GpuRenderTargetDesc){
        .name = name,
        .width = size,
        .height = size,
        .color_attachment_count = 2,
        .mip_levels = 1,
        .formats = {k->data ? GPU_TEXTURE_FORMAT_RGBA8 : GPU_TEXTURE_FORMAT_RGBA8_SRGB, GPU_TEXTURE_FORMAT_RGBA8},
        .contents_undefined = true,
    });
    s->normal = gpu_make_render_target(&(GpuRenderTargetDesc){
        .name = name,
        .width = size,
        .height = size,
        .color_attachment_count = 1,
        .mip_levels = 1,
        .formats = {GPU_TEXTURE_FORMAT_RG8},
        .contents_undefined = true,
    });
    for (u32 m = 0; m < 3; m++) {
      u64 need = cod_bake_chain_bytes(size, m == COD_MAP_NORMAL ? 2 : 4);
      if (s->chain_bytes[m] < need) {
        s->chain[m] = ALLOC_ARRAY_NO_ZERO(&b->alloc, u8, need);
        s->chain_bytes[m] = need;
      }
    }
    return (i32)i;
   }
  }
  return -1;
}

// records every pass of one bake into scratch targets and queues the base-level readbacks; false while no scratch is free
hz_internal b32 cod_bake_record(CodBakeState *b, CodBakeSlot *k, const char *name) {
  i32 si = cod_bake_scratch_acquire(b, k, name);
  if (si < 0)
    return false;
  u64 t0 = os_time_now();
  CodScratch *s = &b->scratch[si];
  u32 size = k->def.size;
  GpuTexture albedo = gpu_get_render_target_attachment(s->surf, 0);
  GpuTexture orm = gpu_get_render_target_attachment(s->surf, 1);
  GpuTexture normal = gpu_get_render_target_attachment(s->normal, 0);

  k->timer = gpu_create_timer(name);
  gpu_timer_begin(k->timer);

  CodBakeUniforms u = {
      .head = {k->def.seed, 0, 0, 0},
      .tint_a = cod_bake_tint(k->def.tint_a),
      .tint_b = cod_bake_tint(k->def.tint_b),
      .param = k->def.param,
  };
  gpu_begin_pass(&(GpuPassDesc){.render_target = s->surf, .mip_level = 0});
  gpu_apply_pipeline(b->prog[k->def.program]);
  gpu_apply_uniforms(0, &u, sizeof(u));
  gpu_apply_bindings(&(GpuBindings){0});
  gpu_draw(3, 1);
  gpu_end_pass();

  if (k->want_normal) {
    ShaderCodBakeNormalParams np = {.head = {(f32)size, k->def.relief / k->def.world_size, 0, 0}};
    gpu_begin_pass(&(GpuPassDesc){.render_target = s->normal, .mip_level = 0});
    gpu_apply_pipeline(b->normal);
    shader_cod_bake_normal_apply_params(&np);
    shader_cod_bake_normal_apply_bindings((ShaderCodBakeNormalBindings){.albedoTexture = albedo, .ormTexture = orm});
    gpu_draw(3, 1);
    gpu_end_pass();
  }
  gpu_timer_end(k->timer);

  GpuReadTextureDesc whole = {0};
  gpu_queue_texture_read(albedo, 0, &whole);
  gpu_queue_texture_read(orm, 0, &whole);
  if (k->want_normal)
    gpu_queue_texture_read(normal, 0, &whole);

  k->scratch = (u32)si;
  k->got = 0;
  k->record_ms = (f32)os_ticks_to_ms(os_time_diff(os_time_now(), t0));
  k->state = COD_SLOT_READBACK;
  return true;
}

// 2x2 box filter of a map's base level down to 1x1; sRGB colour averages in linear light
hz_internal void cod_bake_fill_chain(const CodBakeState *b, u8 *chain, u32 size, u32 channels, b32 srgb) {
  u8 *src = chain;
  for (u32 n = size; n > 1; n >>= 1) {
    u32 m = n >> 1;
    u8 *dst = src + (u64)n * n * channels;
    for (u32 y = 0; y < m; y++) {
      for (u32 x = 0; x < m; x++) {
        const u8 *p0 = src + ((u64)(2 * y) * n + 2 * x) * channels;
        const u8 *p1 = p0 + channels;
        const u8 *p2 = p0 + (u64)n * channels;
        const u8 *p3 = p2 + channels;
        u8 *o = dst + ((u64)y * m + x) * channels;
        for (u32 c = 0; c < channels; c++) {
          if (srgb && c < 3) {
            f32 lin = (b->srgb_decode[p0[c]] + b->srgb_decode[p1[c]] + b->srgb_decode[p2[c]] + b->srgb_decode[p3[c]]) * 0.25f;
            o[c] = b->srgb_encode[(u32)(lin * 4095.0f + 0.5f)];
          } else {
            o[c] = (u8)(((u32)p0[c] + p1[c] + p2[c] + p3[c] + 2u) >> 2);
          }
        }
      }
    }
    src = dst;
  }
}

hz_internal GpuTexture cod_bake_upload_map(u8 *chain, u32 size, GpuTextureFormat fmt, const char *label) {
  GpuTextureMipsDesc d = {.kind = GPU_TEXTURE_KIND_2D, .format = fmt, .width = size, .height = size, .mip_count = cod_bake_mip_count(size), .data = chain, .label = label};
  d.data_size = gpu_texture_mips_data_size(&d);
  return gpu_make_texture_mips(&d);
}

// the detail maps as one 2-layer RGBA8 array: per mip the albedo layer, then the RG normal widened to RGBA
hz_internal GpuTexture cod_bake_upload_layers(const u8 *albedo, const u8 *normal, u32 size, const char *label) {
  GpuTextureMipsDesc d = {.kind = GPU_TEXTURE_KIND_2D_ARRAY, .format = GPU_TEXTURE_FORMAT_RGBA8, .width = size, .height = size,
                          .depth_or_layers = 2, .mip_count = cod_bake_mip_count(size), .label = label};
  d.data_size = gpu_texture_mips_data_size(&d);
  GpuTexture tex = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    u8 *out = ALLOC_ARRAY_NO_ZERO(&ta.allocator, u8, d.data_size);
    u8 *o = out;
    for (u32 m = 0; m < d.mip_count; m++) {
      u32 texels = gpu_texture_mips_level(&d, m).width * gpu_texture_mips_level(&d, m).height;
      mem_cpy(o, albedo, texels * 4u);
      o += texels * 4u;
      albedo += texels * 4u;
      for (u32 t = 0; t < texels; t++) {
        o[0] = normal[0];
        o[1] = normal[1];
        o[2] = 0;
        o[3] = 255;
        o += 4;
        normal += 2;
      }
    }
    d.data = out;
    tex = gpu_make_texture_mips(&d);
  }
  return tex;
}

// main lane, inside a frame: collects finished readbacks, builds the mip chains and publishes the persistent textures
hz_internal void cod_bake_publish(CodBakeState *b, u32 *published, const char *(*name_of)(u32)) {
  for (u32 i = 0; i < COD_SLOT_COUNT; i++) {
    CodBakeSlot *k = &b->slot[i];
    if (k->state != COD_SLOT_READBACK)
      continue;
    CodScratch *s = &b->scratch[k->scratch];
    u32 size = k->def.size;
    u32 want = k->want_normal ? 3u : 2u;
    for (u32 m = 0; m < want; m++) {
      GpuTexture tex = m == COD_MAP_NORMAL ? gpu_get_render_target_attachment(s->normal, 0) : gpu_get_render_target_attachment(s->surf, m);
      u32 bytes = size * size * (m == COD_MAP_NORMAL ? 2u : 4u);
      if ((k->got & (1u << m)) == 0 && gpu_get_texture_read(tex, 0, s->chain[m], bytes))
        k->got |= 1u << m;
    }
    if (k->got != (1u << want) - 1u)
      continue;

    const char *name = name_of(i);
    cod_bake_fill_chain(b, s->chain[COD_MAP_ALBEDO], size, 4, !k->data);
    cod_bake_fill_chain(b, s->chain[COD_MAP_ORM], size, 4, false);
    k->set.orm = cod_bake_upload_map(s->chain[COD_MAP_ORM], size, GPU_TEXTURE_FORMAT_RGBA8, name);
    if (k->want_normal)
      cod_bake_fill_chain(b, s->chain[COD_MAP_NORMAL], size, 2, false);
    if (i == COD_SLOT_DETAIL) {
      k->layers = cod_bake_upload_layers(s->chain[COD_MAP_ALBEDO], s->chain[COD_MAP_NORMAL], size, name);
    } else {
      k->set.albedo = cod_bake_upload_map(s->chain[COD_MAP_ALBEDO], size, k->data ? GPU_TEXTURE_FORMAT_RGBA8 : GPU_TEXTURE_FORMAT_RGBA8_SRGB, name);
      if (k->want_normal)
        k->set.normal = cod_bake_upload_map(s->chain[COD_MAP_NORMAL], size, GPU_TEXTURE_FORMAT_RG8, name);
    }
    s->busy = false;
    k->state = COD_SLOT_BAKED;
    b->epoch++;
    if (i <= COD_SLOT_MACRO)
      b->library_baked++;
    (*published)++;
  }
}

// the pipelines one slot's bake needs are compiled
hz_internal b32 cod_bake_slot_ready(const CodBakeState *b, const CodBakeSlot *k) {
  if (gpu_pipeline_state(b->prog[k->def.program]) != GPU_PIPELINE_READY)
    return false;
  if (k->want_normal && gpu_pipeline_state(b->normal) != GPU_PIPELINE_READY)
    return false;
  return true;
}

hz_internal b32 cod_bake_slot_failed(const CodBakeState *b, const CodBakeSlot *k) {
  return gpu_pipeline_state(b->prog[k->def.program]) == GPU_PIPELINE_FAILED || gpu_pipeline_state(b->normal) == GPU_PIPELINE_FAILED;
}

// reads the GPU times of recorded bakes a few frames after they ran
hz_internal void cod_bake_collect_timers(CodBakeState *b, const char *(*name_of)(u32)) {
  for (u32 i = 0; i < COD_SLOT_COUNT; i++) {
    CodBakeSlot *k = &b->slot[i];
    if (k->state != COD_SLOT_BAKED || k->timer_frames == 0xffffffffu)
      continue;
    if (++k->timer_frames < 4)
      continue;
    f32 gpu_ms = gpu_timer_result_ms(k->timer);
    b->gpu_ms_total += gpu_ms;
    log_info("materials: baked % (% px) record % ms, gpu % ms", fmt_cstr(name_of(i)), fmt_u32(k->def.size), fmt_f32_p(k->record_ms, 2),
             fmt_f32_p(gpu_ms, 2));
    gpu_destroy_timer(k->timer);
    k->timer_frames = 0xffffffffu;
  }
}
