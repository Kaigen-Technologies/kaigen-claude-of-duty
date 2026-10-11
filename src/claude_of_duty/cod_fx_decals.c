// Claude of Duty effects: the decals on the engine's clustered decals: a ring of projected marks with the lifetimes, fades and caps, and the dev bake of their tiles.

#define CF_DECAL_TEX 256
#define CF_DECAL_DIR "assets/claude_of_duty/fx/decals"

typedef struct CodFxDecalSlot {
  RendererDecalHandle h;
  RendererDecalDesc desc;
  f32 born;
  f32 life;
  f32 fade;
  f32 opacity;
  f32 last_alpha;
  b32 live;
  b32 created;
} CodFxDecalSlot;

typedef struct CodFxDecals {
  HzHandleT(TextureBlobAsset) tex[CF_D_COUNT][3]; // albedo, normal, orm per tile
  b32 ready;
  b32 failed;
  CodFxDecalSlot *slots;
  u32 cap;
  u32 cursor;
  u32 live;
  u32 placed;
  u32 dropped;
  GpuPipeline bake_pipe;
  GpuPipeline maps_pipe;
  GpuRenderTarget field_rt;
  GpuRenderTarget bake_rt;
  u32 bake_stage; // 0 idle, 1 recorded and waiting, 2 written
  u32 bake_frames;
} CodFxDecals;

hz_internal const char *cod_fx_decal_suffix(u32 map) { return map == 0 ? "" : map == 1 ? "_n" : "_o"; }

// loads every tile's three maps; the ring waits until all are resident
hz_internal void cod_fx_decals_init(CodFxDecals *d, Allocator *alloc, AssetSystem *assets, u32 cap) {
  d->cap = cap;
  d->slots = ALLOC_ARRAY(alloc, CodFxDecalSlot, cap);
  for (u32 t = 0; t < CF_D_COUNT; t++)
    for (u32 m = 0; m < 3; m++) {
      String path = str_format(alloc, "claude_of_duty/fx/decals/cod_decal_%%.hzt", fmt_u32(t), fmt_cstr(cod_fx_decal_suffix(m)));
      d->tex[t][m] = asset_load(TextureBlobAsset, assets, .path = path.value);
    }
}

hz_internal void cod_fx_decals_poll(CodFxDecals *d, const AssetSystem *assets) {
  if (d->ready || d->failed) return;
  for (u32 t = 0; t < CF_D_COUNT; t++)
    for (u32 m = 0; m < 3; m++)
      if (!asset_is_ready(assets, d->tex[t][m])) return;
  d->ready = true;
  log_info("fx: decal textures resident");
}

// the box a decal projects: its normal, rolled about it from a gravity-aligned reference, over a thickness of twice the half-depth
hz_internal void cod_fx_decal_place(CodFxDecals *d, RendererState *r, const CodFxDecalReq *q, f32 now) {
  CodFxDecalSlot *s = &d->slots[d->cursor];
  d->cursor = (d->cursor + 1) % d->cap;
  v3 n = v3_normalize(q->normal);
  v3 ref = m_fabsf(n.y) > 0.94f ? (v3){1.0f, 0.0f, 0.0f} : (v3){0.0f, 1.0f, 0.0f};
  v3 t = v3_normalize(v3_sub(ref, v3_scale(n, v3_dot(n, ref))));
  // Rodrigues: t rolled about n
  f32 c = m_cosf(q->roll), sn = m_sinf(q->roll);
  v3 up = v3_add(v3_scale(t, c), v3_scale(v3_cross(n, t), sn));
  f32 value_c = m_cosf(q->max_angle * 0.0174532925f) * 0.5f + 0.5f;
  s->desc = (RendererDecalDesc){.position = q->point,
                                .rotation = decal_orient_from_hit(n, up),
                                .size = {q->size, q->size, q->depth * 2.0f},
                                .albedo = d->tex[q->tile][0],
                                .normal = d->tex[q->tile][1],
                                .orm = d->tex[q->tile][2],
                                .color = {1.0f, 1.0f, 1.0f, q->opacity},
                                .fade_upper = 1.0f,
                                .fade_lower = 1.0f,
                                .normal_fade = m_clampf(value_c - 0.12f, 0.0f, 0.95f),
                                .normal_strength = 1.0f,
                                .layers = DECAL_LAYERS_ALL,
                                .priority = 1};
  s->born = now;
  s->life = q->life;
  s->fade = q->fade;
  s->opacity = q->opacity;
  s->last_alpha = q->opacity;
  if (s->created) {
    renderer_set_decal(r, s->h, &s->desc);
    renderer_set_decal_visible(r, s->h, true);
  } else {
    s->h = renderer_create_decal(r, &s->desc);
    s->created = true;
  }
  if (!s->live) d->live++;
  s->live = true;
  d->placed++;
}

