#include "character.h"
#include "ecs/ecs_assets/ecs_assets.h"
#include "ecs/ecs_rigidbody/ecs_rigidbody.h"
#include "lib/assert.h"

#define CHAR_SKIN 0.02f  // CharacterVirtual::mCharacterPadding: the gap kept to every surface, so sweeps never start in contact
#define CHAR_PROBE 0.06f // contacts within this gap are touching: the resting floor is support without a sweep
#define CHAR_ONE_WAY_TOL 0.05f
#define CHAR_SLIDE_ITERS 5
#define CHAR_DEPEN_ITERS 6
#define CHAR_NEAR_MAX 16 // nearby contact planes a slide respects
#define CHAR_INTO_EPS 1e-3f
#define CHAR_LAND_MIN_SPEED 1.0f
#define CHAR_SEPARATION_SPEED 0.5f // m/s along a floor's normal that leaves it (a jump); slower is moving along it
#define CHAR_LANDED_HOLD_FRAMES 3
#define CHAR_DEFAULT_MODEL "gameplay_blocks/character/default_character.glb"
#define CHAR_DEFAULT_IDLE "gameplay_blocks/character/default_character/Idle_Loop.hza"
#define CHAR_DEFAULT_RUN "gameplay_blocks/character/default_character/Sprint_Loop.hza"
#define CHAR_DEFAULT_FALL "gameplay_blocks/character/default_character/Jump_Loop.hza"

// runtime animation driver state, attached by character_avatar_add
typedef struct CharacterAvatar {
  EcsEntity avatar;
  HzHandleT(AnimationBlobAsset) idle;
  HzHandleT(AnimationBlobAsset) run;
  HzHandleT(AnimationBlobAsset) fall;
  f32 run_lo;
  f32 run_hi;
  b32 run_dominant;
  b32 was_airborne;
  u32 landed_hold;
} CharacterAvatar;
ECS_COMPONENT_TYPEDEF(CharacterAvatar);

#define CHAR_DEFAULT(field, value) \
  if ((field) <= 0.0f)             \
  (field) = (value)

CharacterTuning character_tuning_resolve(CharacterTuning t) {
  CHAR_DEFAULT(t.radius, 0.35f);
  CHAR_DEFAULT(t.height, 1.8f);
  CHAR_DEFAULT(t.eye_height, 1.65f);
  CHAR_DEFAULT(t.walk_speed, 6.0f);
  CHAR_DEFAULT(t.run_speed, t.walk_speed);
  CHAR_DEFAULT(t.accel_time, 0.1f);
  CHAR_DEFAULT(t.decel_time, 0.1f);
  CHAR_DEFAULT(t.air_accel_time, 0.4f);
  CHAR_DEFAULT(t.air_decel_time, 0.4f);
  CHAR_DEFAULT(t.ground_accel, 11.0f);
  CHAR_DEFAULT(t.air_accel, 2.2f);
  CHAR_DEFAULT(t.friction, 7.5f);
  CHAR_DEFAULT(t.stop_speed, 1.5f);
  CHAR_DEFAULT(t.move_damping, 14.0f);
  CHAR_DEFAULT(t.air_control, 0.35f);
  CHAR_DEFAULT(t.gravity, 36.0f);
  CHAR_DEFAULT(t.max_fall_speed, 55.0f);
  CHAR_DEFAULT(t.jump_height, 1.8f);
  CHAR_DEFAULT(t.coyote_time, 0.1f);
  CHAR_DEFAULT(t.jump_buffer, 0.15f);
  CHAR_DEFAULT(t.slope_limit_deg, 50.0f);
  CHAR_DEFAULT(t.step_height, 0.35f);
  CHAR_DEFAULT(t.snap_distance, 0.35f);
  CHAR_DEFAULT(t.turn_rate, 12.0f);
  CHAR_DEFAULT(t.footstep_walk, 2.1f);
  CHAR_DEFAULT(t.footstep_run, 2.6f);
  CHAR_DEFAULT(t.swim_depth, 1.25f);
  CHAR_DEFAULT(t.swim_speed_scale, 0.6f);
  CHAR_DEFAULT(t.buoyancy, 6.0f);
  if (t.crouch_height > 0.0f) {
    t.crouch_height = m_clampf(t.crouch_height, 2.0f * t.radius, t.height);
    CHAR_DEFAULT(t.crouch_eye_height, m_maxf(t.eye_height - (t.height - t.crouch_height), t.radius));
    CHAR_DEFAULT(t.crouch_speed, t.walk_speed * 0.5f);
    CHAR_DEFAULT(t.crouch_ease_s, 0.18f);
  }
  return t;
}

f32 character_stand_height(const CharacterTuning *tuning) { return character_tuning_resolve(*tuning).height; }

f32 character_eye_height(const CharacterTuning *t, const Character *c) {
  if (!c || t->crouch_height <= 0.0f)
    return t->eye_height;
  f32 k = c->crouch_k * c->crouch_k * (3.0f - 2.0f * c->crouch_k);
  return m_lerpf(t->eye_height, t->crouch_eye_height, k);
}

f32 character_planar_speed(const Character *c) { return m_sqrtf(c->velocity.x * c->velocity.x + c->velocity.z * c->velocity.z); }

f32 character_spawn_height(EcsWorld *w, f32 x, f32 z, f32 from_y, f32 max_drop, f32 fallback) {
  const EcsCollisionLayer *s = ecs_physics_get_static_layer(w);
  const EcsCollisionLayer *k = ecs_physics_get_kinematic_layer(w);
  CollisionLayer *layers[2];
  i32 n = 0;
  if (s)
    layers[n++] = (CollisionLayer *)&s->layer;
  if (k)
    layers[n++] = (CollisionLayer *)&k->layer;
  PhysicsRaycastResult r = {0};
  i32 body = -1, li = -1;
  if (n > 0 && physics_raycast_layers((v3){x, from_y, z}, (v3){x, from_y - max_drop, z}, layers, n, PHYSICS_QUERY_FILTER_ALL,
                                      &r, &body, &li))
    return r.position.y;
  return fallback;
}

// ---- queries: the capsule against the static and kinematic layers, one body at a time so one-way platforms can opt out ----

typedef struct CharEnv {
  EcsWorld *w;
  const EcsCollisionLayer *layers[2];
  i32 layer_count;
  Collider capsule; // feet space
  f32 cos_slope;
  f32 ref_feet_y; // feet height at the start of the step: decides one-way platforms
} CharEnv;

typedef struct CharContact {
  b32 touched;
  b32 wall;
  b32 ground;
  f32 ground_ny;
  v3 ground_normal;
  EcsEntity ground_entity;
} CharContact;

typedef struct CharProbe {
  b32 hit;
  b32 walkable;
  f32 dist;
  v3 normal;
  EcsEntity entity;
} CharProbe;

