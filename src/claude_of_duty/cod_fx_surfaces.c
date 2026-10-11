// Claude of Duty effects: one impact effect per surface.

#define CF_P(plan, d, ...) cod_fx_plan_part(plan, d, &(CodFxPart)__VA_ARGS__)

// the three dust bands of a masonry hit: a fast small cough, the body and a slow veil; each has its own growth exponent and ignition delay
typedef struct CodFxBand {
  f32 count;
  v2 life, delay, drag, speed, size0, size1, alpha;
  f32 size_curve;
} CodFxBand;

hz_internal CodFxPart cod_fx_band_part(const CodFxBand *b, u32 tile, u32 tile2, f32 tile2_frac, v3 c0, v3 c1, f32 spread, f32 power, f32 sun_gain,
                                       f32 alpha_curve, f32 spin) {
  CodFxPart p = cod_fx_puff(tile, b->count, b->life, b->size0, b->size1, c0, c1, b->alpha, alpha_curve);
  p.tile2 = tile2;
  p.tile2_frac = tile2_frac;
  p.delay = b->delay;
  p.drag = b->drag;
  p.speed = b->speed;
  p.size_curve = b->size_curve;
  p.gravity = -0.7f;
  p.spin = (v2){-spin, spin};
  p.axis = CF_AXIS_ALT;
  p.spread = spread;
  p.power = power;
  p.hemi = 0.05f;
  p.vel_add = (v3){0.0f, 0.45f, 0.0f};
  p.noff = (v2){0.05f, 0.16f};
  p.disc = 0.09f;
  p.soft = 0.09f;
  p.turb = 0.05f;
  p.turb_freq = 2.4f;
  p.sun_base = 0.68f;
  p.sun_gain = sun_gain;
  return p;
}

