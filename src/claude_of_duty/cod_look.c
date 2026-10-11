// Claude of Duty look: the day_night block's physical sky, the budgets over it, fog, exposure, post and quality presets.

#define COD_EXPOSURE_KEY 1.06f
#define COD_MAX_LIGHTS 256
#define COD_FINAL_MAT "claude_of_duty/materials/cod_final.hzt"
// the sky material for the renderer's output mode, as the engine picks its own
#define COD_SKY_MAT "claude_of_duty/materials/cod_sky.hzt"
#define COD_SKY_MAT_GBUFFER "claude_of_duty/materials/cod_sky_gbuffer.hzt"
#define COD_SKY_MAT_TAAU "claude_of_duty/materials/cod_sky_gbuffer_taau.hzt"
// the exposure limits (-4.3, 20), in framebuffer units
#define COD_EXPOSURE_MIN_EV -4.3f
#define COD_EXPOSURE_MAX_EV 20.0f
// fog: extinction at the base, e-folding height, base, phase lobes folded to one
#define COD_FOG_EXTINCTION 1.45e-3f
#define COD_FOG_HEIGHT_SCALE 18.0f
#define COD_FOG_BASE_Y (-2.0f)
#define COD_FOG_MAX_DISTANCE 900.0f
#define COD_FOG_ANISOTROPY (0.66f * 0.76f - 0.34f * 0.36f)
#define COD_AO_RADIUS 1.35f
#define COD_AO_INTENSITY 1.1f
#define COD_CSM_LAMBDA 0.86f

// the look's main-lane state
typedef struct CodLookState {
  CodSky sky;
  v3 sun_applied, moon_applied;
  RendererFeature feature;
  CodExposure *exposure;
  CodPost *post;
  CodLookBlock block; // lux; the prepare pre-exposes the irradiances
  i32 quality_applied;
  EcsQuery *camera_query;
  b32 rooms_ready;
  HzHandleT(MaterialBlobAsset) final_asset;
  HzHandleT(RendererMaterial) final_material; // the composite, in place of the engine's final grade
  GpuTexture grade_lut;
  f32 ads;         // 0 hip .. 1 aimed: the vignette tightens toward the ADS vignette
  f32 ads_applied; // -1 until the first write
  v4 hurt;         // the low-health treatment: x amount, y pulse, z hit flash, w critical
  HzHandleT(MaterialBlobAsset) sky_asset;
  HzHandleT(RendererMaterial) sky_material; // the dome over the engine atmosphere
  CodSkyBlock sky_block;                   // lux; the shader pre-exposes
} CodLookState;
ECS_COMPONENT_TYPEDEF(CodLookState);

hz_internal v4 cod_v4(v3 v, f32 w) { return (v4){v.x, v.y, v.z, w}; }

void cod_look_renderer_desc(RendererInitDesc *desc) {
  CodQualityPreset q = cod_quality_preset((u32)cod_quality_default());
  desc->sample_count = 1;
  desc->gbuffer_enabled = true;
  desc->upscale = (RendererUpscaleDesc){.enabled = true, .quality = q.upscale};
  desc->depth_prepass = true;
  desc->prepass_normals = true;
  desc->shadows_enabled = true;
  desc->shadow = (ShadowInitDesc){.cascade_size = q.shadow_map_size, .cascade_count = q.cascades, .max_distance = q.shadow_distance};
  desc->gtao_enabled = true;
  desc->gtao = (GtaoInitDesc){.radius = COD_AO_RADIUS};
  desc->ssr_enabled = true;
  desc->bloom_enabled = true;
  desc->dof_enabled = true;
  // the meter owns the exposure in place of hz_exposure
  desc->auto_exposure_enabled = false;
  // clustered local lights: the practicals and the FX flashes (without it every EcsPointLight is dropped)
  desc->max_lights = COD_MAX_LIGHTS;
  // the cloud decks draw in the COD sky material, not the engine clouds
  desc->clouds_enabled = false;
  desc->height_fog_enabled = true;
  desc->height_fog = height_fog_default_params();
}