hz_internal CharEnv char_env_make(EcsWorld *w, const CharacterTuning *t, f32 ref_feet_y) {
  CharEnv e = {.w = w, .ref_feet_y = ref_feet_y};
  const EcsCollisionLayer *s = ecs_physics_get_static_layer(w);
  const EcsCollisionLayer *k = ecs_physics_get_kinematic_layer(w);
  if (s)
    e.layers[e.layer_count++] = s;
  if (k)
    e.layers[e.layer_count++] = k;
  e.capsule = collider_capsule((v3){0.0f, t->radius, 0.0f}, (v3){0.0f, t->height - t->radius, 0.0f}, t->radius);
  e.cos_slope = m_cosf(m_radians(t->slope_limit_deg));
  return e;
}

hz_internal b32 char_blocks(const CharEnv *e, const ColliderBody *b, EcsEntity ent) {
  if (!ecs_component_get(e->w, ent, CharacterOneWay))
    return true;
  f32 top = physics_aabb_from(b->collider, b->pos, b->rot, b->scale).max.y;
  return e->ref_feet_y >= top - CHAR_ONE_WAY_TOL;
}

hz_internal aabb char_aabb(const CharEnv *e, v3 a, v3 b) {
  aabb ba = physics_aabb_from(e->capsule, a, quat_identity(), 1.0f);
  aabb bb = physics_aabb_from(e->capsule, b, quat_identity(), 1.0f);
  const f32 pad = CHAR_PROBE + 0.02f;
  return (aabb){{m_minf(ba.min.x, bb.min.x) - pad, m_minf(ba.min.y, bb.min.y) - pad, m_minf(ba.min.z, bb.min.z) - pad},
                {m_maxf(ba.max.x, bb.max.x) + pad, m_maxf(ba.max.y, bb.max.y) + pad, m_maxf(ba.max.z, bb.max.z) + pad}};
}

typedef struct CharDeepCtx {
  const CharEnv *env;
  i32 layer;
  v3 pos;
  f32 depth; // the skin deficit: CHAR_SKIN - gap
  v3 normal;
  EcsEntity entity;
  b32 hit;
} CharDeepCtx;

// the surface eating most into the skin; a gap under half the skin is restored to the full skin
hz_internal void char_deepest_cb(FindObjectsResult fr, void *user) {
  CharDeepCtx *c = user;
  ColliderDistanceResult r = {0};
  if (!physics_distance_between(c->env->capsule, c->pos, quat_identity(), 1.0f, fr.body.collider, fr.body.pos, fr.body.rot,
                                fr.body.scale, CHAR_SKIN, &r) ||
      r.distance >= CHAR_SKIN * 0.5f || CHAR_SKIN - r.distance <= c->depth)
    return;
  EcsEntity ent = c->env->layers[c->layer]->entities[fr.source_index];
  if (!char_blocks(c->env, &fr.body, ent))
    return;
  c->depth = CHAR_SKIN - r.distance;
  c->normal = v3_negate(r.normal_a);
  c->entity = ent;
  c->hit = true;
}

// records a contact normal: walkable ones feed the ground, the rest are walls
hz_internal void char_contact_note(const CharEnv *e, CharContact *ct, v3 n, EcsEntity ent) {
  ct->touched = true;
  if (n.y >= e->cos_slope) {
    if (n.y > ct->ground_ny) {
      ct->ground = true;
      ct->ground_ny = n.y;
      ct->ground_normal = n;
      ct->ground_entity = ent;
    }
  } else {
    ct->wall = true;
  }
}

// Octree-style: the deepest skin violation resolved and re-queried; a steep face pushes horizontally so it never lifts the character
hz_internal b32 char_depenetrate(const CharEnv *e, v3 *pos, v3 *vel, b32 walk, CharContact *ct) {
  b32 moved = false;
  for (u32 iter = 0; iter < CHAR_DEPEN_ITERS; iter++) {
    CharDeepCtx best = {.env = e, .pos = *pos};
    aabb box = char_aabb(e, *pos, *pos);
    for (i32 l = 0; l < e->layer_count; l++) {
      best.layer = l;
      physics_find_objects(box, &e->layers[l]->layer, PHYSICS_QUERY_FILTER_ALL, char_deepest_cb, &best);
    }
    if (!best.hit)
      break;
    v3 n = best.normal;
    f32 hl = m_sqrtf(n.x * n.x + n.z * n.z);
    b32 steep = n.y > 0.0f && n.y < e->cos_slope && hl > 0.05f;
    v3 push = steep ? v3_scale((v3){n.x / hl, 0.0f, n.z / hl}, best.depth / hl) : v3_scale(n, best.depth);
    *pos = v3_add(*pos, push);
    // the velocity into the surface goes with the overlap (Player.resolveCollisions); walkable ground walked on keeps it
    v3 pn = v3_normalize(push);
    f32 vn = v3_dot(*vel, pn);
    if (vn < 0.0f && !(walk && pn.y >= e->cos_slope))
      *vel = v3_sub(*vel, v3_scale(pn, vn));
    moved = true;
    char_contact_note(e, ct, n, best.entity);
  }
  return moved;
}

typedef struct CharCastCtx {
  const CharEnv *env;
  i32 layer;
  v3 from;
  v3 to;
  v3 dir;
  b32 hit;
  ColliderCastResult best;
  EcsEntity entity;
} CharCastCtx;

hz_internal void char_cast_cb(FindObjectsResult fr, void *user) {
  CharCastCtx *c = user;
  ColliderCastResult r = {0};
  if (!physics_collider_cast(c->env->capsule, c->from, quat_identity(), 1.0f, c->to, fr.body.collider, fr.body.pos,
                             fr.body.rot, fr.body.scale, &r))
    return;
  // a surface the character is not moving into is no obstacle: tangent contacts must not stall the slide
  if (v3_dot(r.normal_on_target, c->dir) > -CHAR_INTO_EPS || (c->hit && r.distance >= c->best.distance))
    return;
  EcsEntity ent = c->env->layers[c->layer]->entities[fr.source_index];
  if (!char_blocks(c->env, &fr.body, ent))
    return;
  c->best = r;
  c->entity = ent;
  c->hit = true;
}

// closest obstacle along from -> to
hz_internal b32 char_cast(const CharEnv *e, v3 from, v3 to, ColliderCastResult *out, EcsEntity *ent) {
  v3 d = v3_sub(to, from);
  f32 len = v3_length(d);
  if (len < 1e-6f)
    return false;
  CharCastCtx c = {.env = e, .from = from, .to = to, .dir = v3_scale(d, 1.0f / len)};
  aabb box = char_aabb(e, from, to);
  for (i32 l = 0; l < e->layer_count; l++) {
    c.layer = l;
    physics_find_objects(box, &e->layers[l]->layer, PHYSICS_QUERY_FILTER_ALL, char_cast_cb, &c);
  }
  if (c.hit) {
    *out = c.best;
    *ent = c.entity;
  }
  return c.hit;
}

hz_internal CharProbe char_probe(const CharEnv *e, v3 pos, f32 reach) {
  CharProbe p = {0};
  if (reach <= 0.0f)
    return p;
  ColliderCastResult r = {0};
  EcsEntity ent = 0;
  if (!char_cast(e, pos, v3_add(pos, (v3){0.0f, -reach, 0.0f}), &r, &ent))
    return p;
  p.hit = true;
  p.dist = r.distance;
  p.normal = r.normal_on_target;
  p.entity = ent;
  p.walkable = r.normal_on_target.y >= e->cos_slope;
  return p;
}

