// Claude of Duty effects: muzzle flashes, explosions, tracers and the small reactions.

typedef struct CodFxMuzzleProfile {
  f32 scale;
  u32 ports; // the always-present tongues; a fourth appears in 45% of the shots when `fourth`
  b32 fourth;
  f32 jet;
  f32 light; // peak candela of the world light, before its 0.18
  f32 smoke;
  f32 temp;
  f32 sparks;
} CodFxMuzzleProfile;

hz_internal CodFxMuzzleProfile cod_fx_muzzle_profile(u32 weapon) {
  switch (weapon) {
  case COD_WEAPON_SMG: return (CodFxMuzzleProfile){0.78f, 3, true, 0.75f, 135.0f, 0.7f, 1.05f, 6.0f};
  case COD_WEAPON_PISTOL: return (CodFxMuzzleProfile){0.66f, 3, false, 0.6f, 100.0f, 0.55f, 1.06f, 5.0f};
  default: return (CodFxMuzzleProfile){1.0f, 3, true, 1.0f, 200.0f, 1.0f, 1.0f, 8.0f};
  }
}

#define CF_PSCALE 1.25f

// a muzzle flash: core, heart, tongues, jet, powder, crown puffs and barrel smoke around the bore, plus the world flash light
hz_internal void cod_fx_recipe_muzzle(CodFxPlan *plan, VfxDesc *d, const CodFxMuzzleProfile *pr) {
  v3 core = cod_fx_blackbody(4100.0f * pr->temp);
  v3 gas = cod_fx_blackbody(4100.0f * 0.62f * pr->temp);
  v3 tip = cod_fx_blackbody(2200.0f);
  const u32 add_flags = CF_FLAG_GAIN;
  CodFxPart outer = {.tile = CF_TILE_FLASH_CORE, .count = 1, .life = {0.062f, 0.0f}, .drag = {10.0f, 0.0f}, .size0 = {0.085f, 0.0f}, .size1 = {0.135f, 0.0f}, .size_curve = 0.35f,
                     .c0 = gas, .c1 = tip, .i0 = 17.0f, .i1 = 4.5f, .alpha_curve = 0.7f, .soft = 0.15f, .flags = add_flags, .rot_random = true,
                     .spin = {-3.0f, 3.0f}, .additive = true, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &outer);
  CodFxPart heart = {.tile = CF_TILE_FLASH_CORE, .count = 1, .life = {0.046f, 0.0f}, .drag = {12.0f, 0.0f}, .size0 = {0.036f, 0.0f}, .size1 = {0.056f, 0.0f}, .size_curve = 0.4f,
                     .c0 = core, .c1 = gas, .i0 = 38.0f, .i1 = 6.0f, .alpha_curve = 0.6f, .soft = 0.12f, .flags = add_flags, .rot_random = true,
                     .spin = {-4.0f, 4.0f}, .additive = true, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &heart);

  CodFxPart tongue = {.tile = CF_TILE_FLASH_LOBE, .count = (f32)pr->ports, .life = {0.04f, 0.06f}, .drag = {12.0f, 0.0f}, .size0 = {0.05f, 0.11f},
                      .size1 = {0.14f, 0.34f}, .size_curve = 0.35f, .c0 = gas, .c1 = tip, .i0 = 11.0f, .i0_jit = 0.27f, .i1 = 1.6f, .alpha_curve = 0.75f,
                      .soft = 0.15f, .flags = add_flags | CF_FLAG_LOBE, .pitch = {0.4f, 0.85f}, .speed = {0.5f, 2.0f}, .root_off = 0.34f,
                      .spin = {-1.6f, 1.6f}, .rot_jit = 0.1f, .additive = true, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &tongue);
  if (pr->fourth) {
    tongue.count = 1;
    tongue.gate = 0.45f;
    cod_fx_plan_part(plan, d, &tongue);
  }

  CodFxPart jet = {.tile = CF_TILE_STREAK, .count = 1, .life = {0.044f, 0.0f}, .drag = {14.0f, 0.0f}, .size0 = {0.045f * pr->jet, 0.0f}, .size1 = {0.018f * pr->jet, 0.0f},
                   .c0 = {1.0f, 0.8f, 0.5f}, .c1 = {1.0f, 0.4f, 0.1f}, .i0 = 9.0f, .i1 = 0.0f, .alpha_curve = 0.6f, .soft = 0.12f, .stretch = 0.28f * pr->jet,
                   .flags = add_flags, .speed = {7.0f, 0.0f}, .dir_off = {0.04f, 0.0f}, .additive = true, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &jet);

  CodFxPart grain = {.tile = CF_TILE_STREAK, .count = pr->sparks * CF_PSCALE, .life = {0.1f, 0.4f}, .drag = {3.2f, 0.0f}, .gravity = -11.0f,
                     .size0 = {0.005f, 0.011f}, .size1_ratio = 0.4f, .c0 = cod_fx_blackbody(2600.0f * pr->temp), .c1 = cod_fx_blackbody(1200.0f), .i0 = 8.0f,
                     .i0_jit = 0.375f, .i1 = 0.15f, .alpha_curve = 0.5f, .soft = 0.05f, .stretch = 0.4f, .flags = CF_FLAG_FLICKER | CF_FLAG_EMBER,
                     .spread = 0.55f, .power = 1.4f, .hemi = 0.25f, .clamp_cos = CF_COS55, .speed = {2.5f, 8.5f}, .dir_off = {0.012f, 0.0f}, .additive = true,
                     .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &grain);

  // the crown puff: a hot additive start, and its cooling smoke a frame later
  f32 sm = 0.7f + pr->smoke * 0.5f;
  CodFxPart hot = {.tile = CF_TILE_SMOKE_B, .tile2 = CF_TILE_WISP, .tile2_frac = 0.5f, .count = 4, .life = {0.05f, 0.08f}, .drag = {7.0f, 0.0f},
                   .size0 = {0.016f, 0.031f}, .size1 = {0.09f * 0.75f * sm * 0.75f, 0.145f * 0.75f * sm * 0.75f}, .size_curve = 0.5f, .c0 = {1.0f, 0.5f, 0.17f},
                   .c1 = {1.0f, 0.26f, 0.05f}, .i0 = 0.85f, .i0_jit = 0.41f, .alpha_curve = 1.1f, .soft = 0.2f, .flags = add_flags, .spread = 0.7f,
                   .power = 0.9f, .hemi = 0.1f, .speed = {0.6f, 1.9f}, .vel_add = {0.0f, 0.12f, 0.0f}, .dir_off = {0.018f, 0.0f}, .rot_random = true,
                   .spin = {-2.2f, 2.2f}, .additive = true, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &hot);
  f32 sm_a = 0.7f + pr->smoke * 0.4f;
  CodFxPart puff = {.tile = CF_TILE_SMOKE_B, .tile2 = CF_TILE_WISP, .tile2_frac = 0.5f, .count = 4, .life = {0.3f * sm, 0.55f * sm}, .delay = {0.012f, 0.034f},
                    .drag = {7.0f, 0.0f}, .gravity = 0.5f, .size0 = {0.018f, 0.034f}, .size1 = {0.075f * sm, 0.145f * sm}, .size_curve = 0.5f,
                    .c0 = {0.62f, 0.57f, 0.52f}, .c1 = {0.44f, 0.44f, 0.44f}, .alpha = {0.26f * sm_a, 0.44f * sm_a}, .alpha_curve = 1.35f, .soft = 0.22f,
                    .turb = 0.05f, .turb_freq = 2.4f, .flags = CF_FLAG_EMBER_ALPHA, .spread = 0.7f, .power = 0.9f, .hemi = 0.1f, .speed = {0.6f, 1.9f},
                    .vel_add = {0.0f, 0.12f, 0.0f}, .dir_off = {0.018f, 0.0f}, .lit = true, .rot_random = true, .spin = {-2.2f, 2.2f}, .scale_mask = CF_E};
  cod_fx_plan_part(plan, d, &puff);

  // barrel smoke rolls off the crown and lingers
  CodFxPart bs = {.tile = CF_TILE_SMOKE_A, .tile2 = CF_TILE_WISP, .tile2_frac = 0.5f, .count = (2.0f + pr->smoke * 3.0f) * CF_PSCALE,
                  .life = {0.5f * (0.6f + pr->smoke * 0.6f), 1.15f * (0.6f + pr->smoke * 0.6f)}, .delay = {0.0f, 0.05f}, .drag = {2.6f, 0.0f}, .gravity = 0.36f,
                  .size0 = {0.03f * pr->smoke, 0.055f * pr->smoke}, .size1 = {0.16f * pr->smoke, 0.32f * pr->smoke}, .size_curve = 0.55f,
                  .c0 = {0.5f, 0.49f, 0.47f}, .c1 = {0.42f, 0.41f, 0.4f}, .alpha = {0.1f * (0.6f + pr->smoke * 0.5f), 0.2f * (0.6f + pr->smoke * 0.5f)},
                  .alpha_curve = 1.8f, .soft = 0.25f, .turb = 0.06f, .turb_freq = 1.6f, .flags = CF_FLAG_EMBER_ALPHA, .spread = 1.0f, .power = 0.7f,
                  .speed = {0.4f * pr->smoke, 2.2f * pr->smoke}, .vel_add = {0.0f, 0.28f, 0.0f}, .dir_off = {0.0f, 0.09f}, .lit = true, .rot_random = true,
                  .spin = {-1.6f, 1.6f}, .scale_mask = 0};
  bs.scale_mask = VFX_SCALE_HEIGHT;
  cod_fx_plan_part(plan, d, &bs);
}