/* concrete, brick, stone: pale dust, a directional jet, spall chips, flint sparks and a wisp that hangs */
hz_internal void cod_fx_impact_concrete(CodFxPlan *plan, VfxDesc *d) {
  CF_P(plan, d, {.tile = CF_TILE_FLASH_CORE, .count = 1, .life = {0.07f, 0.0f}, .drag = {6.0f, 0.0f}, .size0 = {0.045f, 0.0f}, .size1 = {0.19f, 0.0f}, .size_curve = 0.42f,
                 .c0 = {1.0f, 0.72f, 0.4f}, .c1 = {1.0f, 0.4f, 0.12f}, .i0 = 10.0f, .alpha_curve = 0.7f, .soft = 0.25f, .flags = CF_FLAG_GAIN,
                 .noff = {0.01f, 0.0f}, .additive = true, .scale_mask = CF_E});
  v3 c0 = {0.56f, 0.462f, 0.33f}, c1 = {0.45f, 0.365f, 0.26f};
  CodFxBand b0 = {5, {0.22f, 0.4f}, {0}, {5.0f, 7.0f}, {1.8f, 3.2f}, {0.036f, 0.08f}, {0.3f, 0.62f}, {0.4f, 0.72f}, 0.3f};
  CodFxBand b1 = {5, {0.5f, 0.85f}, {0.02f, 0.09f}, {2.6f, 4.0f}, {0.9f, 1.9f}, {0.045f, 0.1f}, {0.3f, 0.62f}, {0.4f, 0.72f}, 0.5f};
  CodFxBand b2 = {4, {1.1f, 1.8f}, {0.02f, 0.2f}, {2.6f, 4.0f}, {0.4f, 1.0f}, {0.045f, 0.1f}, {0.405f, 0.837f}, {0.28f, 0.504f}, 0.78f};
  CodFxPart dust = cod_fx_band_part(&b0, CF_TILE_DUST, 0, 0.0f, c0, c1, 0.85f, 0.7f, 0.72f, 1.5f, 1.5f);
  dust.i0 = dust.i1 = 1.0f;
  cod_fx_plan_part(plan, d, &dust);
  dust = cod_fx_band_part(&b1, CF_TILE_DUST, 0, 0.0f, c0, c1, 1.25f, 0.7f, 0.72f, 1.5f, 1.5f);
  dust.i0 = dust.i1 = 1.0f;
  cod_fx_plan_part(plan, d, &dust);
  dust = cod_fx_band_part(&b2, CF_TILE_SMOKE_A, 0, 0.0f, c0, c1, 1.25f, 0.7f, 0.72f, 1.5f, 1.5f);
  dust.i0 = dust.i1 = 1.0f;
  cod_fx_plan_part(plan, d, &dust);

  // the directional jet: the cue that says which way the round was travelling
  CodFxPart jet = cod_fx_puff(CF_TILE_DUST, 8, (v2){0.18f, 0.32f}, (v2){0.025f, 0.05f}, (v2){0.14f, 0.26f}, (v3){0.58f, 0.475f, 0.34f},
                              (v3){0.47f, 0.38f, 0.27f}, (v2){0.35f, 0.6f}, 1.2f);
  jet.size_curve = 0.5f;
  jet.drag = (v2){6.0f, 9.0f};
  jet.gravity = -1.2f;
  jet.spin = (v2){-2.2f, 2.2f};
  jet.axis = CF_AXIS_ALT;
  jet.spread = 0.34f;
  jet.power = 1.6f;
  jet.speed = (v2){3.4f, 7.5f};
  jet.vel_add = (v3){0.0f, 0.2f, 0.0f};
  jet.noff = (v2){0.02f, 0.0f};
  jet.soft = 0.08f;
  cod_fx_plan_part(plan, d, &jet);

  CodFxPart chip = cod_fx_fleck(CF_TILE_CHIP, 14, CF_AXIS_ALT, 0.85f, 1.4f, (v2){3.5f, 9.5f}, (v2){0.008f, 0.026f}, (v2){0.5f, 1.0f}, 0.35f, -19.0f, 22.0f,
                                (v3){0.5f, 0.48f, 0.45f}, (v3){0.42f, 0.4f, 0.38f}, 0.25f);
  chip.size1_ratio = 0.9f;
  chip.i0 = chip.i1 = 1.0f;
  cod_fx_plan_part(plan, d, &chip);

  // concrete has flint in it: a few sparks every hit, the first two skitter off the pavement
  CodFxPart sp = cod_fx_spark(2, CF_AXIS_ALT, 0.85f, 1.2f, (v2){3.0f, 8.0f}, (v2){0.007f, 0.016f}, (v2){0.22f, 0.55f}, 0.7f, 2600.0f, 1);
  sp.hemi = 0.12f;
  cod_fx_plan_part(plan, d, &sp);
  sp.count = 3;
  sp.bounces = 0;
  cod_fx_plan_part(plan, d, &sp);

  for (u32 i = 0; i < 2; i++) {
    CodFxPart w = cod_fx_puff(i ? CF_TILE_WISP : CF_TILE_SMOKE_B, 2, (v2){1.7f, 2.9f}, (v2){0.12f, 0.2f}, (v2){0.7f, 1.2f}, (v3){0.55f, 0.49f, 0.4f},
                              (v3){0.46f, 0.41f, 0.34f}, (v2){0.28f, 0.46f}, 1.35f);
    w.size_curve = 0.75f;
    w.drag = (v2){1.5f, 0.0f};
    w.gravity = 0.28f;
    w.spin = (v2){-0.5f, 0.5f};
    w.spread = 1.0f;
    w.power = 0.6f;
    w.speed = (v2){0.35f, 0.0f};
    w.axis_scale = (v3){1.0f, 0.0f, 1.0f};
    w.vel_add = (v3){0.0f, 0.545f, 0.0f};
    w.noff = (v2){0.16f, 0.0f};
    w.dir_off = (v2){0.07f, 0.0f};
    w.soft = 0.14f;
    w.turb = 0.14f;
    w.turb_freq = 1.1f;
    cod_fx_plan_part(plan, d, &w);
  }
}