// a dev shot or switch pins the block's clock; the block's moon is lit by its illuminated fraction, not the phase law
ecs_system(cod_look_time_system, EcsInOut(DayNightCycle), EcsResIn(CodDev)) {
  const CodDev *dev = ecs_sys_get_resource(CodDev);
  for (i32 i = 0; i < it->count; i++) {
    const DayNightCycle *c = ecs_sys_get_component(DayNightCycle, i);
    if (dev->time_of_day >= 0.0f && c->time_of_day != dev->time_of_day)
      ecs_sys_get_component_mut(DayNightCycle, i)->time_of_day = dev->time_of_day;
    DayNightBodies bodies = day_night_astronomy(c);
    f32 lit = 0.5f * (1.0f - m_clampf(v3_dot(bodies.sun_dir, bodies.moon_dir), -1.0f, 1.0f));
    f32 lux = (f32)(COD_MOON_ILLUMINANCE_NIGHT * COD_SCENE_LUX) * lit / m_maxf(bodies.moon_brightness, 1e-4f);
    if (m_absf(c->physical_moon_illuminance - lux) > lux * 1e-3f)
      ecs_sys_get_component_mut(DayNightCycle, i)->physical_moon_illuminance = lux;
  }
}

// the block's sun and moon through the photometry; publishes CodLook
ecs_system(cod_look_sky_system, EcsResIn(DayNightState), EcsResIn(CodSettings), EcsResIn(CodClock), EcsResIn(CodWorld),
           EcsResInOut(CodLook), EcsResInOut(CodLookState)) {
  const DayNightState *dn = ecs_sys_get_resource(DayNightState);
  if (!dn->ready)
    return;
  CodLook *look = ecs_sys_get_resource_mut(CodLook);
  CodLookState *ls = ecs_sys_get_resource_mut(CodLookState);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  if (!v3_eqv(dn->sun_dir, ls->sun_applied) || !v3_eqv(dn->moon_dir, ls->moon_applied)) {
    cod_sky_set_bodies(&ls->sky, dn->sun_dir, dn->moon_dir);
    ls->sun_applied = dn->sun_dir;
    ls->moon_applied = dn->moon_dir;
  }
  // a cloud crossing the sun, eased hard, from last frame's camera
  v3 eye = ecs_sys_get_resource(CodWorld)->cull_eye;
  f64 occ = cod_cloud_sun_occlusion(eye.x, eye.z, dn->sun_dir, clock->time);
  f64 eased = ls->sky.cloud_occlusion + (occ - ls->sky.cloud_occlusion) * m_min(1.0, (f64)clock->dt * 0.9);
  if (m_abs(eased - ls->sky.cloud_occlusion) > 1e-5) {
    ls->sky.cloud_occlusion = eased;
    cod_sky_apply_intensities(&ls->sky);
  }
  const CodSky *s = &ls->sky;
  f64 wind_x = m_cos(COD_WIND_ANGLE) * COD_WIND_SPEED, wind_z = m_sin(COD_WIND_ANGLE) * COD_WIND_SPEED;
  f32 top = (f32)(COD_SUN_ILLUMINANCE_TOP * COD_SCENE_LUX), moon_irr = (f32)(s->moon_irradiance * COD_SCENE_LUX);
  ls->sky_block = (CodSkyBlock){
      .sun = cod_v4(dn->sun_dir, (f32)(s->rolloff_knee * COD_SCENE_LUX)),
      .sun_irradiance = {top * (f32)COD_SUN_TINT_R, top * (f32)COD_SUN_TINT_G, top * (f32)COD_SUN_TINT_B, 0.34f},
      .moon = cod_v4(dn->moon_dir, (f32)COD_HORIZON_MURK),
      .moon_irradiance = {moon_irr * 0.66f, moon_irr * 0.80f, moon_irr, (f32)(COD_ATMO_VIEW_ALT * 1000.0)},
      .clouds = {(f32)COD_CLOUD_COVERAGE, (f32)COD_CLOUD_DENSITY, 1.0f, clock->time},
      .clouds2 = {(f32)COD_CIRRUS_COVERAGE, (f32)COD_CIRRUS_OPACITY, (f32)wind_x, (f32)wind_z}};
  look->time_of_day = dn->time_of_day;
  look->sun_dir = dn->sun_dir;
  look->moon_dir = dn->moon_dir;
  look->sun_altitude = (f32)s->sun_alt;
  look->key_dir = dn->key_dir;
  look->key_is_moon = dn->key_is_moon;
  v3 key = v3_scale(s->key_irradiance, (f32)COD_SCENE_LUX);
  look->key_color = (ColorF32){key.x, key.y, key.z, 1.0f};
  v3 amb = v3_scale(s->ambient, (f32)COD_SCENE_LUX);
  look->ambient = (ColorF32){amb.x, amb.y, amb.z, 1.0f};
  look->night = (f32)s->night_ramp;
  look->exposure_bias = (f32)s->exposure_bias;
  look->indirect_scale = (f32)s->indirect_scale;
  look->scene_per_unit = (f32)COD_SCENE_LUX;
  look->view_key = cod_view_rig(s).key_intensity;
  look->quality = (u32)ecs_sys_get_resource(CodSettings)->quality;
}

