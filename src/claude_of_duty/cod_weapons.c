// Claude of Duty weapons: procedural guns and hands, the viewmodel, fire, ballistics, shells and magazines.

#define CWP_MAX_LIVE 96
#define CWP_SHELL_SLOTS 8
#define CWP_GRAVITY 9.81f // the projectile sim's own constant, not the world's 2.1x

// a round in flight
typedef struct CwpProjectile {
  v3 pos;
  v3 prev;
  v3 vel;
  f32 damage;
  f32 penetration;
  f32 drag_k;
  f32 dropoff;
  f32 max_range;
  f32 travelled;
  f32 age;
  u32 weapon;
} CwpProjectile;

// one weapon's ammunition and fire control (the per-weapon `states` entry)
typedef struct CwpSlot {
  u32 mag;
  b32 chambered;
  u32 reserve;
  u32 mode_index;
  u32 mode;
  f32 pattern[CWP_PATTERN_MAX * 2];
  CwpClip clips[CWP_CLIP_COUNT];
} CwpSlot;

// what the viewmodel reads of the player this frame
typedef struct CwpInputs {
  b32 ads;
  b32 sprint;
  b32 low_ready;
  b32 crouch;
  b32 airborne;
  b32 trigger;
  b32 empty;
  f32 speed;
} CwpInputs;

typedef struct CwpScene CwpScene;

// the weapon system's private runtime (WeaponSystem + Viewmodel), one allocation behind the CodWeaponsRig resource
typedef struct CwpRig {
  EcsWorld *world;
  CwpModel *models; // COD_WEAPON_COUNT built guns
  CodRng rng;
  CwpView view;
  CwpSlot slot[COD_WEAPON_COUNT];
  CwpInputs st;
  u32 active;
  i32 switch_to; // -1 none
  f32 fire_timer;
  f32 burst_cooldown;
  f32 spread;
  f32 since_shot;
  u32 burst_left;
  u32 shot_index;
  u32 rounds_fired; // rounds fired this match
  u32 pending_shots;
  u32 fire_seed;
  f32 shell_t[CWP_SHELL_SLOTS]; // seconds until the case leaves the port, < 0 free
  v3 muzzle_world;              // the viewmodel muzzle and bore as of the last update, where the next round starts
  v3 bore_dir;
  quat bore_rot;
  CwpProjectile live[CWP_MAX_LIVE];
  u32 live_count;
  i32 applied_weapon; // the dev weapon switch last applied
  // what the update hands to the other systems
  f32 recoil_pitch;
  f32 recoil_yaw;
  f32 recoil_roll;
  f32 recoil_punch;
  CodWeaponEvents *events; // CodWeaponState.events while the update runs
  v3 player_velocity;
  v3 cam_pos;
  quat cam_rot;
  b32 cam_valid;
  f32 reload_progress;
  f32 time;
  b32 ads_was; // the aim wish last frame, for the foley edges
  Allocator persist;
  CwpArm *arm;             // [0] right, [1] left
  ArenaAllocator *arena[5]; // build outputs: the three guns, then the two arms
  u32 stage;                // CwpStage
  b32 ready;                // meshes built, fitted and spawned
  f64 build_ms;
  u64 stage_t0;
  void *grenades; // CwpGrenadeState
  CwpScene *scene;
} CwpRig;

typedef enum CwpStage {
  CWP_STAGE_BUILD = 0, // every lane builds the guns and arms (cod_weapons_lanes)
  CWP_STAGE_FIT,       // the main lane fits the support hand, bakes contact AO, builds the clips
  CWP_STAGE_UPLOAD,    // materials ready: meshes to the GPU, entities spawned
  CWP_STAGE_DONE,
} CwpStage;

ECS_COMPONENT_TYPEDEF(CodWeaponsRig);
typedef struct CodWeaponsRig {
  CwpRig *rig;
} CodWeaponsRig;

// ---- accessors ----

hz_internal b32 cwp_reloading(const CwpRig *r) { return r->view.clip == CWP_CLIP_RELOAD_TAC || r->view.clip == CWP_CLIP_RELOAD_EMPTY; }
hz_internal b32 cwp_inspecting(const CwpRig *r) { return r->view.clip == CWP_CLIP_INSPECT; }
hz_internal b32 cwp_switching(const CwpRig *r) { return r->switch_to >= 0; }
hz_internal b32 cwp_firing(const CwpRig *r) { return r->since_shot < 0.12f; }

// the weapon-space point in world space as of the last view update
hz_internal v3 cwp_world_point(const CwpRig *r, v3 local) {
  v3 cam = v3_add(r->view.rig_pos, quat_rotate(r->view.rig_rot, local));
  return v3_add(r->cam_pos, quat_rotate(r->cam_rot, cam));
}