/* plaster, drywall: white powder, crumbs, no sparks */
hz_internal void cod_fx_impact_plaster(CodFxPlan *plan, VfxDesc *d) {
  v3 c0 = {0.74f, 0.63f, 0.465f}, c1 = {0.63f, 0.53f, 0.385f};
  CodFxBand b0 = {5, {0.25f, 0.45f}, {0}, {5.0f, 7.0f}, {0.6f, 2.2f}, {0.04f, 0.088f}, {0.34f, 0.62f}, {0.42f, 0.72f}, 0.3f};
  CodFxBand b1 = {4, {0.7f, 1.2f}, {0.02f, 0.1f}, {2.6f, 3.8f}, {0.6f, 2.2f}, {0.05f, 0.11f}, {0.34f, 0.62f}, {0.42f, 0.72f}, 0.48f};
  CodFxBand b2 = {4, {1.4f, 2.2f}, {0.02f, 0.22f}, {2.6f, 3.8f}, {0.6f, 2.2f}, {0.05f, 0.11f}, {0.442f, 0.806f}, {0.294f, 0.504f}, 0.78f};
  const CodFxBand *bands[3] = {&b0, &b1, &b2};
  for (u32 i = 0; i < 3; i++) {
    CodFxPart dust = cod_fx_band_part(bands[i], CF_TILE_MIST, CF_TILE_DUST, 0.5f, c0, c1, 1.3f, 0.6f, 0.74f, 1.6f, 1.2f);
    dust.gravity = -0.55f;
    dust.vel_add = (v3){0.0f, 0.4f, 0.0f};
    dust.noff = (v2){0.05f, 0.14f};
    dust.disc = 0.0f;
    dust.turb = 0.06f;
    dust.turb_freq = 2.0f;
    cod_fx_plan_part(plan, d, &dust);
  }
  CodFxPart chip = cod_fx_fleck(CF_TILE_CHIP, 11, CF_AXIS_ALT, 0.95f, 1.3f, (v2){2.0f, 6.0f}, (v2){0.007f, 0.02f}, (v2){0.5f, 0.9f}, 0.5f, -19.0f, 18.0f,
                                (v3){0.72f, 0.65f, 0.52f}, (v3){0.64f, 0.575f, 0.46f}, 0.25f);
  cod_fx_plan_part(plan, d, &chip);
  CodFxPart jet = cod_fx_puff(CF_TILE_DUST, 7, (v2){0.18f, 0.3f}, (v2){0.022f, 0.045f}, (v2){0.12f, 0.24f}, (v3){0.77f, 0.66f, 0.485f},
                              (v3){0.65f, 0.55f, 0.4f}, (v2){0.35f, 0.6f}, 1.2f);
  jet.size_curve = 0.5f;
  jet.drag = (v2){6.0f, 9.0f};
  jet.gravity = -1.1f;
  jet.spin = (v2){-2.2f, 2.2f};
  jet.axis = CF_AXIS_ALT;
  jet.spread = 0.32f;
  jet.power = 1.6f;
  jet.speed = (v2){3.0f, 6.5f};
  jet.vel_add = (v3){0.0f, 0.2f, 0.0f};
  jet.noff = (v2){0.02f, 0.0f};
  jet.soft = 0.08f;
  cod_fx_plan_part(plan, d, &jet);
}