typedef struct CharGroundCtx {
  const CharEnv *env;
  i32 layer;
  v3 pos;
  CharProbe walk;  // the flattest walkable contact
  CharProbe steep; // the flattest too-steep upward contact
} CharGroundCtx;

hz_internal void char_ground_cb(FindObjectsResult fr, void *user) {
  CharGroundCtx *c = user;
  ColliderDistanceResult r = {0};
  if (!physics_distance_between(c->env->capsule, c->pos, quat_identity(), 1.0f, fr.body.collider, fr.body.pos, fr.body.rot,
                                fr.body.scale, CHAR_PROBE, &r) ||
      r.distance > CHAR_PROBE)
    return;
  v3 n = v3_negate(r.normal_a);
  if (n.y <= 1e-3f)
    return;
  EcsEntity ent = c->env->layers[c->layer]->entities[fr.source_index];
  if (!char_blocks(c->env, &fr.body, ent))
    return;
  b32 walkable = n.y >= c->env->cos_slope;
  CharProbe *slot = walkable ? &c->walk : &c->steep;
  if (slot->hit && n.y <= slot->normal.y)
    return;
  *slot = (CharProbe){.hit = true, .walkable = walkable, .dist = r.distance, .normal = n, .entity = ent};
}

// CharacterVirtual::GetContactsAtPosition: the surfaces touching the capsule from below; walkable wins over steep
hz_internal CharProbe char_ground_contact(const CharEnv *e, v3 pos) {
  CharGroundCtx c = {.env = e, .pos = pos};
  aabb box = char_aabb(e, pos, pos);
  for (i32 l = 0; l < e->layer_count; l++) {
    c.layer = l;
    physics_find_objects(box, &e->layers[l]->layer, PHYSICS_QUERY_FILTER_ALL, char_ground_cb, &c);
  }
  return c.walk.hit ? c.walk : c.steep;
}

typedef struct CharNear {
  v3 normal;
  f32 gap;
  EcsEntity entity;
} CharNear;

typedef struct CharNearCtx {
  const CharEnv *env;
  i32 layer;
  v3 pos;
  CharNear *items;
  u32 count;
} CharNearCtx;

hz_internal void char_near_cb(FindObjectsResult fr, void *user) {
  CharNearCtx *c = user;
  ColliderDistanceResult r = {0};
  if (c->count >= CHAR_NEAR_MAX ||
      !physics_distance_between(c->env->capsule, c->pos, quat_identity(), 1.0f, fr.body.collider, fr.body.pos, fr.body.rot,
                                fr.body.scale, CHAR_PROBE, &r) ||
      r.distance > CHAR_PROBE)
    return;
  EcsEntity ent = c->env->layers[c->layer]->entities[fr.source_index];
  if (!char_blocks(c->env, &fr.body, ent))
    return;
  c->items[c->count++] = (CharNear){.normal = v3_negate(r.normal_a), .gap = r.distance, .entity = ent};
}

// CharacterVirtual::GetContactsAtPosition with the predictive distance: the planes the sweep cannot see because it starts inside their padding
hz_internal u32 char_near_contacts(const CharEnv *e, v3 pos, CharNear *out) {
  CharNearCtx c = {.env = e, .pos = pos, .items = out};
  aabb box = char_aabb(e, pos, pos);
  for (i32 l = 0; l < e->layer_count; l++) {
    c.layer = l;
    physics_find_objects(box, &e->layers[l]->layer, PHYSICS_QUERY_FILTER_ALL, char_near_cb, &c);
  }
  return c.count;
}

// removes the motion into plane n, sliding along the crease with an earlier plane it now cuts into
hz_internal void char_slide_plane(v3 *rem, v3 *vel, v3 *planes, u32 *np, v3 n) {
  *rem = v3_sub(*rem, v3_scale(n, v3_dot(*rem, n)));
  f32 vn = v3_dot(*vel, n);
  if (vn < 0.0f)
    *vel = v3_sub(*vel, v3_scale(n, vn));
  for (u32 p = 0; p < *np; p++) {
    if (v3_dot(*rem, planes[p]) < -1e-4f) {
      v3 crease = v3_cross(planes[p], n);
      f32 cl = v3_length(crease);
      if (cl < 1e-4f) {
        *rem = V3_ZERO;
      } else {
        crease = v3_scale(crease, 1.0f / cl);
        *rem = v3_scale(crease, v3_dot(*rem, crease));
        *vel = v3_scale(crease, v3_dot(*vel, crease));
      }
    }
  }
  planes[(*np)++] = n;
}

// CharacterVirtual::MoveShape: sweep, stop at the contact, slide along the accumulated planes
// walking, walkable surfaces shape only this step's path: the stored velocity is the input's, as the per-update desired velocity
hz_internal void char_slide_planes(const CharEnv *e, v3 *pos, v3 *vel, v3 disp, b32 walk, CharContact *ct, v3 *planes,
                                   CharNear *nearby) {
  u32 np = 0;
  v3 rem = disp;
  u32 near_count = char_near_contacts(e, *pos, nearby);
  for (u32 iter = 0; iter < CHAR_SLIDE_ITERS; iter++) {
    f32 len = v3_length(rem);
    if (len < 1e-5f)
      return;
    v3 dir = v3_scale(rem, 1.0f / len);
    // CharacterVirtual::SolveConstraints: each nearby plane stops the move where its gap reaches the padding
    f32 travel = len;
    v3 n = V3_ZERO;
    EcsEntity ent = 0;
    b32 hit = false;
    for (u32 k = 0; k < near_count; k++) {
      f32 approach = -v3_dot(dir, nearby[k].normal);
      if (approach <= 1e-4f)
        continue;
      f32 t = m_maxf((nearby[k].gap - CHAR_SKIN) / approach, 0.0f);
      if (t < travel) {
        travel = t;
        n = nearby[k].normal;
        ent = nearby[k].entity;
        hit = true;
      }
    }
    ColliderCastResult r = {0};
    EcsEntity cast_ent = 0;
    if (travel > 0.0f && char_cast(e, *pos, v3_add(*pos, v3_scale(dir, travel)), &r, &cast_ent)) {
      f32 approach = m_maxf(-v3_dot(dir, r.normal_on_target), 1e-3f);
      f32 t = m_maxf(r.distance - CHAR_SKIN / approach, 0.0f);
      if (t < travel) {
        travel = t;
        n = r.normal_on_target;
        ent = cast_ent;
        hit = true;
      }
    }
    *pos = v3_add(*pos, v3_scale(dir, travel));
    for (u32 k = 0; k < near_count; k++)
      nearby[k].gap += v3_dot(v3_scale(dir, travel), nearby[k].normal);
    if (!hit)
      return;
    rem = v3_scale(dir, len - travel);
    char_contact_note(e, ct, n, ent);
    v3 path_vel = *vel;
    char_slide_plane(&rem, walk && n.y >= e->cos_slope ? &path_vel : vel, planes, &np, n);
    // CharacterVirtual::DetermineConstraints: a steep slope keeps its plane and adds a horizontal one against climbing
    f32 hl = m_sqrtf(n.x * n.x + n.z * n.z);
    if (n.y > 1e-3f && n.y < e->cos_slope && hl > 1e-4f) {
      v3 h = {n.x / hl, 0.0f, n.z / hl};
      if (v3_dot(rem, h) < 0.0f || v3_dot(*vel, h) < 0.0f)
        char_slide_plane(&rem, vel, planes, &np, h);
    }
  }
}

