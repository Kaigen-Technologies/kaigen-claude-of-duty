// Claude of Duty contact shadows: a depth-buffer ray toward the key light, published to the COD surfaces' key term.
#include "shaders/cod_post_contact.h"
#include "shaders/cod_post_contact_blur.h"

// contact shadow ray length, thickness and strength
#define COD_CONTACT_LENGTH 0.4f
#define COD_CONTACT_STRENGTH 1.0f
#define COD_CONTACT_THICKNESS 0.42f

typedef struct CodContact {
  RendererFeature feature;
  GpuComputePipeline kernels[2]; // the march, the separable depth-weighted blur
  GpuTexture ping, pong;         // RG16F: x the shadow, y the view depth; the blur ends in ping
  u32 width, height;
  b32 enabled;
  v3 sun;      // toward the key light, world
  v4 block;    // xy 1 / size: the _CodContact block the surfaces read
} CodContact;

hz_internal void cod_contact_destroy_targets(CodContact *c) {
  if (handle_is_valid(c->ping))
    gpu_destroy_texture(c->ping);
  if (handle_is_valid(c->pong))
    gpu_destroy_texture(c->pong);
  c->ping = (GpuTexture){0};
  c->pong = (GpuTexture){0};
}

hz_internal void cod_contact_make_targets(RendererState *r, CodContact *c, u32 width, u32 height) {
  cod_contact_destroy_targets(c);
  c->width = MAX(width, 1u);
  c->height = MAX(height, 1u);
  GpuStorageTextureDesc desc = {.label = "cod contact shadows",
                                .kind = GPU_TEXTURE_KIND_2D,
                                .format = GPU_TEXTURE_FORMAT_RG16F,
                                .width = c->width,
                                .height = c->height};
  c->ping = gpu_make_storage_texture(&desc);
  c->pong = gpu_make_storage_texture(&desc);
  c->block = (v4){1.0f / (f32)c->width, 1.0f / (f32)c->height, 0.0f, 0.0f};
  // set here, not in prepare: prepare skips a feature that is not ready, and the surfaces bind the block regardless
  renderer_set_uniforms(r, c->feature, "_CodContact", &c->block, sizeof(c->block));
  renderer_feature_publish(r, c->feature, "cod_contact", c->ping);
}

hz_internal b32 cod_contact_ready(void *user) {
  CodContact *c = (CodContact *)user;
  return c->enabled && handle_is_valid(c->ping) && gpu_texture_is_ready(c->ping) && gpu_texture_is_ready(c->pong);
}

hz_internal void cod_contact_blur_pass(CodContact *c, GpuTexture dst, GpuTexture src, i32 dx, i32 dy) {
  gpu_texture_access(dst, GPU_ACCESS_SHADER_WRITE);
  gpu_apply_compute_pipeline(c->kernels[1]);
  shader_cod_post_contact_blur_apply_params(
      &(ShaderCodPostContactBlurParams){.size = {(f32)c->width, (f32)c->height, (f32)dx, (f32)dy}});
  shader_cod_post_contact_blur_apply_bindings((ShaderCodPostContactBlurBindings){.blur_out = dst, .src_tex = src});
  gpu_dispatch((c->width + 7u) / 8u, (c->height + 7u) / 8u, 1);
  gpu_texture_access(dst, GPU_ACCESS_SHADER_READ);
}

// between the pre-pass and the lit pass: the march, then the horizontal and vertical blur
hz_internal void cod_contact_record(RendererState *r, const RendererPassCtx *ctx, void *user) {
  CodContact *c = (CodContact *)user;
  if (!ctx->camera || !handle_is_valid(ctx->scene_depth) || !handle_is_valid(ctx->normals))
    return;
  const Camera *cam = ctx->camera;
  PostProjectionBand band = renderer_projection_band(r);
  ShaderCodPostContactParams p = {
      .view = cam->view,
      .sun = {c->sun.x, c->sun.y, c->sun.z, 0.0f},
      .proj = {1.0f / cam->proj.m[0][0], 1.0f / cam->proj.m[1][1], cam->proj.m[3][2], cam->proj.m[2][2]},
      .size = {(f32)c->width, (f32)c->height, 1.0f / (f32)c->width, 1.0f / (f32)c->height},
      .shot = {COD_CONTACT_LENGTH, COD_CONTACT_THICKNESS, (f32)(ctx->frame_index & 63u), COD_CONTACT_STRENGTH},
      .band = {band.band_min, 0.0f, 0.0f, 0.0f},
  };
  gpu_texture_access(c->ping, GPU_ACCESS_SHADER_WRITE);
  gpu_apply_compute_pipeline(c->kernels[0]);
  shader_cod_post_contact_apply_params(&p);
  shader_cod_post_contact_apply_bindings(
      (ShaderCodPostContactBindings){.contact_out = c->ping, .depth_tex = ctx->scene_depth, .normal_tex = ctx->normals});
  gpu_dispatch((c->width + 7u) / 8u, (c->height + 7u) / 8u, 1);
  gpu_texture_access(c->ping, GPU_ACCESS_SHADER_READ);
  cod_contact_blur_pass(c, c->pong, c->ping, 1, 0);
  cod_contact_blur_pass(c, c->ping, c->pong, 0, 1);
}

hz_internal void cod_contact_resize(RendererState *r, const RendererResizeCtx *ctx, void *user) {
  cod_contact_make_targets(r, (CodContact *)user, ctx->render_width, ctx->render_height);
}

hz_internal void cod_contact_destroy(RendererState *r, void *user) {
  UNUSED(r);
  cod_contact_destroy_targets((CodContact *)user);
}

// main lane, after renderer init: the kernels, the targets and the feature, whose output the COD surfaces' contact field samples
hz_internal CodContact *cod_contact_init(RendererState *r, Allocator *alloc) {
  CodContact *c = ALLOC(alloc, CodContact);
  c->kernels[0] = gpu_make_compute_pipeline_async(shader_cod_post_contact_compute_pipeline_desc());
  c->kernels[1] = gpu_make_compute_pipeline_async(shader_cod_post_contact_blur_compute_pipeline_desc());
  c->sun = (v3){0.0f, 1.0f, 0.0f};
  c->feature = renderer_add_feature(r, &(RendererFeatureDesc){
                                           .name = "cod_contact",
                                           .user = c,
                                           .record = {[RENDERER_PHASE_AFTER_DEPTH_PREPASS] = cod_contact_record},
                                           .reads = RENDERER_RESOURCE_DEPTH | RENDERER_RESOURCE_NORMALS,
                                           .resize = cod_contact_resize,
                                           .destroy = cod_contact_destroy,
                                           .ready = cod_contact_ready,
                                           .outputs = {{.name = "cod_contact", .kind = MAT_TEXTURE_2D, .neutral = TEX_DEFAULT_WHITE}},
                                           .order = RENDERER_ORDER_BUILTIN,
                                       });
  assert_msg(c->feature.id != 0, "cod_contact_init: the cod_contact feature did not register");
  renderer_feature_require(r, c->feature, NULL, 0, c->kernels, ARRAY_SIZE(c->kernels));
  u32 render_w = 0, render_h = 0;
  renderer_render_size(r, &render_w, &render_h);
  cod_contact_make_targets(r, c, render_w, render_h);
  return c;
}