hz_internal v3 cwp_world_dir(const CwpRig *r, v3 local) {
  return v3_normalize(quat_rotate(r->cam_rot, quat_rotate(r->view.rig_rot, local)));
}

// ---- weapon management ----

// a weapon switch: the animation stack restarts on the new weapon
hz_internal void cwp_set_active(CwpRig *r, u32 id, b32 fitted) {
  r->active = id;
  cwp_view_reset(&r->view);
  r->view.hand_pose_rest = fitted ? CWP_POSE_FITTED + id : CWP_POSE_CUP;
}

hz_internal b32 cwp_set_weapon(CwpRig *r, u32 id) {
  if (id >= COD_WEAPON_COUNT || id == r->active || cwp_switching(r))
    return false;
  r->switch_to = (i32)id;
  cwp_view_play(&r->view, CWP_CLIP_HOLSTER);
  return true;
}

hz_internal void cwp_cycle_fire_mode(CwpRig *r, CodWeaponEvents *ev) {
  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  if (def->mode_count < 2)
    return;
  s->mode_index = (s->mode_index + 1) % def->mode_count;
  s->mode = def->modes[s->mode_index];
  r->burst_left = 0;
  ev->fire_mode_changed = true;
}

hz_internal b32 cwp_reload(CwpRig *r) {
  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  if (cwp_reloading(r) || cwp_switching(r))
    return false;
  if (s->mag >= def->mag_size || s->reserve == 0)
    return false;
  cwp_view_stop_clip(&r->view);
  b32 empty = s->mag == 0 && !s->chambered;
  cwp_view_play(&r->view, empty ? CWP_CLIP_RELOAD_EMPTY : CWP_CLIP_RELOAD_TAC);
  return true;
}

hz_internal b32 cwp_inspect(CwpRig *r) {
  if (cwp_reloading(r) || cwp_switching(r) || cwp_inspecting(r))
    return false;
  cwp_view_play(&r->view, CWP_CLIP_INSPECT);
  return true;
}

hz_internal b32 cwp_can_fire(const CwpRig *r) {
  if (cwp_reloading(r) || cwp_switching(r) || r->fire_timer > 0.0f)
    return false;
  return r->slot[r->active].chambered;
}

// a tactical reload keeps the chambered round and gives mag_size + 1; an empty one feeds a round out of the fresh magazine
hz_internal void cwp_complete_reload(CwpRig *r, b32 empty) {
  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  u32 want = def->mag_size - s->mag;
  u32 take = want < s->reserve ? want : s->reserve;
  s->reserve -= take;
  s->mag += take;
  if (empty && !s->chambered && s->mag > 0) {
    s->mag--;
    s->chambered = true;
  }
  r->shot_index = 0;
}

hz_internal void cwp_emit_reload(CwpRig *r, u32 phase) {
  CodWeaponEvents *ev = r->events;
  ev->reload = true;
  ev->reload_phase = phase;}

// ---- projectiles ----

hz_internal void cwp_push_round(CwpRig *r, const CodRound *round) {
  CodWeaponEvents *ev = r->events;
  if (ev->round_count < COD_WEAPON_MAX_ROUNDS)
    ev->rounds[ev->round_count++] = *round;}

// one tracer per few rounds: the muzzle to wherever the round will land
hz_internal void cwp_emit_tracer(EcsWorld *w, CwpRig *r, const CwpProjectile *p, v3 dir, f32 speed) {
  CwbQuery q = cwb_query_make(w);
  f32 dist = m_minf(p->max_range, 260.0f);
  CwbHit hit;
  if (cwb_ray(&q, p->pos, dir, dist, &hit))
    dist = hit.distance;
  CodRound tr = {0};
  tr.shooter.player = true;
  tr.weapon = r->active;
  tr.origin = p->pos;
  tr.dir = dir;
  tr.tracer = (CodTracer){p->pos, v3_add(p->pos, v3_scale(dir, dist)), speed};
  tr.has_tracer = true;
  tr.player_miss = -1.0f;
  cwp_push_round(r, &tr);
}

hz_internal void cwp_spawn_projectile(EcsWorld *w, CwpRig *r, v3 origin, v3 dir, const CwpDef *def, b32 tracer) {
  CwpProjectile *p;
  if (r->live_count < CWP_MAX_LIVE) {
    p = &r->live[r->live_count++];
  } else {
    // the oldest round yields its slot rather than dropping the shot
    for (u32 i = 1; i < CWP_MAX_LIVE; i++)
      r->live[i - 1] = r->live[i];
    p = &r->live[CWP_MAX_LIVE - 1];
  }
  *p = (CwpProjectile){.pos = origin,
                       .prev = origin,
                       .vel = v3_scale(dir, def->muzzle_velocity),
                       .damage = def->damage,
                       .penetration = def->penetration,
                       .drag_k = def->drag_k,
                       .dropoff = def->dropoff,
                       .max_range = def->max_range,
                       .weapon = r->active};
  r->rounds_fired++;
  if (tracer)
    cwp_emit_tracer(w, r, p, dir, def->muzzle_velocity);
}