hz_internal void char_slide(const CharEnv *e, v3 *pos, v3 *vel, v3 disp, b32 walk, CharContact *ct) {
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  v3 *planes = ALLOC_ARRAY(&ta.allocator, v3, CHAR_SLIDE_ITERS * 2);
  CharNear *nearby = ALLOC_ARRAY(&ta.allocator, CharNear, CHAR_NEAR_MAX);
  char_slide_planes(e, pos, vel, disp, walk, ct, planes, nearby);
  tctx_temp_allocator_end(ta);
}

// CharacterVirtual::WalkStairs: up, forward, down; true when it lands on walkable ground at a new height
hz_internal b32 char_step_up(const CharEnv *e, const CharacterTuning *t, v3 start, v3 horiz, v3 *out_pos) {
  ColliderCastResult r = {0};
  EcsEntity ent = 0;
  f32 up = t->step_height;
  if (char_cast(e, start, v3_add(start, (v3){0.0f, up, 0.0f}), &r, &ent))
    up = m_maxf(r.distance - CHAR_SKIN, 0.0f);
  if (up < 0.02f)
    return false;
  v3 p1 = v3_add(start, (v3){0.0f, up, 0.0f});
  v3 p2 = v3_add(p1, horiz);
  if (char_cast(e, p1, p2, &r, &ent))
    p2 = v3_add(p1, v3_scale(v3_normalize(horiz), m_maxf(r.distance - CHAR_SKIN, 0.0f)));
  if (v3_distance(p1, p2) < 0.01f)
    return false;
  CharProbe g = char_probe(e, p2, up + CHAR_SKIN * 2.0f);
  if (!g.hit || !g.walkable)
    return false;
  v3 land = v3_sub(p2, (v3){0.0f, g.dist, 0.0f});
  if (land.y <= start.y + 0.01f || land.y > start.y + t->step_height + CHAR_SKIN)
    return false;
  *out_pos = land;
  return true;
}

// ---- steering: input to velocity, shared by both movers ----

hz_internal f32 char_yaw_of(quat q) {
  v3 f = quat_rotate(q, V3_FORWARD);
  return m_atan2f(f.x, f.z);
}

hz_internal void char_place(Character *c, v3 pos) {
  c->position = pos;
  c->prev_position = pos;
  c->prev_yaw = c->yaw;
  c->velocity = V3_ZERO;
  c->ground = CHARACTER_GROUND_IN_AIR;
  c->ground_entity = ECS_ENTITY_INVALID;
  c->ground_velocity = V3_ZERO;
  c->ground_body_valid = false;
  c->coyote = 0.0f;
  c->jump_buffer = 0.0f;
  c->swimming = false;
  c->footstep_dist = 0.0f;
}

// jump pads and knockback: velocity is added; returns true for an upward launch, which also clears the grace timers
hz_internal b32 char_launch(Character *c, CharacterInput *in, v3 *vel) {
  if (v3_lengthsq(in->add_velocity) <= 0.0f)
    return false;
  b32 up = in->add_velocity.y > 0.0f;
  *vel = v3_add(*vel, in->add_velocity);
  if (up) {
    c->coyote = 0.0f;
    c->jump_buffer = 0.0f;
  }
  in->add_velocity = V3_ZERO;
  c->launches++;
  return up;
}

// the horizontal velocity, jump request and facing; vel is relative to the ground. Returns true when a jump fired
hz_internal b32 char_steer(const CharacterTuning *t, Character *c, CharacterInput *in, f32 dt, b32 supported, b32 swimming,
                           f32 gravity, v3 *vel) {
  if (in->jump) {
    c->jump_buffer = t->jump_buffer;
    in->jump = false;
  } else {
    c->jump_buffer -= dt;
  }
  c->coyote = supported ? t->coyote_time : c->coyote - dt;

  v2 mv = in->move;
  f32 mag = m_sqrtf(mv.x * mv.x + mv.y * mv.y);
  if (mag > 1.0f) {
    mv = (v2){mv.x / mag, mv.y / mag};
    mag = 1.0f;
  }
  f32 sn = m_sinf(in->heading), cs = m_cosf(in->heading);
  v3 wish = {cs * mv.x - sn * mv.y, 0.0f, -sn * mv.x - cs * mv.y};
  f32 wl = v3_length(wish);
  v3 wdir = wl > 1e-4f ? v3_scale(wish, 1.0f / wl) : V3_ZERO;
  b32 moving = mag > 0.01f;
  c->running = in->run && moving;
  f32 speed = c->running ? t->run_speed : t->walk_speed;
  if (swimming)
    speed *= t->swim_speed_scale;
  f32 wish_speed = speed * mag;
  b32 on_floor = supported || swimming;

  v3 hv = {vel->x, 0.0f, vel->z};
  if (t->move_model == CHARACTER_MOVE_DAMPED) {
    f32 k = 1.0f - m_expf(-t->move_damping * (on_floor ? 1.0f : t->air_control) * dt);
    hv = v3_add(hv, v3_scale(v3_sub(v3_scale(wdir, wish_speed), hv), k));
  } else if (t->move_model == CHARACTER_MOVE_QUAKE) {
    if (on_floor) {
      f32 s = v3_length(hv);
      if (s < 1e-4f) {
        hv = V3_ZERO;
      } else {
        f32 drop = m_maxf(s, t->stop_speed) * t->friction * dt;
        hv = v3_scale(hv, m_maxf(s - drop, 0.0f) / s);
      }
    }
    if (moving) {
      f32 add = wish_speed - v3_dot(hv, wdir);
      if (add > 0.0f)
        hv = v3_add(hv, v3_scale(wdir, m_minf((on_floor ? t->ground_accel : t->air_accel) * wish_speed * dt, add)));
    }
  } else {
    f32 secs = on_floor ? (moving ? t->accel_time : t->decel_time) : (moving ? t->air_accel_time : t->air_decel_time);
    f32 max_change = m_maxf(t->walk_speed, t->run_speed) / m_maxf(secs, 1e-3f) * dt;
    v3 dv = v3_sub(v3_scale(wdir, wish_speed), hv);
    f32 dl = v3_length(dv);
    hv = dl <= max_change ? v3_scale(wdir, wish_speed) : v3_add(hv, v3_scale(dv, max_change / dl));
  }
  vel->x = hv.x;
  vel->z = hv.z;

  b32 jumped = false;
  if (c->jump_buffer > 0.0f && c->coyote > 0.0f && !swimming && c->ground != CHARACTER_GROUND_STEEP) {
    vel->y = m_sqrtf(2.0f * gravity * t->jump_height);
    c->jump_buffer = 0.0f;
    c->coyote = 0.0f;
    c->jumps++;
    jumped = true;
  }

  f32 target_yaw = c->yaw;
  b32 turn = false;
  switch (t->facing) {
  case CHARACTER_FACING_HEADING:
    target_yaw = in->heading + M_PI;
    turn = true;
    break;
  case CHARACTER_FACING_INPUT:
    target_yaw = in->face_yaw;
    turn = true;
    break;
  case CHARACTER_FACING_MOVEMENT:
  default:
    if (mag > 0.1f) {
      target_yaw = m_atan2f(wdir.x, wdir.z);
      turn = true;
    }
    break;
  }
  if (turn)
    c->yaw = m_wrap_pi(m_damp_angle(c->yaw, target_yaw, t->turn_rate, dt));
  return jumped;
}