// the world flash light, played apart from the sprites: a first-person flash lights the street from further ahead of the gun than the crown
hz_internal void cod_fx_recipe_flash_light(CodFxPlan *plan, VfxDesc *d) {
  (void)plan;
  f32 peak;
  v3 hue = vfx_light_hue(cod_fx_blackbody(4100.0f), &peak);
  cod_fx_plan_elem(d, (VfxElemDesc){.kind = VFX_ELEM_LIGHT, .light = {.color = hue, .intensity = 1.0f, .radius = 5.0f, .life = 0.09f, .curve = 1.5f}});
}

/* explosions: unit radius, a play's scale is the blast radius R (sizes, spawn offsets, the light) */
hz_internal void cod_fx_recipe_explosion(CodFxPlan *plan, VfxDesc *d) {
  const u32 all = VFX_SCALE_SIZE | VFX_SCALE_SPEED | VFX_SCALE_RADIUS | VFX_SCALE_HEIGHT;
  cod_fx_plan_part(plan, d, &(CodFxPart){.tile = CF_TILE_FLASH_CORE, .count = 1, .life = {0.085f, 0.0f}, .drag = {5.0f, 0.0f}, .size0 = {0.35f, 0.0f}, .size1 = {1.5f, 0.0f}, .size_curve = 0.3f,
                                         .c0 = {1.0f, 0.95f, 0.85f}, .c1 = {1.0f, 0.42f, 0.1f}, .i0 = 85.0f, .alpha_curve = 0.5f, .soft = 0.5f, .additive = true,
                                         .scale_mask = VFX_SCALE_SIZE});
  cod_fx_plan_part(plan, d, &(CodFxPart){.tile = CF_TILE_FIRE, .count = 12.0f * CF_PSCALE + 5.0f, .life = {0.3f, 0.62f}, .delay = {0.0f, 0.06f}, .drag = {2.6f, 4.2f},
                                         .gravity = 2.2f, .size0 = {0.18f, 0.34f}, .size1 = {0.7f, 1.15f}, .size_curve = 0.34f, .c0 = {1.0f, 0.81f, 0.51f},
                                         .c1 = {1.0f, 0.22f, 0.04f}, .i0 = 12.0f, .i0_jit = 0.42f, .i1 = 0.3f, .alpha_curve = 0.55f, .soft = 0.6f, .turb = 0.05f,
                                         .turb_freq = 2.4f, .axis = CF_AXIS_UP, .spread = 1.5f, .power = 0.6f, .speed = {0.375f, 1.375f},
                                         .vel_add = {0.0f, 1.4f, 0.0f}, .disc = 0.16f, .up_off = {0.05f, 0.0f}, .rot_random = true, .spin = {-2.2f, 2.2f},
                                         .additive = true, .scale_mask = all});
  CodFxPart boil = cod_fx_puff(CF_TILE_SMOKE_A, 9.0f * CF_PSCALE + 4.0f, (v2){1.1f, 2.2f}, (v2){0.2f, 0.34f}, (v2){0.8f, 1.35f}, (v3){0.1f, 0.095f, 0.09f},
                               (v3){0.19f, 0.185f, 0.18f}, (v2){0.55f, 0.85f}, 1.5f);
  boil.tile2 = CF_TILE_SMOKE_B;
  boil.tile2_frac = 0.5f;
  boil.delay = (v2){0.03f, 0.16f};
  boil.drag = (v2){1.9f, 0.0f};
  boil.gravity = 0.9f;
  boil.size_curve = 0.5f;
  boil.spin = (v2){-0.9f, 0.9f};
  boil.soft = 0.7f;
  boil.turb = 0.06f;
  boil.turb_freq = 1.1f;
  boil.axis = CF_AXIS_UP;
  boil.spread = 1.4f;
  boil.power = 0.7f;
  boil.speed = (v2){0.3f, 1.0f};
  boil.vel_add = (v3){0.0f, 1.0f, 0.0f};
  boil.dir_off = (v2){0.12f, 0.0f};
  boil.up_off = (v2){0.08f, 0.0f};
  boil.scale_mask = all;
  cod_fx_plan_part(plan, d, &boil);
  cod_fx_plan_part(plan, d, &(CodFxPart){.tile = CF_TILE_RING, .count = 1, .life = {0.2f, 0.0f}, .drag = {6.0f, 0.0f}, .size0 = {0.35f, 0.0f}, .size1 = {2.4f, 0.0f}, .size_curve = 0.42f,
                                         .c0 = {1.0f, 0.9f, 0.78f}, .c1 = {1.0f, 0.6f, 0.3f}, .i0 = 3.2f, .alpha_curve = 1.1f, .soft = 1.2f, .additive = true,
                                         .scale_mask = VFX_SCALE_SIZE});
  CodFxPart ring = cod_fx_puff(CF_TILE_DUST, 11.0f * CF_PSCALE + 5.0f, (v2){0.9f, 1.8f}, (v2){0.12f, 0.22f}, (v2){0.55f, 0.95f}, (v3){0.42f, 0.36f, 0.29f},
                               (v3){0.34f, 0.3f, 0.25f}, (v2){0.4f, 0.7f}, 1.5f);
  ring.tile2 = CF_TILE_SMOKE_B;
  ring.tile2_frac = 0.333f;
  ring.drag = (v2){2.4f, 3.6f};
  ring.gravity = -0.5f;
  ring.size_curve = 0.45f;
  ring.spin = (v2){-1.0f, 1.0f};
  ring.soft = 0.5f;
  ring.turb = 0.1f;
  ring.turb_freq = 1.4f;
  ring.flags = CF_FLAG_TURB_ABS;
  ring.axis = CF_AXIS_UP;
  ring.pitch = (v2){1.38f, 1.52f};
  ring.speed = (v2){1.0f, 2.25f};
  ring.dir_off = (v2){0.2f, 0.0f};
  ring.up_off = (v2){-0.05f, 0.0f};
  ring.scale_mask = all;
  cod_fx_plan_part(plan, d, &ring);
  CodFxPart deb = cod_fx_fleck(CF_TILE_CHIP, 26.0f * CF_PSCALE + 10.0f, CF_AXIS_UP, 1.35f, 0.8f, (v2){6.0f, 20.0f}, (v2){0.01f, 0.05f}, (v2){0.7f, 1.8f}, 0.4f,
                               -19.0f, 26.0f, (v3){0.24f, 0.21f, 0.18f}, (v3){0.2f, 0.18f, 0.16f}, 0.3f);
  deb.tile2 = CF_TILE_SPLINTER;
  deb.tile2_frac = 0.3f;
  deb.noff = (v2){0.0f, 0.0f};
  deb.up_off = (v2){0.05f, 0.0f};
  deb.flags = CF_FLAG_SPEED_DEBRIS;
  deb.scale_mask = VFX_SCALE_RADIUS;
  cod_fx_plan_part(plan, d, &deb);
  CodFxPart emb = cod_fx_spark(14.0f * CF_PSCALE + 5.0f, CF_AXIS_UP, 1.4f, 0.9f, (v2){4.0f, 14.0f}, (v2){0.012f, 0.03f}, (v2){0.5f, 1.4f}, 1.0f, 3000.0f, 0);
  emb.c0 = (v3){1.0f, 0.6f, 0.22f};
  emb.c1 = (v3){1.0f, 0.18f, 0.03f};
  emb.i0 = 14.0f;
  emb.i0_jit = 0.43f;
  emb.i1 = 0.2f;
  emb.stretch = 1.1f;
  emb.drag = (v2){1.1f, 0.0f};
  emb.gravity = -13.0f;
  emb.alpha_curve = 0.6f;
  emb.hemi = 0.0f;
  emb.clamp_cos = 0.0f;
  emb.noff = (v2){0.0f, 0.0f};
  emb.up_off = (v2){0.05f, 0.0f};
  emb.scale_mask = VFX_SCALE_HEIGHT;
  cod_fx_plan_part(plan, d, &emb);

  // the smoke column that keeps rising: a puff every ~0.11 s for 1.5 s
  CodFxPart col = cod_fx_puff(CF_TILE_SMOKE_A, 9.0f * 1.5f, (v2){2.55f, 4.25f}, (v2){0.245f, 0.42f}, (v2){0.896f, 1.4f}, (v3){0.12f, 0.116f, 0.113f},
                              (v3){0.228f, 0.223f, 0.216f}, (v2){0.3f, 0.55f}, 1.7f);
  col.tile2 = CF_TILE_SMOKE_B;
  col.tile2_frac = 0.5f;
  col.delay = (v2){0.0f, 1.5f};
  col.drag = (v2){0.75f, 0.0f};
  col.gravity = 0.42f;
  col.size_curve = 0.7f;
  col.spin = (v2){-0.35f, 0.35f};
  col.soft = 0.8f;
  col.turb = 0.175f;
  col.turb_freq = 0.55f;
  col.axis = CF_AXIS_UP;
  col.spread = 0.6f;
  col.power = 0.7f;
  col.speed = (v2){0.8f, 2.0f};
  col.disc = 0.21f;
  col.up_off = (v2){0.1f, 0.24f};
  col.scale_mask = all & ~VFX_SCALE_SPEED;
  cod_fx_plan_part(plan, d, &col);

  // the fireball's light: warm, wide, a third of a second
  f32 peak;
  v3 hue = vfx_light_hue((v3){1.0f, 0.72f, 0.4f}, &peak);
  cod_fx_plan_elem(d, (VfxElemDesc){.kind = VFX_ELEM_LIGHT,
                                    .light = {.color = hue, .intensity = 105.0f, .radius = 8.0f, .life = 0.45f, .curve = 3.0f, .offset = {0.0f, 0.15f, 0.0f}}});
}

