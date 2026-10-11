// Claude of Duty low-health treatment: a renderer feature after the motion blur and before the exposure meter, recorded only while it shows.
#include "shaders/cod_post_lowhealth.h"

// below these the pass is a no-op and does not record
#define COD_LOWHEALTH_MIN 0.004f

typedef struct CodLowHealth {
  RendererFeature feature;
  GpuComputePipeline kernel;
  GpuBuffer exposure; // the meter's state: [0] the exposure the final pass applies
  GpuTexture out;     // RGBA16F, the treated frame at the post size
  u32 out_w, out_h;
  v4 hurt; // x amount, y pulse, z hit flash, w critical
} CodLowHealth;

hz_internal void cod_lowhealth_make_target(CodLowHealth *l, u32 post_w, u32 post_h) {
  if (handle_is_valid(l->out))
    gpu_destroy_texture(l->out);
  l->out_w = MAX(post_w, 1u);
  l->out_h = MAX(post_h, 1u);
  l->out = gpu_make_storage_texture(&(GpuStorageTextureDesc){.label = "cod low health",
                                                             .kind = GPU_TEXTURE_KIND_2D,
                                                             .format = GPU_TEXTURE_FORMAT_RGBA16F,
                                                             .width = l->out_w,
                                                             .height = l->out_h});
}

hz_internal b32 cod_lowhealth_ready(void *user) {
  CodLowHealth *l = (CodLowHealth *)user;
  b32 shows = l->hurt.x > COD_LOWHEALTH_MIN || l->hurt.z > COD_LOWHEALTH_MIN;
  return shows && handle_is_valid(l->out) && gpu_texture_is_ready(l->out);
}

hz_internal void cod_lowhealth_record(RendererState *r, const RendererPassCtx *ctx, void *user) {
  CodLowHealth *l = (CodLowHealth *)user;
  // resize: keep the vignette circular whatever the aspect
  f32 w = (f32)MAX(ctx->width, 1u), h = (f32)MAX(ctx->height, 1u);
  v2 aspect = w >= h ? (v2){1.0f, h / w} : (v2){w / h, 1.0f};
  gpu_buffer_access(l->exposure, GPU_ACCESS_SHADER_READ);
  gpu_texture_access(l->out, GPU_ACCESS_SHADER_WRITE);
  gpu_apply_compute_pipeline(l->kernel);
  shader_cod_post_lowhealth_apply_params(&(ShaderCodPostLowhealthParams){
      .hurt = {l->hurt.x, l->hurt.y, l->hurt.z, l->hurt.w},
      .size = {(f32)l->out_w, (f32)l->out_h, w, h},
      .shape = {aspect.x, aspect.y, renderer_pre_exposure(r), 0.0f},
  });
  shader_cod_post_lowhealth_apply_bindings(
      (ShaderCodPostLowhealthBindings){.state = l->exposure, .color_out = l->out, .color_tex = ctx->resolved_color});
  gpu_dispatch((l->out_w + 7u) / 8u, (l->out_h + 7u) / 8u, 1);
  gpu_texture_access(l->out, GPU_ACCESS_SHADER_READ);
  renderer_set_post_color(r, l->out);
}

hz_internal void cod_lowhealth_resize(RendererState *r, const RendererResizeCtx *ctx, void *user) {
  UNUSED(r);
  cod_lowhealth_make_target((CodLowHealth *)user, ctx->display_width, ctx->display_height);
}

hz_internal void cod_lowhealth_destroy(RendererState *r, void *user) {
  UNUSED(r);
  CodLowHealth *l = (CodLowHealth *)user;
  if (handle_is_valid(l->out))
    gpu_destroy_texture(l->out);
  l->out = (GpuTexture){0};
}

// main lane, after renderer init and the motion blur (same order, later registration): the kernel, the target and the feature
hz_internal CodLowHealth *cod_lowhealth_init(RendererState *r, Allocator *alloc, GpuBuffer exposure) {
  CodLowHealth *l = ALLOC(alloc, CodLowHealth);
  l->exposure = exposure;
  l->kernel = gpu_make_compute_pipeline_async(shader_cod_post_lowhealth_compute_pipeline_desc());
  u32 post_w = 0, post_h = 0;
  renderer_post_size(r, &post_w, &post_h);
  cod_lowhealth_make_target(l, post_w, post_h);
  l->feature = renderer_add_feature(r, &(RendererFeatureDesc){
                                           .name = "cod_low_health",
                                           .user = l,
                                           .record = {[RENDERER_PHASE_AFTER_RESOLVE] = cod_lowhealth_record},
                                           .reads = RENDERER_RESOURCE_HDR_COLOR,
                                           .resize = cod_lowhealth_resize,
                                           .destroy = cod_lowhealth_destroy,
                                           .ready = cod_lowhealth_ready,
                                           .order = RENDERER_ORDER_BUILTIN,
                                       });
  assert_msg(l->feature.id != 0, "cod_lowhealth_init: the cod_low_health feature did not register");
  renderer_feature_require(r, l->feature, NULL, 0, &l->kernel, 1);
  return l;
}
