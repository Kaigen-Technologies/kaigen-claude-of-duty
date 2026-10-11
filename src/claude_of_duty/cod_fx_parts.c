// Claude of Duty effects: the particle part and the registration plan that turns parts into vfx elements and table rows.

// the 16 sprite tiles of the particle atlas
typedef enum CodFxTile {
  CF_TILE_SMOKE_A = 0,
  CF_TILE_SMOKE_B,
  CF_TILE_WISP,
  CF_TILE_DUST,
  CF_TILE_SPARK,
  CF_TILE_STREAK,
  CF_TILE_FLASH_LOBE,
  CF_TILE_FLASH_CORE,
  CF_TILE_CHIP,
  CF_TILE_SPLINTER,
  CF_TILE_DROPLET,
  CF_TILE_MIST,
  CF_TILE_SPLASH,
  CF_TILE_RING,
  CF_TILE_FIRE,
  CF_TILE_MOTE,
} CodFxTile;

// the axis a part's cone is about; the burst's frame is the surface normal, its alternate axis rides in the play's user
typedef enum CodFxAxis {
  CF_AXIS_NORMAL = 0,
  CF_AXIS_ALT,     // the reflected ray, the bore, the spray
  CF_AXIS_UP,
  CF_AXIS_ALT_NEG,
} CodFxAxis;

#define CF_FLAG_FLICKER 1u
#define CF_FLAG_LOBE 2u
#define CF_FLAG_GAIN 4u
#define CF_FLAG_EMBER 8u
#define CF_FLAG_EMBER_ALPHA 16u
#define CF_FLAG_SPEED_DEBRIS 32u
#define CF_FLAG_TURB_ABS 64u
#define CF_FLAG_ALPHA_GAIN 128u

#define CF_ROW_VEC4S 20u
#define CF_TABLE_ROWS 200u
#define CF_ATLAS_COLS 4u
#define CF_VFX_ALL (VFX_SCALE_SPEED | VFX_SCALE_SIZE | VFX_SCALE_HEIGHT | VFX_SCALE_RADIUS)

// one call of the spawner as a distribution (size = FULL quad width, m); zero takes the spawner's default where it has one
typedef struct CodFxPart {
  u32 tile;
  u32 tile2;       // another tile this share of the particles draw instead
  f32 tile2_frac;
  f32 count;       // particles per play at the ultra budget
  v2 life;
  v2 delay;        // per-particle random wait before it shows
  v2 drag;         // the exponential velocity decay rate k (dv/dt = -kv + g), drawn over the range
  f32 gravity;     // m/s^2 along +Y (buoyant positive)
  v2 size0, size1; // begin and end width ranges
  f32 size1_ratio; // > 0: the end width is the begin width times this
  f32 size_curve;  // size follows n^curve
  v3 c0, c1;       // colour at birth and death (linear)
  f32 i0, i1;      // intensity at birth and death (follows n^2)
  f32 i0_jit;      // +- share of i0 drawn per particle
  f32 gate;        // > 0: the part exists in this share of plays
  v2 alpha;        // opacity range, times (1 - n)^alpha_curve
  f32 alpha_curve;
  f32 soft;        // metres over which it fades into the geometry it crosses
  f32 stretch;     // length = width (1 + stretch |v|), along the screen-space velocity
  f32 turb, turb_freq;
  u32 flags;
  u32 axis;        // CodFxAxis
  f32 spread;      // cone half-angle, radians
  f32 power;       // > 1 biases toward the axis
  v2 pitch;        // a hollow cone: the angle off the axis ranges over this
  v2 speed;        // m/s along the direction
  v3 vel_add;      // m/s added in the world
  f32 scatter;     // +- m/s of horizontal speed
  v2 noff;         // spawn offset along the surface normal
  f32 disc;        // a disc about the normal the spawn is drawn in
  v2 up_off;       // spawn offset along world up, times the play's height scale
  v2 dir_off;      // spawn offset along the sampled direction, times the play's radius scale
  f32 root_off;    // a lobe: the centre slides out along its heading by this much of its end width
  v3 axis_scale;   // per-axis scale of the sampled direction (0 = 1)
  f32 hemi;        // > 0: the direction is mirrored out of the surface to at least this cosine
  f32 clamp_cos;   // > 0: no wider than this cosine off the normal
  b32 rot_random;  // the stamped rotation is drawn over a full turn
  f32 rot;
  f32 rot_jit;     // +- radians drawn around rot
  v2 spin;         // rad/s
  f32 sun_base, sun_gain; // the colour scales by base + gain * (direction . sun): the sub-puffs thrown into the light start pale
  b32 additive;    // blended additively, not over
  b32 lit;         // shaded by the sky and the sun
  u32 bounces;     // floor contacts it survives
  f32 bounce_rest; // energy kept per bounce
  v2 bounce_life;
  f32 bounce_i;    // intensity multiplier after a bounce
  v3 bounce_c;     // the colour it cools to
  u32 scale_mask;  // VFX_SCALE_*: what a play's scale multiplies; 0 = the size
  f32 pool_plays;  // plays the pool holds at once; 0 = 12
} CodFxPart;