/* a soldier's death: dark aerosol thrown up from the body */
hz_internal void cod_fx_recipe_death(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart m = cod_fx_puff(CF_TILE_MIST, 8.0f * CF_PSCALE + 3.0f, (v2){0.4f, 0.8f}, (v2){0.05f, 0.1f}, (v2){0.2f, 0.4f}, (v3){0.3f, 0.03f, 0.026f},
                            (v3){0.15f, 0.015f, 0.013f}, (v2){0.4f, 0.75f}, 1.5f);
  m.tile2 = CF_TILE_SMOKE_A;
  m.tile2_frac = 0.5f;
  m.drag = (v2){5.0f, 0.0f};
  m.gravity = -3.5f;
  m.size_curve = 0.5f;
  m.soft = 0.2f;
  m.axis = CF_AXIS_UP;
  m.spread = 1.4f;
  m.power = 0.7f;
  m.speed = (v2){0.6f, 2.4f};
  m.scale_mask = VFX_SCALE_HEIGHT;
  cod_fx_plan_part(plan, d, &m);
}

/* the player's landing: a ring of pale dust, its size and count following the fall (play gain = strength) */
hz_internal void cod_fx_recipe_land(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart m = cod_fx_puff(CF_TILE_DUST, 7.0f * CF_PSCALE + 2.0f, (v2){0.5f, 1.0f}, (v2){0.05f, 0.1f}, (v2){0.3f, 0.55f}, (v3){0.48f, 0.44f, 0.39f},
                            (v3){0.4f, 0.37f, 0.33f}, (v2){0.25f, 0.5f}, 1.6f);
  m.drag = (v2){3.2f, 0.0f};
  m.gravity = -0.6f;
  m.size_curve = 0.45f;
  m.spin = (v2){-1.2f, 1.2f};
  m.soft = 0.3f;
  m.turb = 0.05f;
  m.turb_freq = 2.0f;
  m.flags = CF_FLAG_ALPHA_GAIN;
  m.axis = CF_AXIS_UP;
  m.pitch = (v2){1.4f, 1.55f};
  m.speed = (v2){0.7f, 2.4f};
  m.dir_off = (v2){0.22f, 0.0f};
  m.up_off = (v2){0.03f, 0.0f};
  m.scale_mask = VFX_SCALE_RADIUS | VFX_SCALE_SPEED;
  cod_fx_plan_part(plan, d, &m);
}

