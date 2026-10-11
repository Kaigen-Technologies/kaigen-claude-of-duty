// Claude of Duty effects: the particle sprite atlas as one GPU bake: 4x4 painters, mipped to where a cell is a few texels wide.

#define CF_ATLAS_FORMAT GPU_TEXTURE_FORMAT_RGBA8_SRGB
#define CF_ATLAS_SEED 2718.0f

typedef struct CodFxAtlas {
  GpuPipeline sprite_pipe;
  GpuPipeline down_pipe;
  GpuRenderTarget rt;
  GpuTexture tex;
  GpuTimer timer;
  u32 size;
  u32 cols;
  u32 mips;
  b32 recorded;
  u32 timer_frames;
  f64 record_ms;
} CodFxAtlas;

hz_internal u32 cod_fx_mip_count(u32 size) {
  u32 n = 1;
  while (size > 1) {
    size >>= 1;
    n++;
  }
  return n;
}

// a bake pass pipeline: no depth, no cull, one sRGB colour attachment
hz_internal void cod_fx_bake_finalize(GpuPipelineDesc *d) {
  d->cull_mode = GPU_CULL_NONE;
  d->depth_test = false;
  d->depth_write = false;
  d->no_depth = true;
  d->color_attachment_count = 1;
  d->color_formats[0] = CF_ATLAS_FORMAT;
}

// main lane: compiles the two bake pipelines asynchronously
hz_internal void cod_fx_atlas_pipelines(CodFxAtlas *a, u32 size) {
  a->size = size;
  a->cols = CF_ATLAS_COLS;
  // the chain stops while a cell is still several texels wide, or the box filter reaches across cell borders
  u32 cell_mips = cod_fx_mip_count(size / a->cols);
  a->mips = cell_mips > 3u ? cell_mips - 3u : 1u;
  GpuPipelineDesc *sp = shader_cod_fx_sprites_pipeline_desc();
  cod_fx_bake_finalize(sp);
  sp->compile_async = true;
  a->sprite_pipe = gpu_make_pipeline(sp);
  GpuPipelineDesc *dp = shader_pg_bake_downsample_rgba_pipeline_desc();
  cod_fx_bake_finalize(dp);
  dp->compile_async = true;
  a->down_pipe = gpu_make_pipeline(dp);
}

hz_internal b32 cod_fx_atlas_pipelines_ready(const CodFxAtlas *a) {
  return gpu_pipeline_state(a->sprite_pipe) == GPU_PIPELINE_READY && gpu_pipeline_state(a->down_pipe) == GPU_PIPELINE_READY;
}

hz_internal b32 cod_fx_atlas_pipelines_failed(const CodFxAtlas *a) {
  return gpu_pipeline_state(a->sprite_pipe) == GPU_PIPELINE_FAILED || gpu_pipeline_state(a->down_pipe) == GPU_PIPELINE_FAILED;
}

// records the 16 painters in one fullscreen pass and the mip chain; main lane inside the frame
hz_internal void cod_fx_atlas_record(CodFxAtlas *a) {
  u64 t0 = os_time_now();
  a->rt = gpu_make_render_target(&(GpuRenderTargetDesc){
      .name = "cod fx sprites",
      .width = a->size,
      .height = a->size,
      .color_attachment_count = 1,
      .mip_levels = a->mips,
      .formats = {CF_ATLAS_FORMAT},
      .contents_undefined = true,
  });
  a->tex = gpu_get_render_target_attachment(a->rt, 0);
  a->timer = gpu_create_timer("cod fx sprites");
  gpu_timer_begin(a->timer);
  ShaderCodFxSpritesParams sp = {.cols = (f32)a->cols, .cell_px = (f32)(a->size / a->cols), .seed = CF_ATLAS_SEED};
  gpu_begin_pass(&(GpuPassDesc){.render_target = a->rt, .mip_level = 0});
  gpu_apply_pipeline(a->sprite_pipe);
  shader_cod_fx_sprites_apply_params(&sp);
  gpu_apply_bindings(&(GpuBindings){0});
  gpu_draw(3, 1);
  gpu_end_pass();
  for (u32 mip = 1; mip < a->mips; mip++) {
    u32 src = a->size >> (mip - 1);
    ShaderPgBakeDownsampleRgbaParams dp = {.texel_size = {1.0f / (f32)src, 1.0f / (f32)src}, .src_mip = (f32)(mip - 1)};
    gpu_begin_pass(&(GpuPassDesc){.render_target = a->rt, .mip_level = mip});
    gpu_apply_pipeline(a->down_pipe);
    shader_pg_bake_downsample_rgba_apply_params(&dp);
    shader_pg_bake_downsample_rgba_apply_bindings((ShaderPgBakeDownsampleRgbaBindings){.sourceTexture = a->tex});
    gpu_draw(3, 1);
    gpu_end_pass();
  }
  gpu_timer_end(a->timer);
  a->recorded = true;
  a->record_ms = os_ticks_to_ms(os_time_diff(os_time_now(), t0));
}

// logs the GPU time a few frames after the bake ran
hz_internal void cod_fx_atlas_collect(CodFxAtlas *a) {
  if (!a->recorded || a->timer_frames == 0xffffffffu) return;
  if (++a->timer_frames < 4) return;
  f32 gpu_ms = gpu_timer_result_ms(a->timer);
  log_info("fx: particle atlas baked (% px, % tiles, % mips): record % ms, gpu % ms", fmt_u32(a->size), fmt_u32(a->cols * a->cols), fmt_u32(a->mips),
           fmt_f32_p((f32)a->record_ms, 2), fmt_f32_p(gpu_ms, 2));
  gpu_destroy_timer(a->timer);
  a->timer_frames = 0xffffffffu;
}