typedef struct CodFxRow {
  v4 v[CF_ROW_VEC4S];
} CodFxRow;

#define CF_PLAN_ASSETS 48

typedef struct CodFxPlanAsset {
  VfxDesc *desc;
  HzHandleT(VfxAsset) *out;
} CodFxPlanAsset;

// the effects registered together: their descs, the table rows their particle elements select and the pool the one material must hold
typedef struct CodFxPlan {
  Allocator *alloc;
  CodFxPlanAsset assets[CF_PLAN_ASSETS];
  u32 asset_count;
  CodFxRow rows[CF_TABLE_ROWS];
  u32 row_count;
  u32 pool_sum;
  u32 element_count;
} CodFxPlan;

hz_internal v4 cf_v4(v3 a, f32 w) { return (v4){a.x, a.y, a.z, w}; }

hz_internal f32 cf_or(f32 v, f32 d) { return v != 0.0f ? v : d; }

// one table row for a part: the shader's CF_R_* layout
hz_internal CodFxRow cod_fx_row(const CodFxPart *in) {
  CodFxPart p = *in;
  if (p.life.x == 0.0f && p.life.y == 0.0f) p.life = (v2){1.0f, 1.0f};
  if (p.drag.x == 0.0f && p.drag.y == 0.0f) p.drag = (v2){1.4f, 1.4f};
  if (p.drag.y == 0.0f) p.drag.y = p.drag.x;
  p.size_curve = cf_or(p.size_curve, 1.0f);
  if (p.c0.x == 0.0f && p.c0.y == 0.0f && p.c0.z == 0.0f) p.c0 = (v3){1.0f, 1.0f, 1.0f};
  if (p.c1.x == 0.0f && p.c1.y == 0.0f && p.c1.z == 0.0f) p.c1 = (v3){1.0f, 1.0f, 1.0f};
  p.i0 = cf_or(p.i0, 1.0f);
  if (p.alpha.x == 0.0f && p.alpha.y == 0.0f) p.alpha = (v2){1.0f, 1.0f};
  p.alpha_curve = cf_or(p.alpha_curve, 1.0f);
  p.soft = cf_or(p.soft, 0.4f);
  p.power = cf_or(p.power, 1.0f);
  if (p.axis_scale.x == 0.0f && p.axis_scale.y == 0.0f && p.axis_scale.z == 0.0f) p.axis_scale = (v3){1.0f, 1.0f, 1.0f};
  if (p.size0.y == 0.0f) p.size0.y = p.size0.x;
  if (p.size1.y == 0.0f) p.size1.y = p.size1.x;
  if (p.noff.y == 0.0f) p.noff.y = p.noff.x;
  if (p.speed.y == 0.0f) p.speed.y = p.speed.x;
  if (p.up_off.y == 0.0f) p.up_off.y = p.up_off.x;
  if (p.dir_off.y == 0.0f) p.dir_off.y = p.dir_off.x;
  if (p.alpha.y == 0.0f) p.alpha.y = p.alpha.x;
  b32 ratio = p.size1_ratio > 0.0f;
  CodFxRow r = {0};
  r.v[0] = (v4){p.drag.x, p.gravity, p.size_curve, p.alpha_curve};
  r.v[1] = cf_v4(p.c0, p.i0);
  r.v[2] = cf_v4(p.c1, p.i1);
  r.v[3] = (v4){p.size0.x, p.size0.y, ratio ? p.size1_ratio : p.size1.x, p.size1.y};
  r.v[4] = (v4){ratio ? 1.0f : 0.0f, p.stretch, p.turb, p.turb_freq};
  r.v[5] = (v4){p.alpha.x, p.alpha.y, p.soft, (f32)p.tile};
  r.v[6] = (v4){p.spread, p.power, p.pitch.x, p.pitch.y};
  r.v[7] = (v4){p.speed.x, p.speed.y, (f32)p.axis, (f32)p.flags};
  r.v[8] = (v4){p.noff.x, p.noff.y, p.disc, p.up_off.x};
  r.v[9] = (v4){p.vel_add.x, p.vel_add.y, p.vel_add.z, p.up_off.y};
  r.v[10] = (v4){p.rot_random ? 0.0f : 1.0f, p.rot, p.spin.x, p.spin.y};
  r.v[11] = (v4){p.delay.x, p.delay.y, p.additive ? 1.0f : 0.0f, p.lit ? 1.0f : 0.0f};
  r.v[12] = (v4){p.sun_base, p.sun_gain, p.root_off, p.dir_off.x};
  r.v[13] = cf_v4(p.axis_scale, p.dir_off.y);
  r.v[14] = (v4){p.scatter, p.hemi > 0.0f ? p.hemi : -1.0f, p.clamp_cos, (f32)p.bounces};
  r.v[15] = (v4){cf_or(p.bounce_rest, 0.32f), cf_or(p.bounce_life.x, 0.18f), cf_or(p.bounce_life.y, 0.4f), cf_or(p.bounce_i, 0.55f)};
  r.v[16] = cf_v4(p.bounce_c, 0.0f);
  r.v[17] = (v4){(f32)p.tile2, p.tile2_frac, p.drag.y, p.rot_jit};
  r.v[18] = (v4){p.i0_jit, p.gate, 0.0f, 0.0f};
  return r;
}