hz_internal void char_footsteps(const CharacterTuning *t, Character *c, b32 supported, f32 dt) {
  if (!supported) {
    return;
  }
  c->footstep_dist += character_planar_speed(c) * dt;
  f32 stride = c->running ? t->footstep_run : t->footstep_walk;
  if (c->footstep_dist > stride) {
    c->footstep_dist = 0.0f;
    c->footsteps++;
    c->footstep_running = c->running;
  }
}

// carries the character with the platform it stood on: translation, rotation about the platform, conveyor velocity
hz_internal v3 char_platform_carry(EcsWorld *w, const CharacterTuning *t, Character *c, f32 dt) {
  v3 carry = V3_ZERO, surface = V3_ZERO;
  if (c->ground == CHARACTER_GROUND_ON_GROUND && c->ground_entity && !t->disable_riding) {
    const ColliderBody *gb = ecs_component_get(w, c->ground_entity, ColliderBody);
    if (gb) {
      b32 moved = !v3_eqv(gb->pos, c->ground_body_pos) || gb->rot.x != c->ground_body_rot.x ||
                  gb->rot.y != c->ground_body_rot.y || gb->rot.z != c->ground_body_rot.z || gb->rot.w != c->ground_body_rot.w;
      if (c->ground_body_valid && moved) {
        quat dq = quat_mul(gb->rot, quat_conjugate(c->ground_body_rot));
        v3 rel = v3_sub(c->position, c->ground_body_pos);
        carry = v3_sub(v3_add(gb->pos, quat_rotate(dq, rel)), c->position);
        v3 f = quat_rotate(dq, V3_FORWARD);
        c->yaw = m_wrap_pi(c->yaw + m_atan2f(f.x, f.z));
      }
      c->ground_body_pos = gb->pos;
      c->ground_body_rot = gb->rot;
      c->ground_body_valid = true;
      const CharacterSurface *sf = ecs_component_get(w, c->ground_entity, CharacterSurface);
      if (sf)
        surface = quat_rotate(gb->rot, sf->local_velocity);
    }
  }
  v3 move = v3_add(carry, v3_scale(surface, dt));
  c->ground_velocity = v3_scale(move, 1.0f / dt);
  return move;
}

hz_internal void char_record_ground_body(EcsWorld *w, Character *c, EcsEntity prev_entity) {
  if (c->ground != CHARACTER_GROUND_ON_GROUND || !c->ground_entity) {
    c->ground_body_valid = false;
    return;
  }
  if (c->ground_entity == prev_entity && c->ground_body_valid)
    return;
  const ColliderBody *gb = ecs_component_get(w, c->ground_entity, ColliderBody);
  c->ground_body_valid = gb != NULL;
  if (gb) {
    c->ground_body_pos = gb->pos;
    c->ground_body_rot = gb->rot;
  }
}

// ---- kinematic mover ----

// an outside write to the root WorldTransform is a teleport, taken by whichever of fixed step or frame pose sees it first
hz_internal void char_kinematic_take_outside_write(Character *c, const WorldTransform *wt) {
  if (v3_eqv(wt->position, c->render_position))
    return;
  char_place(c, wt->position);
  c->render_position = wt->position;
  c->teleports++;
}

// crouch: the capsule drops to crouch_height while the input holds it or the standing capsule has no room; no running
// crouched. Writes the step's height and speeds into the resolved copy `t`.
hz_internal void char_crouch(EcsWorld *w, CharacterTuning *t, Character *c, CharacterInput *in, f32 dt) {
  if (t->crouch_height <= 0.0f || t->crouch_height >= t->height) {
    c->crouched = false;
    c->crouch_k = 0.0f;
    return;
  }
  b32 want = in->crouch;
  if (!want && c->crouched) {
    CharacterTuning low = *t;
    low.height = t->crouch_height;
    CharEnv env = char_env_make(w, &low, c->position.y);
    ColliderCastResult r = {0};
    EcsEntity ent = 0;
    v3 up = {0.0f, t->height - t->crouch_height + CHAR_SKIN, 0.0f};
    if (char_cast(&env, c->position, v3_add(c->position, up), &r, &ent))
      want = true;
  }
  c->crouched = want;
  c->crouch_k = m_saturatef(c->crouch_k + (want ? dt : -dt) / t->crouch_ease_s);
  if (want) {
    t->height = t->crouch_height;
    t->walk_speed = t->crouch_speed;
    t->run_speed = t->crouch_speed;
    in->run = false;
  }
}