/* steel: the sparks are the whole story, bright, streaked, short-lived; `grazing` hits throw a narrower fan */
hz_internal void cod_fx_impact_metal(CodFxPlan *plan, VfxDesc *d, b32 grazing) {
  f32 spread = grazing ? 0.5f : 0.9f;
  // the ricochet tongue: FLASH_LOBE is rooted at its -X edge, so it rides the reflected ray with its root on the impact point
  CF_P(plan, d, {.tile = CF_TILE_FLASH_LOBE, .count = 1, .life = {0.07f, 0.0f}, .drag = {8.0f, 0.0f}, .size0 = {0.08f, 0.0f}, .size1 = {0.3f, 0.0f}, .size_curve = 0.38f,
                 .c0 = {1.0f, 0.6f, 0.26f}, .c1 = {1.0f, 0.5f, 0.16f}, .i0 = 9.0f, .alpha_curve = 0.6f, .soft = 0.2f, .flags = CF_FLAG_GAIN | CF_FLAG_LOBE,
                 .axis = CF_AXIS_ALT, .speed = {0.001f, 0.0f}, .noff = {0.012f, 0.0f}, .dir_off = {0.15f, 0.0f}, .additive = true, .rot_jit = 0.2f,
                 .scale_mask = VFX_SCALE_SIZE | VFX_SCALE_RADIUS});
  CF_P(plan, d, {.tile = CF_TILE_FLASH_CORE, .count = 1, .life = {0.075f, 0.0f}, .drag = {8.0f, 0.0f}, .size0 = {0.03f, 0.0f}, .size1 = {0.11f, 0.0f}, .size_curve = 0.4f,
                 .c0 = {1.0f, 0.97f, 0.92f}, .c1 = {1.0f, 0.55f, 0.2f}, .i0 = 24.0f, .alpha_curve = 0.5f, .soft = 0.2f, .flags = CF_FLAG_GAIN,
                 .noff = {0.012f, 0.0f}, .additive = true, .scale_mask = CF_E});
  // the shower: velocity-aligned streaks tight around the reflected ray, a few long fliers that bounce twice
  CodFxPart fl = cod_fx_spark(7, CF_AXIS_ALT, spread, 2.4f, (v2){11.0f, 17.0f}, (v2){0.008f, 0.017f}, (v2){0.4f, 0.72f}, 1.1f, 2500.0f, 2);
  fl.noff = (v2){0.012f, 0.0f};
  cod_fx_plan_part(plan, d, &fl);
  CodFxPart sp = cod_fx_spark(3, CF_AXIS_ALT, spread, 1.1f, (v2){4.5f, 11.0f}, (v2){0.008f, 0.017f}, (v2){0.2f, 0.5f}, 1.1f, 2500.0f, 1);
  sp.noff = (v2){0.012f, 0.0f};
  cod_fx_plan_part(plan, d, &sp);
  sp.count = 21;
  sp.bounces = 0;
  cod_fx_plan_part(plan, d, &sp);
  // round embers that survive longer and land
  CodFxPart em = cod_fx_spark(6, CF_AXIS_ALT, 1.0f, 1.0f, (v2){2.0f, 6.0f}, (v2){0.008f, 0.016f}, (v2){0.5f, 1.0f}, 1.0f, 2100.0f, 0);
  em.tile = CF_TILE_SPARK;
  em.size1_ratio = 0.6f;
  em.stretch = 0.0f;
  em.drag = (v2){1.6f, 0.0f};
  em.gravity = -13.0f;
  em.c1 = cod_fx_blackbody(1150.0f);
  em.i0 = 7.5f;
  em.i0_jit = 0.2f;
  em.i1 = 0.1f;
  em.alpha_curve = 0.7f;
  em.noff = (v2){0.0f, 0.0f};
  cod_fx_plan_part(plan, d, &em);
  // hot metal vapour: thin, dark, short
  CodFxPart sm = cod_fx_puff(CF_TILE_WISP, 4, (v2){0.6f, 1.1f}, (v2){0.05f, 0.0f}, (v2){0.3f, 0.5f}, (v3){0.2f, 0.19f, 0.18f}, (v3){0.26f, 0.25f, 0.24f},
                             (v2){0.3f, 0.5f}, 1.7f);
  sm.size_curve = 0.6f;
  sm.drag = (v2){2.4f, 0.0f};
  sm.gravity = 0.4f;
  sm.spin = (v2){-1.4f, 1.4f};
  sm.spread = 1.1f;
  sm.power = 0.7f;
  sm.speed = (v2){0.7f, 0.0f};
  sm.vel_add = (v3){0.0f, 0.5f, 0.0f};
  sm.noff = (v2){0.1f, 0.0f};
  sm.soft = 0.1f;
  sm.turb = 0.09f;
  sm.turb_freq = 1.8f;
  cod_fx_plan_part(plan, d, &sm);
}

/* wood: splinters and a brown, resinous puff */
hz_internal void cod_fx_impact_wood(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart spl = cod_fx_fleck(CF_TILE_SPLINTER, 18, CF_AXIS_ALT, 0.9f, 1.3f, (v2){2.5f, 7.5f}, (v2){0.014f, 0.045f}, (v2){0.6f, 1.2f}, 0.8f, -18.0f, 26.0f,
                               (v3){0.44f, 0.3f, 0.16f}, (v3){0.36f, 0.24f, 0.13f}, 0.25f);
  spl.tile2 = CF_TILE_CHIP;
  spl.tile2_frac = 0.25f;
  cod_fx_plan_part(plan, d, &spl);
  CodFxPart dust = cod_fx_puff(CF_TILE_DUST, 8, (v2){0.45f, 0.9f}, (v2){0.04f, 0.09f}, (v2){0.24f, 0.44f}, (v3){0.46f, 0.34f, 0.2f}, (v3){0.38f, 0.28f, 0.17f},
                               (v2){0.5f, 0.8f}, 1.5f);
  dust.size_curve = 0.45f;
  dust.drag = (v2){3.4f, 0.0f};
  dust.gravity = -0.9f;
  dust.spin = (v2){-1.4f, 1.4f};
  dust.axis = CF_AXIS_ALT;
  dust.spread = 1.1f;
  dust.power = 0.7f;
  dust.speed = (v2){0.6f, 2.0f};
  dust.vel_add = (v3){0.0f, 0.35f, 0.0f};
  dust.noff = (v2){0.05f, 0.12f};
  dust.soft = 0.09f;
  dust.turb = 0.05f;
  dust.turb_freq = 2.2f;
  cod_fx_plan_part(plan, d, &dust);
}