// the fixed step of every round in flight; a contact hands the round to the penetration solver
hz_internal void cwp_step_projectiles(EcsWorld *w, CwpRig *r, f32 h) {
  if (r->live_count == 0)
    return;
  CwbQuery q = cwb_query_make(w);
  for (i32 i = (i32)r->live_count - 1; i >= 0; i--) {
    CwpProjectile *p = &r->live[i];
    p->prev = p->pos;
    p->vel.y -= CWP_GRAVITY * h;
    p->vel = v3_scale(p->vel, m_maxf(0.0f, 1.0f - p->drag_k * h));
    p->pos = v3_add(p->pos, v3_scale(p->vel, h));
    p->age += h;
    v3 seg = v3_sub(p->pos, p->prev);
    f32 seg_len = v3_length(seg);
    p->travelled += seg_len;
    b32 dead = false;
    if (seg_len > 1e-6f) {
      v3 dir = v3_scale(seg, 1.0f / seg_len);
      CwbHit hit;
      if (cwb_ray(&q, p->prev, dir, seg_len, &hit)) {
        f32 range01 = m_minf(1.0f, p->travelled / p->max_range);
        f32 falloff = 1.0f - (1.0f - p->dropoff) * range01 * range01;
        CodRound round = {0};
        round.shooter.player = true;
        round.weapon = p->weapon;
        round.origin = p->prev;
        round.dir = dir;
        round.dropoff = 1.0f;
        cod_ballistics_fire(w, &round, p->damage * falloff, p->penetration,
                            m_minf(24.0f, m_maxf(1.5f, p->max_range - p->travelled + seg_len)));
        cwp_push_round(r, &round);
        cod_ballistics_apply_impulses(w, &round);
        dead = true;
      }
    }
    if (!dead && (p->travelled > p->max_range || p->age > 5.0f || p->pos.y < -80.0f))
      dead = true;
    if (dead) {
      r->live[i] = r->live[r->live_count - 1];
      r->live_count--;
    }
  }
}

// ---- firing ----

hz_internal void cwp_queue_shell(CwpRig *r, f32 delay) {
  for (u32 i = 0; i < CWP_SHELL_SLOTS; i++) {
    if (r->shell_t[i] < 0.0f) {
      r->shell_t[i] = delay;
      return;
    }
  }
}

// one round leaves the barrel; false when the trigger clicked dry
hz_internal b32 cwp_try_fire(EcsWorld *w, CwpRig *r) {
  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  CodWeaponEvents *ev = r->events;
  if (cwp_reloading(r) || cwp_switching(r) || r->fire_timer > 0.0f)
    return false;
  if (!s->chambered) {
    // dry: lock the bolt back and let the player know by feel
    r->view.bolt_hold = 1.0f;
    r->fire_timer = 0.25f;
    ev->dry_fire = true;
    return false;
  }
  if (cwp_inspecting(r))
    cwp_view_stop_clip(&r->view);

  b32 first = r->since_shot > 0.35f;
  // feed the next round
  s->chambered = false;
  if (s->mag > 0) {
    s->mag--;
    s->chambered = true;
  } else {
    r->view.bolt_hold = 1.0f;
  }

  // the deterministic recoil pattern
  u32 idx = r->shot_index < def->recoil.pattern_length - 1 ? r->shot_index : def->recoil.pattern_length - 1;
  f32 pitch = s->pattern[idx * 2], yaw = s->pattern[idx * 2 + 1];
  r->shot_index++;

  // aim: the camera forward plus a spread cone
  v3 dir = quat_rotate(r->cam_rot, V3_BACK);
  f32 spread_rad = m_radians(r->spread);
  if (spread_rad > 1e-5f) {
    v2 d = cod_rng_disc(&r->rng);
    v3 right = quat_rotate(r->cam_rot, (v3){1.0f, 0.0f, 0.0f});
    v3 up = quat_rotate(r->cam_rot, (v3){0.0f, 1.0f, 0.0f});
    f32 tn = m_tanf(spread_rad);
    dir = v3_normalize(v3_add(dir, v3_add(v3_scale(right, tn * d.x), v3_scale(up, tn * d.y))));
  }

  // the projectile starts at the viewmodel muzzle
  u32 seed = cod_rng_u32(&r->rng);
  b32 tracer = (r->rounds_fired % def->tracer_every) == 0;
  cwp_spawn_projectile(w, r, r->muzzle_world, dir, def, tracer);

  // feedback: the camera climb is the learnable part, the viewmodel kick is the feel
  cwp_view_add_recoil(&r->view, def, pitch, yaw, first);
  r->recoil_pitch += pitch;
  r->recoil_yaw += yaw;
  r->recoil_roll += def->recoil.roll * 0.35f;
  r->recoil_punch += def->recoil.punch;
  r->spread = m_minf(def->spread_max, r->spread + def->spread_per_shot);
  r->fire_timer = 60.0f / def->rpm;
  r->since_shot = 0.0f;
  r->pending_shots++;
  r->fire_seed = seed;  // the case leaves the port shortly after the shot, once the bolt is back
  cwp_queue_shell(r, m_minf(0.05f, r->fire_timer * 0.45f));
  return true;
}