// main lane: places what the consumers queued, fades the marks and frees the dead ones
hz_internal void cod_fx_decals_frame(CodFxDecals *d, RendererState *r, CodFxDecalQueue *q, f32 now) {
  u32 n = concurrent_arr_len(q->q);
  u32 over = (u32)q->dropped_atomic;
  if (n > q->q.cap) n = q->q.cap;
  if (d->ready)
    for (u32 i = 0; i < n; i++) cod_fx_decal_place(d, r, &q->q.items[i], now);
  else
    d->dropped += n;
  d->dropped += over;
  atomic_u32_write_relaxed(&q->q.len_atomic, 0);
  q->dropped_atomic = 0;
  for (u32 i = 0; i < d->cap; i++) {
    CodFxDecalSlot *s = &d->slots[i];
    if (!s->live) continue;
    f32 u = (now - s->born) / s->life;
    if (u >= 1.0f) {
      s->live = false;
      d->live--;
      renderer_set_decal_visible(r, s->h, false);
      continue;
    }
    f32 alpha = s->opacity * (1.0f - m_smoothstepf(s->fade, 1.0f, u));
    if (m_fabsf(alpha - s->last_alpha) > 0.004f) {
      s->desc.color.a = alpha;
      renderer_set_decal(r, s->h, &s->desc);
      s->last_alpha = alpha;
    }
  }
}

/* ---------------------------------------------------------------- dev bake */

hz_internal void cod_fx_decal_pipeline(CodFxDecals *d) {
  GpuPipelineDesc *p = shader_cod_fx_decals_pipeline_desc();
  p->cull_mode = GPU_CULL_NONE;
  p->depth_test = false;
  p->depth_write = false;
  p->no_depth = true;
  p->color_attachment_count = 2;
  p->color_formats[0] = GPU_TEXTURE_FORMAT_RGBA8_SRGB;
  p->color_formats[1] = GPU_TEXTURE_FORMAT_RGBA16F;
  d->bake_pipe = gpu_make_pipeline(p);
  GpuPipelineDesc *m = shader_cod_fx_decal_maps_pipeline_desc();
  m->cull_mode = GPU_CULL_NONE;
  m->depth_test = false;
  m->depth_write = false;
  m->no_depth = true;
  m->color_attachment_count = 2;
  m->color_formats[0] = GPU_TEXTURE_FORMAT_RGBA8;
  m->color_formats[1] = GPU_TEXTURE_FORMAT_RGBA8;
  d->maps_pipe = gpu_make_pipeline(m);
}

// the painters into albedo and a height field, then the normal and ORM maps from the field; four sheets of 4x4 tiles
hz_internal void cod_fx_decal_record(CodFxDecals *d) {
  u32 size = CF_DECAL_TEX * 4;
  d->field_rt = gpu_make_render_target(&(GpuRenderTargetDesc){.name = "cod fx decal field",
                                                              .width = size,
                                                              .height = size,
                                                              .color_attachment_count = 2,
                                                              .mip_levels = 1,
                                                              .formats = {GPU_TEXTURE_FORMAT_RGBA8_SRGB, GPU_TEXTURE_FORMAT_RGBA16F},
                                                              .contents_undefined = true});
  d->bake_rt = gpu_make_render_target(&(GpuRenderTargetDesc){.name = "cod fx decal maps",
                                                             .width = size,
                                                             .height = size,
                                                             .color_attachment_count = 2,
                                                             .mip_levels = 1,
                                                             .formats = {GPU_TEXTURE_FORMAT_RGBA8, GPU_TEXTURE_FORMAT_RGBA8},
                                                             .contents_undefined = true});
  ShaderCodFxDecalsParams p = {.cols = 4.0f, .cell_px = (f32)CF_DECAL_TEX, .seed = 3141.0f};
  gpu_begin_pass(&(GpuPassDesc){.render_target = d->field_rt, .mip_level = 0});
  gpu_apply_pipeline(d->bake_pipe);
  shader_cod_fx_decals_apply_params(&p);
  gpu_apply_bindings(&(GpuBindings){0});
  gpu_draw(3, 1);
  gpu_end_pass();
  ShaderCodFxDecalMapsParams mp = {.cols = 4.0f, .cell_px = (f32)CF_DECAL_TEX};
  gpu_begin_pass(&(GpuPassDesc){.render_target = d->bake_rt, .mip_level = 0});
  gpu_apply_pipeline(d->maps_pipe);
  shader_cod_fx_decal_maps_apply_params(&mp);
  shader_cod_fx_decal_maps_apply_bindings((ShaderCodFxDecalMapsBindings){.fieldTexture = gpu_get_render_target_attachment(d->field_rt, 1)});
  gpu_draw(3, 1);
  gpu_end_pass();
  d->bake_stage = 1;
  d->bake_frames = 0;
}