/* dirt and sand: a plume you can hide behind, plus heavy ejected clods */
hz_internal void cod_fx_impact_ground(CodFxPlan *plan, VfxDesc *d, b32 sand) {
  v3 c = sand ? (v3){0.66f, 0.56f, 0.4f} : (v3){0.3f, 0.22f, 0.15f};
  CodFxPart plume = cod_fx_puff(CF_TILE_DUST, 13, (v2){0.8f, 1.5f}, (v2){0.06f, 0.13f}, (v2){0.55f, 1.0f}, c, v3_scale(c, 0.85f), (v2){0.6f, 0.95f}, 1.4f);
  plume.tile2 = CF_TILE_SMOKE_B;
  plume.tile2_frac = 0.333f;
  plume.size_curve = 0.5f;
  plume.drag = (v2){2.2f, 3.2f};
  plume.gravity = -1.6f;
  plume.spin = (v2){-1.1f, 1.1f};
  plume.spread = 0.75f;
  plume.power = 0.55f;
  plume.speed = (v2){1.6f, 4.2f};
  plume.disc = 0.06f;
  plume.up_off = (v2){0.01f, 0.0f};
  plume.soft = 0.14f;
  plume.turb = 0.08f;
  plume.turb_freq = 1.6f;
  plume.scale_mask = CF_E_SPEED;
  cod_fx_plan_part(plan, d, &plume);
  CodFxPart clod = cod_fx_fleck(CF_TILE_CHIP, 21, CF_AXIS_NORMAL, 0.95f, 1.1f, (v2){3.0f, 9.0f}, (v2){0.008f, sand ? 0.02f : 0.035f}, (v2){0.6f, 1.2f},
                                sand ? 1.4f : 0.5f, -19.0f, 20.0f, v3_scale(c, 0.8f), v3_scale(c, 0.7f), 0.3f);
  clod.noff = (v2){0.0f, 0.0f};
  clod.up_off = (v2){0.01f, 0.0f};
  clod.scale_mask = 0;
  cod_fx_plan_part(plan, d, &clod);
}

/* glass: glinting shards thrown forward and a few back, fine aerosol */
hz_internal void cod_fx_impact_glass(CodFxPlan *plan, VfxDesc *d) {
  for (u32 back = 0; back < 2; back++) {
    CodFxPart sh = cod_fx_fleck(CF_TILE_CHIP, back ? 7 : 16, back ? CF_AXIS_ALT_NEG : CF_AXIS_ALT, 1.0f, 1.2f, (v2){2.5f, 8.0f}, (v2){0.01f, 0.038f},
                                (v2){0.7f, 1.4f}, 0.6f, -19.0f, 30.0f, (v3){0.72f, 0.8f, 0.84f}, (v3){0.6f, 0.68f, 0.72f}, 0.3f);
    sh.tile2 = CF_TILE_SPLINTER;
    sh.tile2_frac = 0.4f;
    sh.alpha = (v2){0.85f, 0.0f};
    sh.noff = (v2){0.0f, 0.0f};
    cod_fx_plan_part(plan, d, &sh);
    // the specular twinkle riding the shard
    CodFxPart gl = cod_fx_spark(back ? 4 : 9, back ? CF_AXIS_ALT_NEG : CF_AXIS_ALT, 1.0f, 1.2f, (v2){2.5f, 8.0f}, (v2){0.01f, 0.02f}, (v2){0.7f, 1.4f}, 0.55f, 4000.0f, 0);
    gl.tile = CF_TILE_SPARK;
    gl.stretch = 0.0f;
    gl.size1_ratio = 1.0f;
    gl.drag = (v2){0.6f, 0.0f};
    gl.gravity = -19.0f;
    gl.c0 = (v3){0.85f, 0.95f, 1.0f};
    gl.c1 = (v3){0.8f, 0.9f, 1.0f};
    gl.i0 = 5.5f;
    gl.i0_jit = 0.45f;
    gl.i1 = 0.2f;
    gl.alpha_curve = 1.0f;
    gl.hemi = 0.0f;
    gl.clamp_cos = 0.0f;
    gl.noff = (v2){0.0f, 0.0f};
    cod_fx_plan_part(plan, d, &gl);
  }
  CodFxPart mist = cod_fx_puff(CF_TILE_MIST, 6, (v2){0.35f, 0.7f}, (v2){0.05f, 0.0f}, (v2){0.28f, 0.45f}, (v3){0.8f, 0.86f, 0.9f}, (v3){0.7f, 0.76f, 0.8f}, (v2){0.5f, 0.0f},
                               1.6f);
  mist.size_curve = 0.45f;
  mist.drag = (v2){4.2f, 0.0f};
  mist.gravity = -3.0f;
  mist.axis = CF_AXIS_ALT;
  mist.spread = 1.2f;
  mist.power = 0.7f;
  mist.speed = (v2){1.6f, 0.0f};
  mist.soft = 0.1f;
  cod_fx_plan_part(plan, d, &mist);
}

