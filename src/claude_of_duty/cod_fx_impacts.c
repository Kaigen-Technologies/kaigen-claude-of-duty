// Claude of Duty effects: the per-surface impact recipes as registered distributions; counts are at the ultra budget (particle scale 1.25).

#define CF_COS55 0.5735764f

// the play's scale is the round's energy e: sizes follow it, and some speeds
#define CF_E (VFX_SCALE_SIZE)
#define CF_E_SPEED (VFX_SCALE_SIZE | VFX_SCALE_SPEED)

// one incandescent spark: a streak whose length is its speed, a blackbody ramp from `kelvin` to 1200 K
hz_internal CodFxPart cod_fx_spark(f32 count, u32 axis, f32 spread, f32 power, v2 speed, v2 size, v2 life, f32 intensity, f32 kelvin, u32 bounces) {
  return (CodFxPart){.tile = CF_TILE_STREAK,
                     .count = count,
                     .life = life,
                     .drag = {1.4f, 2.6f},
                     .gravity = -14.0f,
                     .size0 = size,
                     .size1_ratio = 0.4f,
                     .c0 = cod_fx_blackbody(kelvin),
                     .c1 = cod_fx_blackbody(1200.0f),
                     .i0 = 9.5f * intensity,
                     .i0_jit = 0.368f,
                     .i1 = 0.2f,
                     .alpha_curve = 0.45f,
                     .soft = 0.05f,
                     .stretch = 0.4f,
                     .flags = CF_FLAG_FLICKER,
                     .axis = axis,
                     .spread = spread,
                     .power = power,
                     .hemi = 0.1f,
                     .clamp_cos = CF_COS55,
                     .speed = speed,
                     .noff = {0.01f, 0.0f},
                     .additive = true,
                     .bounces = bounces,
                     .bounce_c = cod_fx_blackbody(1700.0f),
                     .scale_mask = VFX_SCALE_HEIGHT};
}

// a dust or debris puff of the lit kind: the lit emitter with its defaults spelled once
hz_internal CodFxPart cod_fx_puff(u32 tile, f32 count, v2 life, v2 size0, v2 size1, v3 c0, v3 c1, v2 alpha, f32 alpha_curve) {
  return (CodFxPart){.tile = tile,
                     .count = count,
                     .life = life,
                     .size0 = size0,
                     .size1 = size1,
                     .c0 = c0,
                     .c1 = c1,
                     .alpha = alpha,
                     .alpha_curve = alpha_curve,
                     .lit = true,
                     .rot_random = true,
                     .scale_mask = CF_E};
}

// a solid fleck (chip, splinter, shard, clod): a ballistic arc, a tumble, the sprite's own detail
hz_internal CodFxPart cod_fx_fleck(u32 tile, f32 count, u32 axis, f32 spread, f32 power, v2 speed, v2 size, v2 life, f32 drag, f32 gravity, f32 spin, v3 c0,
                                   v3 c1, f32 alpha_curve) {
  return (CodFxPart){.tile = tile,
                     .count = count,
                     .life = life,
                     .drag = {drag, 0.0f},
                     .gravity = gravity,
                     .size0 = size,
                     .size1_ratio = 1.0f,
                     .c0 = c0,
                     .c1 = c1,
                     .alpha_curve = alpha_curve,
                     .soft = 0.06f,
                     .axis = axis,
                     .spread = spread,
                     .power = power,
                     .speed = speed,
                     .noff = {0.01f, 0.0f},
                     .lit = true,
                     .rot_random = true,
                     .spin = {-spin, spin},
                     .scale_mask = VFX_SCALE_HEIGHT};
}

// the pulverised-surface flash masonry hits open with: two frames of incandescent grit
hz_internal CodFxPart cod_fx_grit_flash(void) {
  return (CodFxPart){.tile = CF_TILE_FLASH_CORE,
                     .count = 1,
                     .life = {0.07f, 0.0f},
                     .drag = {6.0f, 0.0f},
                     .size0 = {0.045f, 0.0f},
                     .size1 = {0.19f, 0.0f},
                     .size_curve = 0.42f,
                     .c0 = {1.0f, 0.72f, 0.4f},
                     .c1 = {1.0f, 0.4f, 0.12f},
                     .i0 = 10.0f,
                     .alpha_curve = 0.7f,
                     .soft = 0.25f,
                     .flags = CF_FLAG_GAIN,
                     .noff = {0.01f, 0.0f},
                     .additive = true,
                     .scale_mask = CF_E};
}