// the engine bloom the COD final composites; the lens and grade are the final's
ecs_system(cod_look_post_system, EcsInOut(EcsPostProcessing)) {
  for (i32 i = 0; i < it->count; i++) {
    EcsPostProcessing t = *ecs_sys_get_component(EcsPostProcessing, i);
    t.tonemap_mode = TONEMAP_AGX;
    t.exposure = 1.0f;
    t.bloom_intensity = 0.14f;
    t.bloom_threshold = 1.6f;
    t.bloom_tint = (ColorF32){1.0f, 1.0f, 1.0f, 1.0f};
    t.chromatic_aberration = 0.0f;
    t.grain = 0.0f;
    t.dither = 0.0f;
    if (mem_cmp(&t, ecs_sys_get_component(EcsPostProcessing, i), sizeof(t)) != 0)
      *ecs_sys_get_component_mut(EcsPostProcessing, i) = t;
  }
}

// the renderer's pipelines into the load gate
ecs_system(cod_look_load_system, EcsResInOut(CodLoad)) {
  const RendererState *r = ecs_resource_get(it->world, RendererState);
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_PIPELINES] = renderer_pipelines_ready(r) ? 1.0f : 0.0f;
}

static_assert(COD_LOOK_VIEW_FLASHES == COD_FX_VIEW_FLASHES, "the look block carries the whole view flash pool");

hz_internal v4 cod_look_pre_expose(v4 c, f32 pre) { return (v4){c.x * pre, c.y * pre, c.z * pre, c.w}; }

// this frame's pre-exposure on the look block the COD surfaces read
hz_internal void cod_look_prepare(RendererState *r, const RendererPrepareCtx *ctx, void *user) {
  UNUSED(ctx);
  CodLookState *ls = (CodLookState *)user;
  CodLookBlock b = ls->block;
  f32 pre = renderer_pre_exposure(r);
  b.sky_fill = cod_v4(v3_scale((v3){b.sky_fill.x, b.sky_fill.y, b.sky_fill.z}, pre), b.sky_fill.w);
  b.ground_fill = cod_v4(v3_scale((v3){b.ground_fill.x, b.ground_fill.y, b.ground_fill.z}, pre), b.ground_fill.w);
  b.view_key = cod_look_pre_expose(b.view_key, pre);
  b.view_fill = cod_look_pre_expose(b.view_fill, pre);
  b.view_rim = cod_look_pre_expose(b.view_rim, pre);
  b.view_bounce = cod_look_pre_expose(b.view_bounce, pre);
  b.view_hemi_sky = cod_look_pre_expose(b.view_hemi_sky, pre);
  b.view_hemi_ground = cod_look_pre_expose(b.view_hemi_ground, pre);
  for (u32 i = 0; i < COD_LOOK_VIEW_FLASHES; i++)
    b.view_flash_color[i] = cod_look_pre_expose(b.view_flash_color[i], pre);
  renderer_set_uniforms(r, ls->feature, "_CodLook", &b, sizeof(b));
  renderer_set_uniforms(r, ls->feature, "_CodSky", &ls->sky_block, sizeof(ls->sky_block));
}