hz_internal void char_kinematic_step(EcsWorld *w, const CharacterTuning *t, Character *c, CharacterInput *in,
                                     const WorldTransform *wt, f32 dt) {
  if (!c->placed) {
    c->yaw = char_yaw_of(wt->rotation);
    char_place(c, wt->position);
    c->render_position = wt->position;
    c->placed = true;
  } else {
    char_kinematic_take_outside_write(c, wt);
  }
  if (in->teleport) {
    char_place(c, in->teleport_pos);
    c->yaw = in->teleport_yaw;
    c->prev_yaw = c->yaw;
    c->teleports++;
    in->teleport = false;
  }
  c->prev_position = c->position;
  c->prev_yaw = c->yaw;

  b32 supported = c->ground == CHARACTER_GROUND_ON_GROUND;
  EcsEntity prev_ground = c->ground_entity;
  v3 ground_move = char_platform_carry(w, t, c, dt);
  CharEnv env = char_env_make(w, t, c->position.y);

  v3 vel = c->velocity;
  f32 vy_before = vel.y;
  if (char_launch(c, in, &vel))
    supported = false;
  b32 swimming = t->swim_enabled && c->position.y <= t->water_height - t->swim_depth + 0.1f;
  b32 jumped = char_steer(t, c, in, dt, supported, swimming, t->gravity, &vel);
  if (jumped) {
    vel = v3_add(vel, c->ground_velocity);
    supported = false;
  }
  if (swimming) {
    f32 float_y = t->water_height - t->swim_depth;
    vel.y = m_clampf((float_y - c->position.y) * t->buoyancy, -4.0f, 4.0f);
  } else if (supported && !jumped && vel.y <= 0.0f) {
    vel.y = 0.0f;
  } else {
    vel.y = m_maxf(vel.y - t->gravity * dt, -t->max_fall_speed);
  }
  c->swimming = swimming;

  b32 walk = supported && !jumped && !swimming;
  v3 start = c->position;
  v3 pos = start;
  CharContact ct = {0};
  char_depenetrate(&env, &pos, &vel, walk, &ct);
  v3 disp = v3_scale(vel, dt);
  if (walk) {
    // along the ground the character stands on, the horizontal length kept: slopes neither slow nor speed the walk
    v3 gn = c->ground_normal;
    disp = (v3){vel.x * dt, -(gn.x * vel.x + gn.z * vel.z) * dt / gn.y, vel.z * dt};
    disp = v3_add(disp, ground_move);
  }
  v3 begin = pos, vel_in = vel;
  char_slide(&env, &pos, &vel, disp, walk, &ct);

  if (walk && ct.wall) {
    v3 horiz = {disp.x, 0.0f, disp.z};
    f32 want = v3_length(horiz);
    v3 got = {pos.x - begin.x, 0.0f, pos.z - begin.z};
    v3 stepped;
    // from where the slide stopped (WalkStairs), at least until the lip's contact is walkable: a high step rate never overhangs the edge
    f32 reach = t->radius * (1.0f - m_sqrtf(1.0f - env.cos_slope * env.cos_slope)) + 2.0f * CHAR_SKIN;
    f32 rest = m_maxf(want - v3_length(got), reach);
    // a move that ends against the riser keeps its length but loses its speed: that is blocked too
    f32 kept = vel.x * vel_in.x + vel.z * vel_in.z, had = vel_in.x * vel_in.x + vel_in.z * vel_in.z;
    b32 blocked = v3_length(got) < want * 0.9f || kept < had * 0.81f;
    if (want > 1e-4f && blocked && char_step_up(&env, t, pos, v3_scale(horiz, rest / want), &stepped)) {
      v3 sh = {stepped.x - begin.x, 0.0f, stepped.z - begin.z};
      if (v3_length(sh) > v3_length(got) + 0.01f) {
        // the step went over the riser, so the slide's stop against it does not stand
        pos = stepped;
        vel.x = vel_in.x;
        vel.z = vel_in.z;
      }
    }
  }
  char_depenetrate(&env, &pos, &vel, walk, &ct);

  // the resting contact is the support; walking, StickToFloor sweeps further down for slopes and ledges
  b32 leaving = !walk && vel.y > 0.05f && !(ct.ground && v3_dot(vel, ct.ground_normal) < CHAR_SEPARATION_SPEED); // up a slope is support
  CharProbe pr = leaving ? (CharProbe){0} : char_ground_contact(&env, pos);
  if (pr.hit && pr.walkable && walk)
    pos.y -= m_maxf(pr.dist - CHAR_SKIN, 0.0f) / pr.normal.y;
  if (!(pr.hit && pr.walkable) && walk) {
    CharProbe snap = char_probe(&env, pos, t->snap_distance);
    if (snap.hit && snap.walkable) {
      pos.y -= m_maxf(snap.dist - CHAR_SKIN, 0.0f);
      pr = snap;
    }
  }
  CharacterGround next = CHARACTER_GROUND_IN_AIR;
  EcsEntity ground_entity = ECS_ENTITY_INVALID;
  v3 ground_normal = V3_UP;
  if (pr.hit && pr.walkable) {
    next = CHARACTER_GROUND_ON_GROUND;
    ground_entity = pr.entity;
    ground_normal = pr.normal;
  } else if (pr.hit) {
    next = CHARACTER_GROUND_STEEP;
    ground_normal = pr.normal;
  } else if (ct.touched) {
    next = CHARACTER_GROUND_NOT_SUPPORTED;
  }
  // on the ground the vertical speed is the ground's (carried by ground_move); a slope's slide must not keep a launch
  if (next == CHARACTER_GROUND_ON_GROUND)
    vel.y = 0.0f;
  if (c->ground != CHARACTER_GROUND_ON_GROUND && next == CHARACTER_GROUND_ON_GROUND && vy_before < -CHAR_LAND_MIN_SPEED && !swimming) {
    c->landings++;
    c->land_speed = -vy_before;
  }

  c->ground = next;
  c->ground_entity = ground_entity;
  c->ground_normal = ground_normal;
  c->position = pos;
  c->velocity = vel;
  char_record_ground_body(w, c, prev_ground);
  char_footsteps(t, c, next == CHARACTER_GROUND_ON_GROUND, dt);
}

ecs_system(character_attach_system, EcsIn(CharacterTuning), EcsIn(WorldTransform), EcsNot(Character)) {
  for (i32 i = 0; i < it->count; i++) {
    EcsEntity e = it->entities[i];
    v3 p = ecs_sys_get_component(WorldTransform, i)->position;
    ecs_component_set(it->world, e, Character, {.position = p, .prev_position = p, .render_position = p, .ground = CHARACTER_GROUND_IN_AIR});
    ecs_component_set(it->world, e, CharacterInput, {0});
  }
}

ecs_system(character_kinematic_system, EcsIn(CharacterTuning), EcsInOut(Character), EcsInOut(CharacterInput),
           EcsIn(WorldTransform), EcsResIn(EcsPhysicsWorld), EcsExternIn(ColliderBody), EcsExternIn(CharacterOneWay),
           EcsExternIn(CharacterSurface)) {
  for (i32 i = 0; i < it->count; i++) {
    const CharacterTuning *raw = ecs_sys_get_component(CharacterTuning, i);
    if (raw->mover != CHARACTER_MOVER_KINEMATIC)
      continue;
    CharacterTuning t = character_tuning_resolve(*raw);
    Character *c = ecs_sys_get_component_mut(Character, i);
    CharacterInput *in = ecs_sys_get_component_mut(CharacterInput, i);
    char_crouch(it->world, &t, c, in, it->delta_time);
    char_kinematic_step(it->world, &t, c, in, ecs_sys_get_component(WorldTransform, i), it->delta_time);
  }
}

// ---- rigidbody mover (Character.cpp): the solver owns collision, the system steers the velocity before ecs_rigidbody_step ----

hz_internal void char_wake(RigidBodySleep *slp) {
  slp->state.asleep = false;
  slp->state.timer = 0.0f;
  slp->state.wake_requested = false;
}