// the fire-mode state machine
hz_internal void cwp_run_trigger(EcsWorld *w, CwpRig *r, b32 held, b32 pressed) {
  CwpSlot *s = &r->slot[r->active];
  const CwpDef *def = cwp_def(r->active);
  switch (s->mode) {
  case COD_FIRE_AUTO:
    if (held)
      cwp_try_fire(w, r);
    break;
  case COD_FIRE_BURST:
    if (pressed && r->burst_left == 0 && r->burst_cooldown <= 0.0f)
      r->burst_left = def->burst_count;
    if (r->burst_left > 0 && r->fire_timer <= 0.0f) {
      if (cwp_try_fire(w, r)) {
        r->burst_left--;
        r->fire_timer = 60.0f / def->burst_rpm;
        if (r->burst_left == 0)
          r->burst_cooldown = def->burst_delay;
      } else {
        r->burst_left = 0;
      }
    }
    break;
  default:
    if (pressed)
      cwp_try_fire(w, r);
    break;
  }
}

// the resting cone for the stance and speed
hz_internal f32 cwp_rest_spread(const CwpRig *r, const CwpDef *def, const CodPlayerState *ps) {
  f32 base = m_lerpf(def->spread_hip, def->spread_ads, r->view.ads_t);
  if (r->st.crouch)
    base *= CWP_SPREAD_CROUCH;
  if (ps->stance == COD_STANCE_PRONE)
    base *= CWP_SPREAD_PRONE;
  if (r->st.speed < 0.4f)
    base *= CWP_SPREAD_STILL;
  else if (r->st.speed > 3.2f)
    base *= CWP_SPREAD_WALKING;
  if (r->st.sprint)
    base *= CWP_SPREAD_SPRINTING;
  if (r->st.airborne)
    base *= CWP_SPREAD_AIRBORNE;
  return base;
}

// ---- clip beats ----

hz_internal void cwp_drop_magazine(CwpRig *r);

hz_internal void cwp_on_clip_event(void *user, u32 event, i32 clip) {
  CwpRig *r = user;
  b32 is_reload = clip == CWP_CLIP_RELOAD_TAC || clip == CWP_CLIP_RELOAD_EMPTY;
  switch (event) {
  case CWP_EV_START:
    if (is_reload)
      cwp_emit_reload(r, COD_RELOAD_START);
    break;
  case CWP_EV_MAGOUT:
    if (is_reload)
      cwp_emit_reload(r, COD_RELOAD_MAG_OUT);
    break;
  case CWP_EV_MAGDROP:
    if (is_reload)
      cwp_drop_magazine(r);
    break;
  case CWP_EV_MAGIN:
    if (is_reload) {
      cwp_emit_reload(r, COD_RELOAD_MAG_IN);
      cwp_complete_reload(r, clip == CWP_CLIP_RELOAD_EMPTY);
    }
    break;
  case CWP_EV_BOLTRELEASE:
    r->view.bolt_hold = 0.0f;
    break;
  case CWP_EV_END:
    if (is_reload) {
      cwp_emit_reload(r, COD_RELOAD_END);
      r->view.bolt_hold = 0.0f;
    }
    if (clip == CWP_CLIP_HOLSTER && r->switch_to >= 0) {
      cwp_set_active(r, (u32)r->switch_to, r->switch_to != COD_WEAPON_PISTOL);
      r->switch_to = -1;
      cwp_view_play(&r->view, CWP_CLIP_DRAW);
      r->shot_index = 0;
      r->spread = 0.0f;
      r->events->swapped = true;
    }
    break;
  default:
    break;
  }
}

#include "cod_weapons_scene.c"
#include "cod_weapons_grenade.c"
#include "cod_weapons_update.c"