void cod_look_init(EcsWorld *w, Allocator *alloc) {
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  day_night_init(w, r, alloc, DAY_NIGHT_SKY_PHYSICAL);
  // Hillaire's Earth with the aerosol scaled by the weather's turbidity, its Mie absorption and ground albedo
  SkyAtmosphereParams atmo = sky_atmosphere_earth_params();
  f32 mie_s = 0.003996f * (f32)COD_TURBIDITY, mie_a = 0.0044f * (f32)COD_TURBIDITY;
  atmo.mie_scattering = (v3){mie_s, mie_s, mie_s};
  atmo.mie_absorption = (v3){mie_a, mie_a, mie_a};
  atmo.ground_albedo = (v3){0.24f, 0.24f, 0.24f};
  renderer_set_sky_atmosphere(r, &atmo);
  // metals reflect the sky on Low and Medium too
  renderer_sky_atmosphere_set_capture_always(r, true);
  ecs_resource_set(w, CodLook, ((CodLook){.quality = (u32)cod_quality_default(), .scene_per_unit = (f32)COD_SCENE_LUX}));
  CodLookState *ls = ALLOC(alloc, CodLookState);
  ls->quality_applied = -1;
  ls->ads_applied = -1.0f;
  ls->final_asset = asset_load(MaterialBlobAsset, ecs_resource_get_mut(w, AssetSystem), .path = COD_FINAL_MAT);
  ls->grade_lut = cod_grade_lut_make();
  ls->sky.cloud_occlusion = 1.0;
  CString sky_path = renderer_taau_active(r) ? COD_SKY_MAT_TAAU : renderer_gbuffer_active(r) ? COD_SKY_MAT_GBUFFER : COD_SKY_MAT;
  ls->sky_asset = asset_load(MaterialBlobAsset, ecs_resource_get_mut(w, AssetSystem), .path = sky_path);
  ls->camera_query = ecs_query_cached(w, EcsIn(Camera));
  ls->exposure = cod_exposure_init(r, alloc);
  ls->post = cod_post_init(w, r, alloc, ls->exposure->state);
  ls->feature = renderer_add_feature(r, &(RendererFeatureDesc){.name = "cod_look", .user = ls, .prepare = cod_look_prepare});
  assert_msg(ls->feature.id != 0, "cod_look_init: the cod_look feature did not register");
  ecs_resource_set_ref(w, CodLookState, ls);
  // static level, moving camera: centred cascades rebuild only on anchor moves
  renderer_shadows_set_centred(r, 0.25f);
}

// the player's aim blend, which tightens the final's vignette, and the low-health treatment
ecs_system(cod_look_ads_system, EcsResIn(CodPlayerState), EcsResInOut(CodLookState)) {
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  CodLookState *ls = ecs_sys_get_resource_mut(CodLookState);
  ls->ads = m_clampf(ps->ads, 0.0f, 1.0f);
  ls->hurt = (v4){ps->low_effect, ps->heartbeat_pulse, ps->hit_flash, ps->critical_health ? 1.0f : 0.0f};
}

void cod_look_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_look_ads_system);   // writes the CodLookState resource
  ecs_schedule(w, cod_look_time_system);
  day_night_schedule(w);
  ecs_schedule_single(w, cod_look_sky_system); // writes the CodLook resource
  ecs_schedule(w, cod_look_post_system);
  ecs_schedule_single(w, cod_look_load_system); // writes the CodLoad resource
}