hz_internal void char_rigidbody_step(EcsWorld *w, const CharacterTuning *t, Character *c, CharacterInput *in, RigidBody *rb,
                                     RigidBodySleep *slp, ColliderBody *cb, WorldTransform *wt, f32 gravity, f32 dt) {
  if (!c->placed) {
    c->yaw = char_yaw_of(wt->rotation);
    char_place(c, wt->position);
    c->placed = true;
  }
  if (in->teleport) {
    char_place(c, in->teleport_pos);
    c->yaw = in->teleport_yaw;
    c->prev_yaw = c->yaw;
    c->teleports++;
    in->teleport = false;
    wt->position = in->teleport_pos;
    cb->pos = in->teleport_pos;
    rb->velocity.linear = V3_ZERO;
    rb->velocity.angular = V3_ZERO;
  }
  c->position = wt->position;
  c->prev_position = c->position;
  c->prev_yaw = c->yaw;
  char_wake(slp);

  v3 carry = char_platform_carry(w, t, c, dt);
  if (v3_lengthsq(carry) > 0.0f) {
    c->position = v3_add(c->position, carry);
    wt->position = c->position;
    cb->pos = c->position;
  }
  CharEnv env = char_env_make(w, t, c->position.y);
  v3 vel = rb->velocity.linear;
  f32 vy_before = vel.y;
  CharProbe pr = char_ground_contact(&env, c->position);
  // leaving the floor along its normal is a jump; rising along it (the solver pushing up a slope) is support
  if (pr.hit && v3_dot(vel, pr.normal) > CHAR_SEPARATION_SPEED)
    pr = (CharProbe){0};
  if (!(pr.hit && pr.walkable) && vel.y <= 0.5f && c->ground == CHARACTER_GROUND_ON_GROUND) {
    CharProbe snap = char_probe(&env, c->position, t->snap_distance);
    if (snap.hit && snap.walkable)
      pr = snap;
  }
  b32 supported = pr.hit && pr.walkable;
  if (char_launch(c, in, &vel))
    supported = false;
  b32 swimming = t->swim_enabled && c->position.y <= t->water_height - t->swim_depth + 0.1f;
  b32 jumped = char_steer(t, c, in, dt, supported, swimming, gravity, &vel);
  // the integrator subtracts gravity after this system: cancel it wherever the body must hold its height
  f32 hold = gravity * dt;
  if (swimming) {
    vel.y = m_clampf((t->water_height - t->swim_depth - c->position.y) * t->buoyancy, -4.0f, 4.0f) + hold;
  } else if (jumped) {
    vel.y += hold * 0.5f;
  } else if (supported) {
    vel.y = -m_minf(pr.dist, t->snap_distance) / dt * (pr.dist > 0.002f ? 1.0f : 0.0f) + hold;
    v3 horiz = {vel.x, 0.0f, vel.z};
    f32 hl = v3_length(horiz);
    ColliderCastResult r = {0};
    EcsEntity ent = 0;
    v3 look = v3_scale(horiz, hl > 1e-4f ? (hl * dt * 2.0f + t->radius * 0.5f) / hl : 0.0f);
    v3 stepped;
    if (hl > 1e-4f && char_cast(&env, c->position, v3_add(c->position, look), &r, &ent) && r.normal_on_target.y < env.cos_slope &&
        char_step_up(&env, t, c->position, look, &stepped)) {
      wt->position = stepped;
      cb->pos = stepped;
      c->position = stepped;
    }
  }
  rb->velocity.linear = vel;

  CharacterGround next = supported ? CHARACTER_GROUND_ON_GROUND : (pr.hit && !pr.walkable ? CHARACTER_GROUND_STEEP : CHARACTER_GROUND_IN_AIR);
  if (c->ground != CHARACTER_GROUND_ON_GROUND && next == CHARACTER_GROUND_ON_GROUND && vy_before < -CHAR_LAND_MIN_SPEED && !swimming) {
    c->landings++;
    c->land_speed = -vy_before;
  }
  EcsEntity prev_ground = c->ground_entity;
  c->ground = next;
  c->ground_entity = supported ? pr.entity : ECS_ENTITY_INVALID;
  c->ground_normal = supported ? pr.normal : V3_UP;
  c->swimming = swimming;
  c->velocity = vel;
  char_record_ground_body(w, c, prev_ground);
  char_footsteps(t, c, supported, dt);
}

ecs_system(character_rigidbody_system, EcsIn(CharacterTuning), EcsInOut(Character), EcsInOut(CharacterInput),
           EcsInOut(RigidBody), EcsInOut(RigidBodySleep), EcsInOut(ColliderBody), EcsInOut(WorldTransform),
           EcsResIn(EcsPhysicsWorld)) {
  const EcsRigidBodyWorld *rbw = ecs_resource_get(it->world, EcsRigidBodyWorld);
  f32 gravity = v3_length(rbw->settings.gravity);
  for (i32 i = 0; i < it->count; i++) {
    const CharacterTuning *raw = ecs_sys_get_component(CharacterTuning, i);
    if (raw->mover != CHARACTER_MOVER_RIGIDBODY)
      continue;
    CharacterTuning t = character_tuning_resolve(*raw);
    Character *c = ecs_sys_get_component_mut(Character, i);
    CharacterInput *in = ecs_sys_get_component_mut(CharacterInput, i);
    ColliderBody *cb = ecs_sys_get_component_mut(ColliderBody, i);
    b32 was = c->crouched;
    char_crouch(it->world, &t, c, in, it->delta_time);
    if (was != c->crouched)
      cb->collider = collider_capsule((v3){0.0f, t.radius, 0.0f}, (v3){0.0f, t.height - t.radius, 0.0f}, t.radius);
    char_rigidbody_step(it->world, &t, c, in, ecs_sys_get_component_mut(RigidBody, i), ecs_sys_get_component_mut(RigidBodySleep, i),
                        cb, ecs_sys_get_component_mut(WorldTransform, i), gravity, it->delta_time);
  }
}

void character_schedule_fixed(EcsWorld *w) {
  ecs_schedule(w, character_kinematic_system);
  if (ecs_resource_get(w, EcsRigidBodyWorld))
    ecs_schedule(w, character_rigidbody_system);
}

// ---- frame: interpolated pose and the avatar animation ----

ecs_system(character_pose_system, EcsIn(CharacterTuning), EcsInOut(Character), EcsInOut(LocalTransform),
           EcsInOut(WorldTransform), EcsOptional(CharacterVisual), EcsResIn(CharacterFrame)) {
  f32 alpha = ecs_sys_get_resource(CharacterFrame)->alpha;
  for (i32 i = 0; i < it->count; i++) {
    Character *c = ecs_sys_get_component_mut(Character, i);
    if (!c->placed)
      continue;
    b32 kinematic = ecs_sys_get_component(CharacterTuning, i)->mover == CHARACTER_MOVER_KINEMATIC;
    WorldTransform *wt = ecs_sys_get_component_mut(WorldTransform, i);
    if (kinematic)
      char_kinematic_take_outside_write(c, wt);
    v3 curr = kinematic ? c->position : wt->position;
    if (!kinematic)
      c->position = curr;
    v3 p = v3_lerp(c->prev_position, curr, alpha);
    f32 yaw = m_wrap_pi(c->prev_yaw + m_wrap_pi(c->yaw - c->prev_yaw) * alpha);
    quat rot = quat_rotate_y(yaw);
    if (kinematic) {
      LocalTransform *lt = ecs_sys_get_component_mut(LocalTransform, i);
      lt->position = p;
      lt->rotation = rot;
      wt->position = p;
      wt->rotation = rot;
      c->render_position = p;
    } else {
      const CharacterVisual *vis = ecs_sys_get_component_optional(CharacterVisual, i);
      if (vis && vis->entity) {
        ecs_component_set(it->world, vis->entity, LocalTransform, {.position = p, .rotation = rot, .scale = V3_ONE});
        ecs_component_set(it->world, vis->entity, WorldTransform, {.position = p, .rotation = rot, .scale = V3_ONE});
      }
    }
  }
}

hz_internal b32 char_airborne(CharacterGround g) { return g != CHARACTER_GROUND_ON_GROUND; }

