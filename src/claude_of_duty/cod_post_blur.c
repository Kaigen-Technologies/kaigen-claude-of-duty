// Claude of Duty motion blur: per-object blur from the velocity buffer as a renderer feature between the temporal resolve and the DoF.
#include "shaders/cod_post_tile.h"
#include "shaders/cod_post_tile_r32f.h"
#include "shaders/cod_post_blur.h"
#include "shaders/cod_post_blur_r32f.h"

// the shutter, the dt clamp and its 1/60 base rate; tile, taps' max radius and intensity
#define COD_BLUR_SHUTTER 0.42f
#define COD_BLUR_MIN_DT (1.0f / 480.0f)
#define COD_BLUR_MAX_DT 0.1f
#define COD_BLUR_RADIUS_PX 48.0f
#define COD_BLUR_INTENSITY 1.0f
#define COD_BLUR_TILE 16u

typedef struct CodBlur {
  RendererFeature feature;
  GpuComputePipeline kernels[4]; // tile and blur, each over the scene depth and over the upscaler's R32F depth
  GpuTexture tile;               // RG16F, the max velocity per 16x16 tile, render res
  GpuTexture out;                // RGBA16F, the blurred frame at the post size
  u32 out_w, out_h, tile_w, tile_h;
  b32 enabled;
  b32 have_time;
  f32 prev_time;
} CodBlur;

hz_internal u32 cod_blur_groups(u32 n) { return (n + 7u) / 8u; }

hz_internal void cod_blur_destroy_targets(CodBlur *b) {
  if (handle_is_valid(b->tile))
    gpu_destroy_texture(b->tile);
  if (handle_is_valid(b->out))
    gpu_destroy_texture(b->out);
  b->tile = (GpuTexture){0};
  b->out = (GpuTexture){0};
}

// the tile texture follows the velocity buffer's size (render res), the output the post size
hz_internal void cod_blur_make_targets(CodBlur *b, u32 render_w, u32 render_h, u32 post_w, u32 post_h) {
  cod_blur_destroy_targets(b);
  b->out_w = MAX(post_w, 1u);
  b->out_h = MAX(post_h, 1u);
  b->tile_w = MAX((render_w + COD_BLUR_TILE - 1u) / COD_BLUR_TILE, 1u);
  b->tile_h = MAX((render_h + COD_BLUR_TILE - 1u) / COD_BLUR_TILE, 1u);
  b->tile = gpu_make_storage_texture(&(GpuStorageTextureDesc){.label = "cod motion blur tiles",
                                                              .kind = GPU_TEXTURE_KIND_2D,
                                                              .format = GPU_TEXTURE_FORMAT_RG16F,
                                                              .width = b->tile_w,
                                                              .height = b->tile_h});
  b->out = gpu_make_storage_texture(&(GpuStorageTextureDesc){.label = "cod motion blur",
                                                             .kind = GPU_TEXTURE_KIND_2D,
                                                             .format = GPU_TEXTURE_FORMAT_RGBA16F,
                                                             .width = b->out_w,
                                                             .height = b->out_h});
}

hz_internal b32 cod_blur_ready(void *user) {
  CodBlur *b = (CodBlur *)user;
  return b->enabled && handle_is_valid(b->out) && gpu_texture_is_ready(b->out) && gpu_texture_is_ready(b->tile);
}