// split i of n as a fraction of the shadow distance, lambda 0.86 between log and uniform; 0 past the last split
hz_internal f32 cod_csm_split(u32 i, u32 cascades, f32 dist) {
  if (i >= cascades)
    return 0.0f;
  f32 n = COD_CAMERA_NEAR, p = (f32)i / (f32)cascades;
  return (COD_CSM_LAMBDA * n * m_powf(dist / n, p) + (1.0f - COD_CSM_LAMBDA) * (n + (dist - n) * p)) / dist;
}

// the quality preset's renderer level, upscale tier, AO, SSR, bloom and cascades
hz_internal void cod_look_apply_quality(RendererState *r, u32 quality) {
  CodQualityPreset q = cod_quality_preset(quality);
  renderer_set_quality_with_upscale(r, (RendererQuality)MIN(quality, (u32)RENDERER_QUALITY_ULTRA), q.upscale);
  hz_set_max_dpr(q.max_dpr);
  RendererGtaoParams ao = renderer_get_gtao(r);
  ao.enabled = q.gtao;
  ao.radius = COD_AO_RADIUS;
  ao.intensity = COD_AO_INTENSITY;
  // the AO's micro-shadowing share on the direct light
  ao.direct_strength = 0.35f;
  // Ultra keeps High's AO (3x3 slices at half rate): the 9x3 full-rate estimator adds ~1.4 ms on an iGPU for no visible change
  if (ao.quality > GTAO_QUALITY_HIGH) {
    ao.quality = GTAO_QUALITY_HIGH;
    ao.resolution = GTAO_RESOLUTION_HALF;
  }
  renderer_set_gtao(r, &ao);
  renderer_height_fog_set_enabled(r, true);
  renderer_ssr_set_enabled(r, q.ssr);
  // High and Ultra trace Hi-Z: 32 iterations reach every hit at the 2/3 TAAU size (64 measured identical, 0.1 ms slower); the DDA tiers keep the default
  SsrParams ssr = renderer_get_ssr(r);
  ssr.max_steps = quality >= COD_QUALITY_HIGH ? 32u : ssr_default_params().max_steps;
  renderer_set_ssr(r, &ssr);
  renderer_bloom_set_enabled(r, q.bloom);
  v3 split = {cod_csm_split(1, q.cascades, q.shadow_distance), cod_csm_split(2, q.cascades, q.shadow_distance),
              cod_csm_split(3, q.cascades, q.shadow_distance)};
  renderer_shadows_set_scene(r, (ColorF32){0}, q.cascades, split, q.shadow_distance);
}