// the three maps of a tile as a texture source, cooked to BC7 like any authored one
hz_internal void cod_fx_decal_write_hzt(const char *name, b32 srgb, b32 alpha) {
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  String text = str_format(&ta.allocator,
                           "type TextureBlobAsset\nversion 23\n---\nimage \"/claude_of_duty/fx/decals/%\" u8\nformat TEX_FORMAT_BC7\nwidth 0\nheight 0\nis_srgb %\n"
                           "has_alpha %\nhas_alpha_cutoff %\nmax_size 0\n",
                           fmt_cstr(name), fmt_cstr(srgb ? "true" : "false"), fmt_cstr(alpha ? "true" : "false"), fmt_cstr(alpha ? "true" : "false"));
  String path = str_format(&ta.allocator, "%/%.hzt", fmt_cstr(CF_DECAL_DIR), fmt_cstr(name));
  os_write_file(path.value, (u8 *)text.value, text.len);
  tctx_temp_allocator_end(ta);
}

// reads the baked sheet back and writes each tile's maps as png + hzt
hz_internal void cod_fx_decal_write(CodFxDecals *d) {
  u32 size = CF_DECAL_TEX * 4;
  os_create_dir("assets/claude_of_duty/fx");
  os_create_dir(CF_DECAL_DIR);
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  u8 *tile = ALLOC_ARRAY(&ta.allocator, u8, CF_DECAL_TEX * CF_DECAL_TEX * 4);
  u32 written = 0;
  for (u32 m = 0; m < 3; m++) {
    GpuTexture tex = m == 0 ? gpu_get_render_target_attachment(d->field_rt, 0) : gpu_get_render_target_attachment(d->bake_rt, m - 1);
    u8 *sheet = pg_dump_read_rgba8(tex, size, m == 0 ? GPU_TEXTURE_FORMAT_RGBA8_SRGB : GPU_TEXTURE_FORMAT_RGBA8, &ta.allocator);
    if (!sheet) {
      log_error("fx: the decal sheet readback failed (map %)", fmt_u32(m));
      continue;
    }
    for (u32 t = 0; t < CF_D_COUNT; t++) {
      u32 ox = (t % 4) * CF_DECAL_TEX, oy = (t / 4) * CF_DECAL_TEX;
      for (u32 y = 0; y < CF_DECAL_TEX; y++) mem_cpy(tile + y * CF_DECAL_TEX * 4, sheet + ((oy + y) * size + ox) * 4, CF_DECAL_TEX * 4);
      String name = str_format(&ta.allocator, "cod_decal_%%", fmt_u32(t), fmt_cstr(cod_fx_decal_suffix(m)));
      if (pg_dump_rgba_png(tile, CF_DECAL_TEX, CF_DECAL_TEX, CF_DECAL_DIR, name.value, &ta.allocator)) {
        cod_fx_decal_write_hzt(name.value, m == 0, m == 0);
        written++;
      }
    }
  }
  tctx_temp_allocator_end(ta);
  log_info("fx: wrote % decal maps to % (run hzbuild cook, then restart)", fmt_u32(written), fmt_cstr(CF_DECAL_DIR));
  d->bake_stage = 2;
}

// dev: the tiles are baked and dumped when fx_dump is set
hz_internal void cod_fx_decal_dump_frame(CodFxDecals *d, i32 dump) {
  if (dump == 0 || d->bake_stage == 2) return;
  if (d->bake_stage == 0) {
    if (!handle_is_valid(d->bake_pipe)) {
      cod_fx_decal_pipeline(d);
      return;
    }
    if (gpu_pipeline_state(d->bake_pipe) != GPU_PIPELINE_READY || gpu_pipeline_state(d->maps_pipe) != GPU_PIPELINE_READY) return;
    cod_fx_decal_record(d);
    return;
  }
  if (++d->bake_frames >= 4) cod_fx_decal_write(d);
}