// the element of a part: a custom-material particle element whose user selects the table row; counts are placed exactly
hz_internal VfxElemDesc cod_fx_part_elem(CodFxPlan *plan, const CodFxPart *in, HzHandleT(ParticleMaterial) mat) {
  assert_msg(plan->row_count < CF_TABLE_ROWS, "cod_fx: the table has % rows", fmt_u32(CF_TABLE_ROWS));
  u32 row = plan->row_count++;
  plan->rows[row] = cod_fx_row(in);
  f32 life_hi = in->life.y > 0.0f ? in->life.y : (in->life.x > 0.0f ? in->life.x : 1.0f);
  f32 life_lo = in->life.x > 0.0f ? in->life.x : life_hi;
  // a delayed particle lives its delay on top: the pool is sized by the count, the drain is not used (the emitters never die)
  u32 pool = (u32)(in->count * cf_or(in->pool_plays, 12.0f) * 1.25f) + 8u;
  plan->pool_sum += pool;
  plan->element_count++;
  return (VfxElemDesc){
      .kind = VFX_ELEM_PARTICLES,
      .material = mat,
      .user = row,
      .count = in->count,
      .fixed = true,
      .life = {life_lo, life_hi},
      .speed = {1.0f, 1.0f},
      .size = {1.0f, 1.0f},
      .radius = 1.0f,
      .height = {1.0f, 1.0f},
      .scale_mask = in->scale_mask ? in->scale_mask : VFX_SCALE_SIZE,
      .pool = pool,
      .stops = 1,
      .color = {{0.0f, {1.0f, 1.0f, 1.0f, 128.0f / 255.0f}}}, // the burst's default gain 1: its alpha channel carries gain / 2
  };
}

// a new effect in the plan; the handle lands in *out when the plan is registered
hz_internal VfxDesc *cod_fx_plan_asset(CodFxPlan *plan, const char *name, HzHandleT(VfxAsset) *out) {
  assert_msg(plan->asset_count < CF_PLAN_ASSETS, "cod_fx: the plan holds % effects", fmt_u32(CF_PLAN_ASSETS));
  VfxDesc *d = ALLOC(plan->alloc, VfxDesc);
  d->name = name;
  plan->assets[plan->asset_count++] = (CodFxPlanAsset){d, out};
  return d;
}

// the placeholder material every element carries until the plan's material exists
hz_internal void cod_fx_plan_part(CodFxPlan *plan, VfxDesc *d, const CodFxPart *p) {
  assert_msg(d->count < VFX_ELEM_MAX, "cod_fx: '%' holds % elements", fmt_cstr(d->name), fmt_u32(VFX_ELEM_MAX));
  d->elem[d->count++] = cod_fx_part_elem(plan, p, (HzHandleT(ParticleMaterial)){0});
}

hz_internal VfxElemDesc *cod_fx_plan_elem(VfxDesc *d, VfxElemDesc e) {
  assert_msg(d->count < VFX_ELEM_MAX, "cod_fx: '%' holds % elements", fmt_cstr(d->name), fmt_u32(VFX_ELEM_MAX));
  d->elem[d->count] = e;
  return &d->elem[d->count++];
}

// the Planckian locus normalised to its brightest channel: the colour a cooling spark goes through
hz_internal v3 cod_fx_blackbody(f32 kelvin) {
  f32 t = m_clampf(kelvin, 1000.0f, 6500.0f) / 100.0f;
  f32 r, g, b;
  if (t <= 66.0f) {
    r = 255.0f;
    g = 99.4708025861f * m_logf(t) - 161.1195681661f;
  } else {
    r = 329.698727446f * m_powf(t - 60.0f, -0.1332047592f);
    g = 288.1221695283f * m_powf(t - 60.0f, -0.0755148492f);
  }
  if (t >= 66.0f) b = 255.0f;
  else if (t <= 19.0f) b = 0.0f;
  else b = 138.5177312231f * m_logf(t - 10.0f) - 305.0447927307f;
  r = m_clampf(r, 0.0f, 255.0f) / 255.0f;
  g = m_clampf(g, 0.0f, 255.0f) / 255.0f;
  b = m_clampf(b, 0.0f, 255.0f) / 255.0f;
  r = srgb_to_linear(r);
  g = srgb_to_linear(g);
  b = srgb_to_linear(b);
  f32 m = m_maxf(m_maxf(r, g), m_maxf(b, 1e-4f));
  return (v3){r / m, g / m, b / m};
}