void cod_look_sync(EcsWorld *w) {
  // the sky, fog, exposure and quality are main-lane renderer calls outside the frame
  assert(is_main_thread());
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  day_night_sync(w, r);
  CodLookState *ls = ecs_resource_get_mut(w, CodLookState);
  const CodLook *look = ecs_resource_get(w, CodLook);
  const DayNightState *dn = ecs_resource_get(w, DayNightState);
  const CodSky *s = &ls->sky;
  if (!dn->ready)
    return;

  // the sky: created once its material loads, depth-tested against a matching pre-pass so it shades only its own pixels
  if (!handle_is_valid(HANDLE_CAST(HzHandle, ls->sky_material))) {
    AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
    if (asset_is_ready(assets, ls->sky_asset)) {
      MaterialBlobAsset *blob = asset_get(assets, ls->sky_asset, MaterialBlobAsset);
      RendererPassProfile prepass = renderer_pass_profile(r, RENDERER_PHASE_DEPTH_PREPASS);
      b32 depth_test = prepass.has_depth &&
                       prepass.sample_count == renderer_pass_profile(r, RENDERER_PHASE_HDR_BACKGROUND).sample_count;
      ls->sky_material = renderer_create_material_from_embedded_ex(
          r, "cod_sky", asset_get(assets, blob->variant_handle, ShaderVariantBlobAsset),
          asset_get(assets, blob->material_shader_handle, MaterialShaderBlobAsset),
          (ShaderPipelineStateBlob){.cull_mode = GPU_CULL_NONE, .depth_test = depth_test, .depth_write = false});
      renderer_sky_atmosphere_set_material(r, ls->sky_material);
    }
  }

  // the composite final: created once its material loads, the vignette rewritten as the aim moves
  if (!handle_is_valid(HANDLE_CAST(HzHandle, ls->final_material))) {
    AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
    if (asset_is_ready(assets, ls->final_asset)) {
      HzHandleT(RendererMaterial) m = renderer_create_material_from_asset(r, assets, asset_get(assets, ls->final_asset, MaterialBlobAsset));
      material_set_texture(r, m, "look_lut", ls->grade_lut);
      material_set_vec4(r, m, "grade", (v4){COD_LUT_STRENGTH, COD_SHARPEN, (f32)COD_LUT_SIZE, 0.0f});
      material_set_vec4(r, m, "look", (v4){COD_AGX_SLOPE, COD_AGX_POWER, COD_AGX_SAT, 0.0f});
      renderer_set_final_material(r, m);
      ls->final_material = m;
    }
  }
  if (handle_is_valid(HANDLE_CAST(HzHandle, ls->final_material)) && ls->ads != ls->ads_applied) {
    f32 vignette = COD_VIGNETTE + (COD_ADS_VIGNETTE - COD_VIGNETTE) * ls->ads;
    material_set_vec4(r, ls->final_material, "lens", (v4){COD_CHROMATIC, vignette, COD_GRAIN, 1.0f / (f32)COD_SCENE_LUX});
    ls->ads_applied = ls->ads;
  }

  if ((i32)look->quality != ls->quality_applied) {
    cod_look_apply_quality(r, look->quality);
    ls->quality_applied = (i32)look->quality;
  }
  cod_post_sync(ls->post, look, ls->hurt);

  // the key (aureole-shifted beam, floored, key gain) over the block's physical key at the camera
  const Camera *cam = ecs_query_get_singleton(Camera, ls->camera_query);
  v3 te = renderer_sky_atmosphere_sun_transmittance(r, cam ? cam->pos.y : 0.0f, dn->key_dir);
  ColorF32 kc = dn->mood.light_color;
  f32 key_units = dn->key_lux / (f32)COD_SCENE_LUX;
  v3 phys = {key_units * srgb_to_linear(kc.r) * te.x, key_units * srgb_to_linear(kc.g) * te.y,
             key_units * srgb_to_linear(kc.b) * te.z};
  v3 beam = s->key_irradiance;
  v3 gain = {phys.x > 1e-6f ? m_minf(beam.x / phys.x, 64.0f) : 0.0f, phys.y > 1e-6f ? m_minf(beam.y / phys.y, 64.0f) : 0.0f,
             phys.z > 1e-6f ? m_minf(beam.z / phys.z, 64.0f) : 0.0f};

  // the fill bands and budgets, lux
  v3 sky_fill, ground_fill;
  cod_sky_fill(s, &sky_fill, &ground_fill);
  CodLookBlock *b = &ls->block;
  b->key_gain = cod_v4(gain, 0.35f);
  b->sky_fill = cod_v4(v3_scale(sky_fill, (f32)COD_SCENE_LUX), 1.0f);
  b->ground_fill = cod_v4(v3_scale(ground_fill, (f32)COD_SCENE_LUX), (f32)(COD_BOUNCE_FILL / COD_GROUND_FILL));
  b->fill_dir = (v4){-0.95f, 0.85f, -0.05f, 0.7f};
  b->indirect.x = (f32)(COD_IBL_DIFFUSE * s->indirect_scale);
  b->indirect.y = (f32)COD_INTERIOR_INDIRECT;
  b->indirect.w = 0.25f;
  b->sun_dir = cod_v4(s->sun, (f32)s->night_ramp);
  CodViewRig rig = cod_view_rig(s);
  f32 lux = (f32)COD_SCENE_LUX;
  b->view_key = cod_v4(v3_scale(rig.key, lux), COD_VIEW_FILL_OCCLUSION);
  b->view_fill = cod_v4(v3_scale(rig.fill, lux), COD_VIEW_ENV_OCCLUSION);
  b->view_rim = cod_v4(v3_scale(rig.rim, lux), 0.0f);
  b->view_bounce = cod_v4(v3_scale(rig.bounce, lux), 0.0f);
  b->view_hemi_sky = cod_v4(v3_scale(rig.hemi_sky, lux), 0.0f);
  b->view_hemi_ground = cod_v4(v3_scale(rig.hemi_ground, lux), 0.0f);
  const CodFxViewFlash *flash = ecs_resource_get(w, CodFxViewFlash);
  for (u32 i = 0; i < COD_LOOK_VIEW_FLASHES; i++) {
    const CodFxViewLight *l = &flash->lights[i];
    b->view_flash[i] = cod_v4(l->position, m_maxf(l->distance, 1e-3f));
    b->view_flash_color[i] = cod_v4(v3_scale(l->color, l->intensity * lux), 0.0f);
  }
  if (b->room_xf.x == 0.0f && b->room_xf.y == 0.0f) {
    // world -> level 2D: the inverse of cod_level_to_world's rotation and offset
    v3 o = cod_level_to_world((v3){0, 0, 0}), ex = cod_level_to_world((v3){1, 0, 0});
    f32 c = ex.x - o.x, sn = ex.z - o.z, inv = 1.0f / m_maxf(1e-6f, m_sqrtf(c * c + sn * sn));
    f32 cs = c * inv, sni = sn * inv;
    b->room_xf = (v4){cs, sni, -(o.x * cs + o.z * sni), -(-o.x * sni + o.z * cs)};
  }
  // the indirect gate's volumes: enterable, roofed footprints, once the world exists
  const CodWorld *world = ecs_resource_get(w, CodWorld);
  if (!ls->rooms_ready && world->ready) {
    ls->rooms_ready = true;
    u32 n = 0;
    for (u32 i = 0; i < world->building_count && n < COD_LOOK_ROOMS; i++) {
      const CodBuildingInfo *bi = &world->buildings[i];
      if (!bi->enterable || bi->collapsed)
        continue;
      b->rooms[n] = (v4){bi->level_rect.x, bi->level_rect.y, bi->level_rect.z * 0.5f, bi->level_rect.w * 0.5f};
      b->rooms_y[n] = (v4){-0.8f, bi->room_top, 0.0f, 0.0f};
      n++;
    }
    b->indirect.z = (f32)n;
    log_info("look: indirect gate over % interior volumes", fmt_u32(n));
  }

  // fog: the medium; the key's shafts come from the engine's main light
  HeightFogParams fog = renderer_get_height_fog(r);
  HeightFogParams cur = fog;
  fog.density = COD_FOG_EXTINCTION;
  fog.base_height = COD_FOG_BASE_Y;
  fog.height_falloff = 1.0f / COD_FOG_HEIGHT_SCALE;
  fog.anisotropy = COD_FOG_ANISOTROPY;
  fog.max_distance = COD_FOG_MAX_DISTANCE;
  if (mem_cmp(&cur, &fog, sizeof(fog)) != 0)
    renderer_set_height_fog(r, &fog);

  // exposure: the window, key and EV bias (settings plus the sky's), in its units; the meter reads them at its record
  ls->exposure->bias = look->exposure_bias;
  ls->exposure->key = COD_EXPOSURE_KEY;
  ls->exposure->min_ev = COD_EXPOSURE_MIN_EV;
  ls->exposure->max_ev = COD_EXPOSURE_MAX_EV;
}