// after the temporal resolve, before the exposure meter and the DoF: the blurred frame replaces the post colour
hz_internal void cod_blur_record(RendererState *r, const RendererPassCtx *ctx, void *user) {
  CodBlur *b = (CodBlur *)user;
  b32 r32f = handle_is_valid(ctx->depth_r32f);
  GpuTexture depth = r32f ? ctx->depth_r32f : ctx->scene_depth;
  if (!ctx->camera || !handle_is_valid(depth) || !handle_is_valid(ctx->velocity))
    return;
  // dt is the frame time clamped to [1/480, 0.1], 1/60 when the clock is stopped
  f32 dt = b->have_time ? ctx->time - b->prev_time : 0.0f;
  if (dt <= 0.0f)
    dt = 1.0f / 60.0f;
  dt = m_clampf(dt, COD_BLUR_MIN_DT, COD_BLUR_MAX_DT);
  b->prev_time = ctx->time;
  b->have_time = true;
  f32 shutter = COD_BLUR_SHUTTER * (1.0f / 60.0f / dt);
  PostProjectionBand band = renderer_projection_band(r);
  f32 depth_b = ctx->camera->proj.m[3][2], depth_a = ctx->camera->proj.m[2][2];

  gpu_texture_access(b->tile, GPU_ACCESS_SHADER_WRITE);
  if (r32f) {
    gpu_apply_compute_pipeline(b->kernels[1]);
    shader_cod_post_tile_r32f_apply_params(&(ShaderCodPostTileR32fParams){.size = {(f32)b->tile_w, (f32)b->tile_h, band.band_min, 0.0f}});
    shader_cod_post_tile_r32f_apply_bindings(
        (ShaderCodPostTileR32fBindings){.tile_out = b->tile, .velocity_tex = ctx->velocity, .depth_tex = depth});
  } else {
    gpu_apply_compute_pipeline(b->kernels[0]);
    shader_cod_post_tile_apply_params(&(ShaderCodPostTileParams){.size = {(f32)b->tile_w, (f32)b->tile_h, band.band_min, 0.0f}});
    shader_cod_post_tile_apply_bindings(
        (ShaderCodPostTileBindings){.tile_out = b->tile, .velocity_tex = ctx->velocity, .depth_tex = depth});
  }
  gpu_dispatch(cod_blur_groups(b->tile_w), cod_blur_groups(b->tile_h), 1);
  gpu_texture_access(b->tile, GPU_ACCESS_SHADER_READ);

  gpu_texture_access(b->out, GPU_ACCESS_SHADER_WRITE);
  v4 shot = {shutter, COD_BLUR_RADIUS_PX, (f32)(ctx->frame_index & 63u), COD_BLUR_INTENSITY};
  v4 size = {(f32)b->out_w, (f32)b->out_h, (f32)ctx->width, (f32)ctx->height};
  v4 dp = {depth_b, depth_a, band.band_min, 0.0f};
  if (r32f) {
    gpu_apply_compute_pipeline(b->kernels[3]);
    shader_cod_post_blur_r32f_apply_params(&(ShaderCodPostBlurR32fParams){.shot = shot, .size = size, .depth = dp});
    shader_cod_post_blur_r32f_apply_bindings((ShaderCodPostBlurR32fBindings){.color_out = b->out,
                                                                             .color_tex = ctx->resolved_color,
                                                                             .velocity_tex = ctx->velocity,
                                                                             .tile_tex = b->tile,
                                                                             .depth_tex = depth});
  } else {
    gpu_apply_compute_pipeline(b->kernels[2]);
    shader_cod_post_blur_apply_params(&(ShaderCodPostBlurParams){.shot = shot, .size = size, .depth = dp});
    shader_cod_post_blur_apply_bindings((ShaderCodPostBlurBindings){.color_out = b->out,
                                                                    .color_tex = ctx->resolved_color,
                                                                    .velocity_tex = ctx->velocity,
                                                                    .tile_tex = b->tile,
                                                                    .depth_tex = depth});
  }
  gpu_dispatch(cod_blur_groups(b->out_w), cod_blur_groups(b->out_h), 1);
  gpu_texture_access(b->out, GPU_ACCESS_SHADER_READ);
  renderer_set_post_color(r, b->out);
}

hz_internal void cod_blur_resize(RendererState *r, const RendererResizeCtx *ctx, void *user) {
  UNUSED(r);
  CodBlur *b = (CodBlur *)user;
  cod_blur_make_targets(b, ctx->render_width, ctx->render_height, ctx->display_width, ctx->display_height);
}

// a cut or a resize drops the previous frame's time so the first dt is the nominal one
hz_internal void cod_blur_reset(RendererState *r, RendererResetReason reason, void *user) {
  UNUSED(r);
  UNUSED(reason);
  ((CodBlur *)user)->have_time = false;
}

hz_internal void cod_blur_destroy(RendererState *r, void *user) {
  UNUSED(r);
  cod_blur_destroy_targets((CodBlur *)user);
}

// main lane, after renderer init: the kernels, the targets and the feature
hz_internal CodBlur *cod_blur_init(RendererState *r, Allocator *alloc) {
  CodBlur *b = ALLOC(alloc, CodBlur);
  b->kernels[0] = gpu_make_compute_pipeline_async(shader_cod_post_tile_compute_pipeline_desc());
  b->kernels[1] = gpu_make_compute_pipeline_async(shader_cod_post_tile_r32f_compute_pipeline_desc());
  b->kernels[2] = gpu_make_compute_pipeline_async(shader_cod_post_blur_compute_pipeline_desc());
  b->kernels[3] = gpu_make_compute_pipeline_async(shader_cod_post_blur_r32f_compute_pipeline_desc());
  u32 render_w = 0, render_h = 0, post_w = 0, post_h = 0;
  renderer_render_size(r, &render_w, &render_h);
  renderer_post_size(r, &post_w, &post_h);
  cod_blur_make_targets(b, render_w, render_h, post_w, post_h);
  b->feature = renderer_add_feature(r, &(RendererFeatureDesc){
                                           .name = "cod_motion_blur",
                                           .user = b,
                                           .record = {[RENDERER_PHASE_AFTER_RESOLVE] = cod_blur_record},
                                           .reads = RENDERER_RESOURCE_HDR_COLOR | RENDERER_RESOURCE_VELOCITY | RENDERER_RESOURCE_DEPTH,
                                           .resize = cod_blur_resize,
                                           .reset = cod_blur_reset,
                                           .destroy = cod_blur_destroy,
                                           .ready = cod_blur_ready,
                                           .order = RENDERER_ORDER_BUILTIN,
                                       });
  assert_msg(b->feature.id != 0, "cod_blur_init: the cod_motion_blur feature did not register");
  renderer_feature_require(r, b->feature, NULL, 0, b->kernels, ARRAY_SIZE(b->kernels));
  return b;
}
