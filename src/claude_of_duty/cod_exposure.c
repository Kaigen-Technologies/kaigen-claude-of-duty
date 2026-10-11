// Claude of Duty auto-exposure: the meter as a renderer feature in place of the engine's hz_exposure.
#include "shaders/cod_exposure.h"
#include "shaders/cod_exposure_r32f.h"

// the meter, sky knee and exposure parameters
#define COD_EXPOSURE_SKY_WEIGHT 0.15f
#define COD_EXPOSURE_FAR 400.0f
#define COD_EXPOSURE_TAP_CLAMP 40.0f
#define COD_EXPOSURE_KNEE_LO 0.06f
#define COD_EXPOSURE_KNEE_HI 0.3f
#define COD_EXPOSURE_SPEED_UP 1.4f
#define COD_EXPOSURE_SPEED_DOWN 3.2f
#define COD_EXPOSURE_MAX_DT 0.1f

typedef struct CodExposure {
  RendererFeature feature;
  GpuComputePipeline kernels[2]; // over the scene depth, over the upscaler's R32F depth
  GpuBuffer state;               // the engine's exposure state: [0] exposure, [1] EV100, [2] this frame's target EV100
  b32 reset;                     // the next dispatch snaps to its target
  b32 readback_pending;          // one state copy in flight: queued in the record, collected in a later prepare
  b32 have_time;
  b32 have_ev;
  f32 ev100; // the latest GPU EV100 read back, engine units
  f32 prev_time;
  f32 bias, key, min_ev, max_ev; // the look's, in scene units
} CodExposure;

// the latest GPU EV100 is the pre-exposure, re-applied every frame over the manual EV100 day_night_sync writes without hz_exposure
hz_internal void cod_exposure_prepare(RendererState *r, const RendererPrepareCtx *ctx, void *user) {
  UNUSED(ctx);
  CodExposure *e = (CodExposure *)user;
  if (e->readback_pending) {
    TempAllocator ta = tctx_temp_allocator_begin(NULL);
    f32 *state = ALLOC_ARRAY(&ta.allocator, f32, 4);
    if (gpu_get_buffer_read(e->state, 0, state, 4 * sizeof(f32))) {
      e->readback_pending = false;
      e->ev100 = state[1];
      e->have_ev = true;
    }
    tctx_temp_allocator_end(ta);
  }
  if (e->have_ev)
    renderer_set_exposure_ev100(r, e->ev100);
}