/* a running footstep: one small puff */
hz_internal void cod_fx_recipe_step(CodFxPlan *plan, VfxDesc *d) {
  CodFxPart m = cod_fx_puff(CF_TILE_DUST, 1, (v2){0.4f, 0.75f}, (v2){0.04f, 0.0f}, (v2){0.18f, 0.32f}, (v3){0.46f, 0.42f, 0.37f}, (v3){0.4f, 0.36f, 0.32f},
                            (v2){0.1f, 0.22f}, 1.7f);
  m.drag = (v2){3.4f, 0.0f};
  m.gravity = -0.5f;
  m.size_curve = 0.45f;
  m.soft = 0.25f;
  m.axis = CF_AXIS_UP;
  m.spread = 1.2f;
  m.speed = (v2){0.1f, 0.4f};
  m.disc = 0.08f;
  m.up_off = (v2){0.02f, 0.0f};
  m.scale_mask = VFX_SCALE_RADIUS;
  cod_fx_plan_part(plan, d, &m);
}

/* tracers: three beams (core streak, dimmer afterglow, incandescent head), colours HDR in display units set by the play */
typedef enum CodFxTracerLayer {
  CF_TRACER_CORE = 0,
  CF_TRACER_GLOW,
  CF_TRACER_HEAD,
  CF_TRACER_COUNT,
} CodFxTracerLayer;

hz_internal void cod_fx_recipe_tracer(CodFxPlan *plan, VfxDesc *d, u32 layer) {
  (void)plan;
  VfxElemDesc e = {.kind = VFX_ELEM_BEAM, .additive = 1.0f, .pool = 24};
  switch (layer) {
  case CF_TRACER_CORE: e.trail = (VfxTrailDesc){.width = 0.055f, .width_scale = {1.0f, 0.727f}, .fade = 0.25f, .length = 4.9f, .speed = 340.0f}; break;
  case CF_TRACER_GLOW: e.trail = (VfxTrailDesc){.width = 0.09f, .width_scale = {1.0f, 0.777f}, .fade = 0.3f, .length = 18.5f, .speed = 340.0f}; break;
  default: e.trail = (VfxTrailDesc){.width = 0.05f, .width_scale = {1.0f, 0.84f}, .fade = 0.2f, .length = 0.12f, .speed = 340.0f}; break;
  }
  cod_fx_plan_elem(d, e);
}