/* water: a column that rises and collapses, droplets, a mist over the impact */
hz_internal void cod_fx_impact_water(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart col = cod_fx_puff(CF_TILE_SPLASH, 7, (v2){0.4f, 0.72f}, (v2){0.07f, 0.13f}, (v2){0.3f, 0.55f}, (v3){0.7f, 0.76f, 0.8f}, (v3){0.6f, 0.68f, 0.72f},
                              (v2){0.6f, 0.9f}, 1.5f);
  col.rot_random = false;
  col.rot_jit = 0.25f;
  col.size_curve = 0.55f;
  col.drag = (v2){1.1f, 0.0f};
  col.gravity = -13.0f;
  col.spin = (v2){-0.6f, 0.6f};
  col.axis = CF_AXIS_UP;
  col.speed = (v2){2.4f, 4.6f};
  col.disc = 0.05f;
  col.up_off = (v2){0.02f, 0.0f};
  col.soft = 0.1f;
  col.scale_mask = CF_E_SPEED;
  cod_fx_plan_part(plan, d, &col);
  CodFxPart drop = cod_fx_fleck(CF_TILE_DROPLET, 28, CF_AXIS_NORMAL, 0.85f, 0.9f, (v2){2.5f, 7.5f}, (v2){0.008f, 0.026f}, (v2){0.4f, 0.9f}, 0.7f, -19.0f, 0.0f,
                                (v3){0.72f, 0.78f, 0.82f}, (v3){0.66f, 0.72f, 0.76f}, 0.4f);
  drop.size1_ratio = 0.9f;
  drop.stretch = 0.5f;
  drop.rot_random = false;
  drop.alpha = (v2){0.8f, 0.0f};
  drop.noff = (v2){0.0f, 0.0f};
  drop.up_off = (v2){0.02f, 0.0f};
  cod_fx_plan_part(plan, d, &drop);
  CodFxPart mist = cod_fx_puff(CF_TILE_MIST, 5, (v2){0.5f, 0.9f}, (v2){0.08f, 0.0f}, (v2){0.35f, 0.6f}, (v3){0.78f, 0.83f, 0.86f}, (v3){0.7f, 0.75f, 0.78f}, (v2){0.4f, 0.0f},
                               1.7f);
  mist.size_curve = 0.5f;
  mist.drag = (v2){3.4f, 0.0f};
  mist.gravity = -1.4f;
  mist.vel_add = (v3){0.0f, 0.7f, 0.0f};
  mist.up_off = (v2){0.05f, 0.0f};
  mist.soft = 0.12f;
  cod_fx_plan_part(plan, d, &mist);
}