// after the temporal resolve, before DoF and bloom: meters the resolved frame
hz_internal void cod_exposure_record(RendererState *r, const RendererPassCtx *ctx, void *user) {
  CodExposure *e = (CodExposure *)user;
  b32 r32f = handle_is_valid(ctx->depth_r32f);
  GpuTexture depth = r32f ? ctx->depth_r32f : ctx->scene_depth;
  assert_msg(ctx->camera && handle_is_valid(depth), "cod_exposure: the DEPTH read guarantees a depth at AFTER_RESOLVE");
  f32 dt = e->have_time ? m_clampf(ctx->time - e->prev_time, 0.0f, COD_EXPOSURE_MAX_DT) : 0.0f;
  e->prev_time = ctx->time;
  e->have_time = true;
  f32 pre = renderer_pre_exposure(r);
  ShaderCodExposureParams p = {
      .meter = {COD_EXPOSURE_SKY_WEIGHT, COD_EXPOSURE_FAR, COD_EXPOSURE_TAP_CLAMP, 1.0f / (pre * (f32)COD_SCENE_LUX)},
      .knee = {COD_EXPOSURE_KNEE_LO, COD_EXPOSURE_KNEE_HI, ctx->camera->proj.m[3][2], ctx->camera->proj.m[2][2]},
      .adapt = {dt, COD_EXPOSURE_SPEED_UP, COD_EXPOSURE_SPEED_DOWN, e->bias},
      .limits = {e->min_ev, e->max_ev, e->reset ? 1.0f : 0.0f, e->key},
      .src = {(f32)ctx->width, (f32)ctx->height, m_log2f((f32)COD_SCENE_LUX), 0.0f},
  };
  e->reset = false;
  // last frame's final pass and readback copy read the state; the meter rewrites it
  gpu_buffer_access(e->state, GPU_ACCESS_SHADER_WRITE);
  if (r32f) {
    ShaderCodExposureR32fParams p32;
    static_assert(sizeof(p32) == sizeof(p), "the two kernels share one params block");
    mem_cpy(&p32, &p, sizeof(p));
    gpu_apply_compute_pipeline(e->kernels[1]);
    shader_cod_exposure_r32f_apply_params(&p32);
    shader_cod_exposure_r32f_apply_bindings(
        (ShaderCodExposureR32fBindings){.state = e->state, .color_tex = ctx->resolved_color, .depth_tex = depth});
  } else {
    gpu_apply_compute_pipeline(e->kernels[0]);
    shader_cod_exposure_apply_params(&p);
    shader_cod_exposure_apply_bindings(
        (ShaderCodExposureBindings){.state = e->state, .color_tex = ctx->resolved_color, .depth_tex = depth});
  }
  gpu_dispatch(1, 1, 1);
  gpu_buffer_access(e->state, GPU_ACCESS_SHADER_READ);
  if (!e->readback_pending) {
    gpu_queue_buffer_read(e->state);
    e->readback_pending = true;
  }
  ShaderHzExposureInput in = {.enabled = 1u, .inv_pre_exposure = 1.0f / pre};
  renderer_feature_publish_buffer(r, e->feature, "_HzExposureState", e->state);
  renderer_set_uniforms(r, e->feature, "_HzExposureInput", &in, sizeof(in));
}

// a cut snaps to the new view's exposure instead of adapting from the old one (exposure.reset on a resize does the same)
hz_internal void cod_exposure_reset(RendererState *r, RendererResetReason reason, void *user) {
  UNUSED(r);
  UNUSED(reason);
  CodExposure *e = (CodExposure *)user;
  e->reset = true;
  e->have_time = false;
}

// main lane, after renderer init: the meter's kernels, its state and the feature
CodExposure *cod_exposure_init(RendererState *r, Allocator *alloc) {
  CodExposure *e = ALLOC(alloc, CodExposure);
  e->reset = true;
  e->key = 1.0f;
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  e->state = gpu_make_buffer(&(GpuBufferDesc){.type = GPU_BUFFER_STRUCTURED,
                                              .usage = GPU_BUFFER_USAGE_GPU_WRITE,
                                              .size = 4 * sizeof(f32),
                                              .stride = sizeof(f32),
                                              .data = ALLOC_ARRAY(&ta.allocator, f32, 4),
                                              .allow_readback = true,
                                              .label = "cod exposure state"});
  tctx_temp_allocator_end(ta);
  e->kernels[0] = gpu_make_compute_pipeline_async(shader_cod_exposure_compute_pipeline_desc());
  e->kernels[1] = gpu_make_compute_pipeline_async(shader_cod_exposure_r32f_compute_pipeline_desc());
  e->feature = renderer_add_feature(r, &(RendererFeatureDesc){
                                           .name = "cod_exposure",
                                           .user = e,
                                           .prepare = cod_exposure_prepare,
                                           .record = {[RENDERER_PHASE_AFTER_RESOLVE] = cod_exposure_record},
                                           .reads = RENDERER_RESOURCE_HDR_COLOR | RENDERER_RESOURCE_DEPTH,
                                           .reset = cod_exposure_reset,
                                           .order = RENDERER_ORDER_BUILTIN + 1,
                                       });
  assert_msg(e->feature.id != 0, "cod_exposure_init: the cod_exposure feature did not register");
  renderer_feature_require(r, e->feature, NULL, 0, e->kernels, ARRAY_SIZE(e->kernels));
  return e;
}