ecs_system(character_avatar_system, EcsIn(Character), EcsInOut(CharacterAvatar), EcsExternInOut(EcsAnimationLayer),
           EcsResIn(AssetSystem)) {
  const AssetSystem *assets = ecs_sys_get_resource(AssetSystem);
  for (i32 i = 0; i < it->count; i++) {
    const Character *c = ecs_sys_get_component(Character, i);
    CharacterAvatar *av = ecs_sys_get_component_mut(CharacterAvatar, i);
    EcsAnimationLayer *al = ecs_component_get_mut(it->world, av->avatar, EcsAnimationLayer);
    if (!al || !asset_is_ready(assets, av->idle) || !asset_is_ready(assets, av->run) || !asset_is_ready(assets, av->fall))
      continue;

    // the fall pose is held a few frames past landing before the crossfade
    b32 air = char_airborne(c->ground);
    if (air)
      av->landed_hold = 0;
    else if (av->was_airborne)
      av->landed_hold = CHAR_LANDED_HOLD_FRAMES;
    av->was_airborne = air;
    if (av->landed_hold > 0) {
      air = true;
      av->landed_hold--;
    }

    f32 speed = character_planar_speed(c);
    f32 blend = m_saturatef((speed - av->run_lo) / m_maxf(av->run_hi - av->run_lo, 1e-3f));
    if (blend > 0.6f)
      av->run_dominant = true;
    else if (blend < 0.4f)
      av->run_dominant = false;

    HzHandleT(AnimationBlobAsset) target = air ? av->fall : (av->run_dominant ? av->run : av->idle);
    AnimationBlobAsset *blob = asset_get(assets, target, AnimationBlobAsset);
    if (animation_layer_current_animation(&al->layer) != blob && !animation_layer_has_animation(&al->layer, blob)) {
      animation_layer_play_ex(&al->layer, blob, ANIM_TRANSITION_CROSSFADE, 0.20f, 1.0f, true, 0.0f);
      ecs_component_set(it->world, av->avatar, EcsAnimationRef, {.animation = blob_asset_ref_from_handle(AnimationBlobAsset, target)});
    }
  }
}

void character_schedule_frame(EcsWorld *w) {
  ecs_schedule(w, character_attach_system); // once a frame: its deferred sets apply at the scope's end, after every fixed step
  ecs_schedule(w, character_pose_system);
  ecs_schedule_single(w, character_avatar_system); // writes other entities' animation layers
}

// ---- setup ----

void character_controller_init(EcsWorld *w) {
  assert_msg(is_main_thread(), "character_controller_init: main lane only"); // ECS resources are set outside a scope
  ecs_resource_set(w, CharacterFrame, ((CharacterFrame){.alpha = 1.0f}));
}

EcsEntity character_spawn(EcsWorld *w, CString name, v3 feet, const CharacterTuning *tuning) {
  assert_msg(is_main_thread(), "character_spawn: main lane only"); // creates entities outside a scope
  CharacterTuning t = character_tuning_resolve(*tuning);
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, EntityName, {.name = fixedstr64_from_cstr((char *)name)});
  ecs_component_set(w, e, LocalTransform, {.position = feet, .rotation = quat_identity(), .scale = V3_ONE});
  ecs_component_set(w, e, WorldTransform, {.position = feet, .rotation = quat_identity(), .scale = V3_ONE});
  ecs_component_set(w, e, CharacterTuning, *tuning);
  if (t.mover == CHARACTER_MOVER_RIGIDBODY) {
    ecs_component_set(w, e, ColliderBody,
                      {.collider = collider_capsule((v3){0.0f, t.radius, 0.0f}, (v3){0.0f, t.height - t.radius, 0.0f}, t.radius),
                       .pos = feet,
                       .rot = quat_identity(),
                       .scale = 1.0f});
    ecs_rigidbody_body_add(w, e, (RigidBody){.inverse_mass = 1.0f, .lock_rotation = true, .undamped = true});
  }
  return e;
}

void character_avatar_add(EcsWorld *w, EcsEntity character, const CharacterAvatarDesc *desc) {
  assert_msg(is_main_thread(), "character_avatar_add: main lane only"); // loads assets and creates entities outside a scope
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  b32 defaults = !desc->model_path;
  CString model = defaults ? CHAR_DEFAULT_MODEL : desc->model_path;
  quat rot = desc->local_rotation;
  if (quat_dot(rot, rot) < 1e-6f)
    rot = quat_identity(); // models face +Z, the character's forward
  v3 scale = v3_lengthsq(desc->local_scale) > 0.0f ? desc->local_scale : V3_ONE;

  CharacterAvatar av = {.run_lo = desc->run_speed_lo > 0.0f ? desc->run_speed_lo : 0.5f,
                        .run_hi = desc->run_speed_hi > 0.0f ? desc->run_speed_hi : 4.0f};
  HzHandleT(ModelBlobAsset) model_h = asset_load(ModelBlobAsset, assets, .path = model);
  av.idle = asset_load(AnimationBlobAsset, assets, .path = defaults ? CHAR_DEFAULT_IDLE : desc->idle_clip_path);
  av.run = asset_load(AnimationBlobAsset, assets, .path = defaults ? CHAR_DEFAULT_RUN : desc->run_clip_path);
  av.fall = asset_load(AnimationBlobAsset, assets, .path = defaults ? CHAR_DEFAULT_FALL : desc->fall_clip_path);

  // a rigidbody character's body never rotates: the avatar hangs under a pivot that character_pose_system turns
  EcsEntity parent = character;
  const CharacterTuning *tuning = ecs_component_get(w, character, CharacterTuning);
  const CharacterVisual *visual = ecs_component_get(w, character, CharacterVisual);
  if (visual && visual->entity) {
    parent = visual->entity;
  } else if (tuning && tuning->mover == CHARACTER_MOVER_RIGIDBODY) {
    const WorldTransform *wt = ecs_component_get(w, character, WorldTransform);
    v3 at = wt ? wt->position : V3_ZERO;
    parent = ecs_entity_create(w);
    ecs_component_set(w, parent, LocalTransform, {.position = at, .rotation = quat_identity(), .scale = V3_ONE});
    ecs_component_set(w, parent, WorldTransform, {.position = at, .rotation = quat_identity(), .scale = V3_ONE});
    ecs_component_set(w, character, CharacterVisual, {.entity = parent});
  }

  av.avatar = ecs_entity_create(w);
  ecs_component_set(w, av.avatar, EcsParent, {.parent = parent});
  ecs_component_set(w, av.avatar, LocalTransform, {.position = desc->local_offset, .rotation = rot, .scale = scale});
  ecs_component_set(w, av.avatar, EcsModelRef, {.model = blob_asset_ref_from_handle(ModelBlobAsset, model_h)});
  ecs_component_set(w, av.avatar, EcsAnimationRef, {.animation = blob_asset_ref_from_handle(AnimationBlobAsset, av.idle)});
  ecs_component_set(w, av.avatar, EcsVisibility, {.visible = true, .casts_shadow = true});
  ecs_component_set(w, character, CharacterAvatar, av);
}