/* flesh: a dark aerosol cone and heavy droplets sprayed with the round */
hz_internal void cod_fx_impact_flesh(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart mist = cod_fx_puff(CF_TILE_MIST, 15, (v2){0.3f, 0.62f}, (v2){0.035f, 0.075f}, (v2){0.16f, 0.34f}, (v3){0.34f, 0.035f, 0.03f},
                               (v3){0.16f, 0.016f, 0.014f}, (v2){0.6f, 0.95f}, 1.5f);
  mist.tile2 = CF_TILE_SMOKE_A;
  mist.tile2_frac = 0.333f;
  mist.size_curve = 0.5f;
  mist.drag = (v2){4.5f, 6.5f};
  mist.gravity = -3.2f;
  mist.spin = (v2){-2.0f, 2.0f};
  mist.axis = CF_AXIS_ALT;
  mist.spread = 0.95f;
  mist.power = 0.8f;
  mist.speed = (v2){1.2f, 4.5f};
  mist.vel_add = (v3){0.0f, 0.3f, 0.0f};
  mist.noff = (v2){-0.02f, 0.0f};
  mist.soft = 0.08f;
  mist.turb = 0.04f;
  mist.turb_freq = 3.0f;
  cod_fx_plan_part(plan, d, &mist);
  CodFxPart drop = cod_fx_fleck(CF_TILE_DROPLET, 23, CF_AXIS_ALT, 1.1f, 1.2f, (v2){2.0f, 8.0f}, (v2){0.007f, 0.022f}, (v2){0.35f, 0.8f}, 0.9f, -19.0f, 0.0f,
                                (v3){0.3f, 0.03f, 0.025f}, (v3){0.22f, 0.022f, 0.018f}, 0.35f);
  drop.stretch = 0.6f;
  drop.rot_random = false;
  drop.alpha = (v2){0.95f, 0.0f};
  drop.noff = (v2){0.0f, 0.0f};
  cod_fx_plan_part(plan, d, &drop);
}

/* foliage: shredded leaf matter, no hole */
hz_internal void cod_fx_impact_foliage(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart leaf = cod_fx_fleck(CF_TILE_CHIP, 17, CF_AXIS_ALT, 1.3f, 1.0f, (v2){1.5f, 5.0f}, (v2){0.012f, 0.035f}, (v2){0.8f, 1.6f}, 2.2f, -8.0f, 16.0f,
                                (v3){0.14f, 0.22f, 0.08f}, (v3){0.11f, 0.17f, 0.06f}, 0.4f);
  leaf.tile2 = CF_TILE_SPLINTER;
  leaf.tile2_frac = 0.5f;
  leaf.noff = (v2){0.0f, 0.0f};
  cod_fx_plan_part(plan, d, &leaf);
}

/* fabric and rubber: dust, fibres and a tear */
hz_internal void cod_fx_impact_soft(CodFxPlan *plan, VfxDesc *d, b32 rubber) {
  v3 a0 = rubber ? (v3){0.1f, 0.095f, 0.09f} : (v3){0.5f, 0.45f, 0.38f};
  v3 a1 = rubber ? (v3){0.08f, 0.078f, 0.075f} : (v3){0.42f, 0.38f, 0.32f};
  CodFxPart dust = cod_fx_puff(CF_TILE_MIST, 10, (v2){0.4f, 0.8f}, (v2){0.04f, 0.08f}, (v2){0.2f, 0.36f}, a0, a1, (v2){0.45f, 0.7f}, 1.5f);
  dust.tile2 = CF_TILE_DUST;
  dust.tile2_frac = 0.5f;
  dust.size_curve = 0.45f;
  dust.drag = (v2){3.8f, 0.0f};
  dust.gravity = -1.2f;
  dust.spin = (v2){-1.5f, 1.5f};
  dust.axis = CF_AXIS_ALT;
  dust.spread = 1.1f;
  dust.power = 0.8f;
  dust.speed = (v2){0.8f, 3.0f};
  dust.vel_add = (v3){0.0f, 0.3f, 0.0f};
  dust.noff = (v2){0.02f, 0.0f};
  dust.soft = 0.09f;
  cod_fx_plan_part(plan, d, &dust);
  v3 f0 = rubber ? (v3){0.09f, 0.085f, 0.08f} : (v3){0.46f, 0.41f, 0.34f};
  CodFxPart fib = cod_fx_fleck(CF_TILE_SPLINTER, 8, CF_AXIS_ALT, 1.0f, 1.2f, (v2){1.5f, 4.5f}, (v2){0.01f, 0.03f}, (v2){0.6f, 1.2f}, 2.4f, -14.0f, 18.0f, f0,
                               v3_scale(f0, 0.9f), 0.35f);
  fib.noff = (v2){0.0f, 0.0f};
  cod_fx_plan_part(plan, d, &fib);
}
