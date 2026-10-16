#include "stalker.h"
#include "lib/assert.h"
#include "lib/thread_context.h"

#define STALKER_ARRIVE 0.4f       // metres from the goal that count as there
#define STALKER_CORNER 0.35f      // metres from a waypoint that move on to the next
#define STALKER_REPATH_DIST 0.5f  // the goal moved this far from the path's: ask again
#define STALKER_ALERT_DECAY 0.25f // per second

hz_internal f32 stalker_flat_dist(v3 a, v3 b) { return m_sqrtf((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z)); }

void stalker_init(EcsWorld *w, NavAgentKey agent) {
  assert_msg(is_main_thread(), "stalker_init: main lane only"); // sets a resource outside a scope
  ecs_resource_set_zero(w, StalkerWorld);
  ecs_resource_get_mut(w, StalkerWorld)->agent = agent;
  ecs_navigation_register_agent(w, agent);
}

void stalker_add(EcsWorld *w, EcsEntity e, const StalkerSenses *senses) {
  assert_msg(is_main_thread(), "stalker_add: main lane only"); // adds components outside a scope
  ecs_component_set(w, e, StalkerSenses, *senses);
  ecs_component_set(w, e, Stalker, {.state = STALKER_IDLE, .halt = true, .speed = senses->walk_speed});
}

void stalker_noise_emit(EcsWorld *w, v3 pos, f32 radius) {
  StalkerWorld *sw = ecs_resource_get_mut(w, StalkerWorld);
  u32 b = (u32)atomic_u32_read_relaxed(&sw->write);
  u32 i = (u32)atomic_u32_inc_eval(&sw->noise_count[b]) - 1u;
  if (i < STALKER_MAX_NOISES)
    sw->noises[b][i] = (StalkerNoise){.pos = pos, .radius = radius};
}

// the frame's start: the noises emitted since the last one become readable, the targets are gathered anew
ecs_system(stalker_begin_system, EcsResInOut(StalkerWorld)) {
  StalkerWorld *sw = ecs_sys_get_resource_mut(StalkerWorld);
  u32 next = sw->write ^ 1u;
  atomic_u32_write_relaxed(&sw->noise_count[next], 0u);
  atomic_u32_write_relaxed(&sw->write, next);
  sw->target_count = 0;
}

ecs_system(stalker_gather_system, EcsIn(WorldTransform), EcsWith(StalkerTarget), EcsOptional(CharacterTuning),
           EcsResInOut(StalkerWorld)) {
  StalkerWorld *sw = ecs_sys_get_resource_mut(StalkerWorld);
  for (i32 i = 0; i < it->count && sw->target_count < STALKER_MAX_TARGETS; i++) {
    const CharacterTuning *ct = ecs_sys_get_component_optional(CharacterTuning, i);
    sw->targets[sw->target_count++] = (StalkerTargetInfo){
        .entity = it->entities[i],
        .feet = ecs_sys_get_component(WorldTransform, i)->position,
        .eye_height = ct ? character_tuning_resolve(*ct).eye_height : 1.0f,
    };
  }
}

// sight: in range, in the cone (a chase looks all around), and no static or kinematic body on the line eye to eye
hz_internal b32 stalker_can_see(EcsWorld *w, const StalkerSenses *ss, const Stalker *s, v3 eye, f32 yaw, v3 target_eye) {
  v3 d = v3_sub(target_eye, eye);
  f32 flat = m_sqrtf(d.x * d.x + d.z * d.z);
  if (v3_length(d) > ss->sight_range)
    return false;
  if (s->state != STALKER_CHASE && flat > 1e-3f) {
    f32 cosang = (m_sinf(yaw) * d.x + m_cosf(yaw) * d.z) / flat;
    if (cosang < m_cosf(m_radians(ss->sight_cone_deg * 0.5f)))
      return false;
  }
  const EcsCollisionLayer *st = ecs_physics_get_static_layer(w);
  const EcsCollisionLayer *kn = ecs_physics_get_kinematic_layer(w);
  CollisionLayer *layers[2];
  i32 n = 0;
  if (st)
    layers[n++] = (CollisionLayer *)&st->layer;
  if (kn)
    layers[n++] = (CollisionLayer *)&kn->layer;
  PhysicsRaycastResult r = {0};
  i32 body = -1, li = -1;
  return n == 0 ||
         !physics_raycast_layers(eye, target_eye, layers, n, (PhysicsQueryFilter){.mask = ss->sight_layer}, &r, &body, &li);
}

ecs_system(stalker_perceive_system, EcsIn(StalkerSenses), EcsInOut(Stalker), EcsIn(Character), EcsIn(CharacterTuning),
           EcsResIn(StalkerWorld), EcsResIn(EcsPhysicsWorld)) {
  const StalkerWorld *sw = ecs_sys_get_resource(StalkerWorld);
  u32 rb = sw->write ^ 1u;
  u32 noise_n = (u32)atomic_u32_read_relaxed(&sw->noise_count[rb]);
  noise_n = noise_n < STALKER_MAX_NOISES ? noise_n : STALKER_MAX_NOISES;
  f32 dt = it->delta_time;
  for (i32 i = 0; i < it->count; i++) {
    const StalkerSenses *ss = ecs_sys_get_component(StalkerSenses, i);
    Stalker *s = ecs_sys_get_component_mut(Stalker, i);
    const Character *c = ecs_sys_get_component(Character, i);
    f32 eye_h = character_tuning_resolve(*ecs_sys_get_component(CharacterTuning, i)).eye_height;
    v3 eye = v3_add(c->position, (v3){0.0f, eye_h, 0.0f});
    // the chased target wins while in sight, else the nearest target in sight
    b32 tracked = false, sees_current = false;
    EcsEntity seen = ECS_ENTITY_INVALID;
    v3 seen_feet = V3_ZERO;
    f32 best = 1e30f;
    for (u32 t = 0; t < sw->target_count; t++) {
      const StalkerTargetInfo *ti = &sw->targets[t];
      b32 current = ti->entity == s->target;
      if (current) {
        tracked = true;
        s->target_pos = ti->feet;
      }
      f32 d = stalker_flat_dist(ti->feet, c->position);
      if (sees_current || (!current && d >= best))
        continue;
      if (stalker_can_see(it->world, ss, s, eye, c->yaw, v3_add(ti->feet, (v3){0.0f, ti->eye_height, 0.0f}))) {
        best = d;
        seen = ti->entity;
        seen_feet = ti->feet;
        sees_current = current;
      }
    }
    s->sees = seen != ECS_ENTITY_INVALID;
    if (s->sees) {
      s->target = seen;
      s->target_pos = seen_feet;
      s->last_seen = seen_feet;
    } else if (!tracked) {
      s->target = ECS_ENTITY_INVALID;
    }
    s->heard = false;
    f32 loud = 0.0f;
    for (u32 k = 0; k < noise_n; k++) {
      const StalkerNoise *nz = &sw->noises[rb][k];
      f32 reach = nz->radius * ss->hear_scale;
      f32 d = v3_distance(nz->pos, c->position);
      if (d <= reach && reach - d >= loud) {
        loud = reach - d;
        s->heard = true;
        s->noise = nz->pos;
      }
    }
    s->alert = s->sees ? 1.0f : m_maxf(s->heard ? 0.5f : 0.0f, s->alert - STALKER_ALERT_DECAY * dt);
  }
}

ecs_system(stalker_decide_system, EcsIn(StalkerSenses), EcsInOut(Stalker), EcsIn(Character)) {
  f32 dt = it->delta_time;
  for (i32 i = 0; i < it->count; i++) {
    Stalker *s = ecs_sys_get_component_mut(Stalker, i);
    if (s->held)
      continue;
    const StalkerSenses *ss = ecs_sys_get_component(StalkerSenses, i);
    v3 pos = ecs_sys_get_component(Character, i)->position;
    StalkerState was = s->state;
    if (s->sees) {
      s->state = STALKER_CHASE;
    } else if (s->state == STALKER_CHASE) {
      if (s->timer + dt >= ss->lose_after_s || !s->target) {
        s->state = STALKER_SEARCH;
        s->goal = s->last_seen;
      }
    } else if (s->heard) {
      s->state = STALKER_INVESTIGATE;
      s->goal = s->noise;
    }
    if (s->state != was)
      s->timer = 0.0f;
    switch (s->state) {
    case STALKER_CHASE:
      // an unseen target is tracked until lose_after_s runs out
      s->timer = s->sees ? 0.0f : s->timer + dt;
      s->goal = s->target ? s->target_pos : s->last_seen;
      s->halt = false;
      break;
    case STALKER_INVESTIGATE:
      s->halt = false;
      if (stalker_flat_dist(pos, s->goal) < STALKER_ARRIVE) {
        s->state = STALKER_SEARCH;
        s->timer = 0.0f;
      }
      break;
    case STALKER_SEARCH:
      // walks to where the target was, then stands until the search runs out
      s->halt = stalker_flat_dist(pos, s->goal) < STALKER_ARRIVE;
      s->timer += dt;
      if (s->timer >= ss->search_s) {
        s->state = STALKER_IDLE;
        s->timer = 0.0f;
      }
      break;
    default: s->halt = true; break;
    }
    s->speed = s->state == STALKER_CHASE ? ss->chase_speed : ss->walk_speed;
  }
}

// path: a navmesh request when the goal left the path's and repath_s has passed; the old path is followed until the
// answer replaces it. The steer point is the next corner, then the goal itself.
ecs_system(stalker_path_system, EcsIn(StalkerSenses), EcsInOut(Stalker), EcsIn(Character), EcsResIn(StalkerWorld),
           EcsResIn(EcsNavigationWorld)) {
  const StalkerWorld *sw = ecs_sys_get_resource(StalkerWorld);
  f32 dt = it->delta_time;
  for (i32 i = 0; i < it->count; i++) {
    const StalkerSenses *ss = ecs_sys_get_component(StalkerSenses, i);
    Stalker *s = ecs_sys_get_component_mut(Stalker, i);
    v3 pos = ecs_sys_get_component(Character, i)->position;
    s->repath_t -= dt;
    NavPathResult st = NAV_PATH_IN_PROGRESS;
    const NavWaypoint *wps = NULL;
    i32 n = 0;
    if (s->pending && ecs_navigation_result(it->world, s->pending, &st, &wps, &n)) {
      if (s->path)
        ecs_navigation_release(it->world, s->path);
      s->path = 0;
      s->unreachable = st != NAV_PATH_SUCCESS;
      if (st == NAV_PATH_SUCCESS) {
        s->path = s->pending;
        s->waypoint = n > 1 ? 1 : 0;
      } else {
        ecs_navigation_release(it->world, s->pending);
      }
      s->pending = 0;
    }
    s->arrived = stalker_flat_dist(pos, s->goal) < STALKER_ARRIVE;
    b32 off_path = stalker_flat_dist(s->path_goal, s->goal) > STALKER_REPATH_DIST;
    b32 stale = off_path || (!s->path && !s->unreachable);
    if (!s->halt && !s->arrived && stale && !s->pending && s->repath_t <= 0.0f) {
      NavPathRequest r = {.start = pos,
                          .finish = s->goal,
                          .search_half_extents = {2.0f, 2.0f, 2.0f},
                          .waypoint_spacing = HZ_INF,
                          .max_los_distance = HZ_INF};
      s->pending = ecs_navigation_request(it->world, sw->agent, &r);
      s->path_goal = s->goal;
      s->repath_t = ss->repath_s;
    }
    s->steer = s->goal;
    if (s->path && ecs_navigation_result(it->world, s->path, &st, &wps, &n) && n > 0) {
      while (s->waypoint < n - 1 && stalker_flat_dist(pos, wps[s->waypoint].pos) < STALKER_CORNER)
        s->waypoint++;
      // past the last corner (or the goal moved on while the new path is pending): straight at the goal
      if (s->waypoint < n - 1 || stalker_flat_dist(pos, wps[n - 1].pos) >= STALKER_CORNER)
        s->steer = wps[s->waypoint].pos;
    }
  }
}

// steer: the character walks at the stalker's speed toward the steer point and faces it (halted: faces the goal)
ecs_system(stalker_move_system, EcsIn(Stalker), EcsIn(Character), EcsInOut(CharacterInput), EcsInOut(CharacterTuning)) {
  for (i32 i = 0; i < it->count; i++) {
    const Stalker *s = ecs_sys_get_component(Stalker, i);
    const Character *c = ecs_sys_get_component(Character, i);
    CharacterInput *in = ecs_sys_get_component_mut(CharacterInput, i);
    const CharacterTuning *ct = ecs_sys_get_component(CharacterTuning, i);
    if (ct->facing != CHARACTER_FACING_INPUT || ct->walk_speed != s->speed || ct->run_speed != s->speed) {
      CharacterTuning *t = ecs_sys_get_component_mut(CharacterTuning, i);
      t->facing = CHARACTER_FACING_INPUT;
      t->walk_speed = s->speed;
      t->run_speed = s->speed;
    }
    b32 go = !s->halt && !s->arrived;
    v3 to = v3_sub(go ? s->steer : s->goal, c->position);
    b32 has_dir = to.x * to.x + to.z * to.z > 1e-6f;
    in->run = false;
    in->move = go && has_dir ? (v2){0.0f, 1.0f} : (v2){0};
    if (has_dir) {
      in->heading = m_atan2f(-to.x, -to.z);
      in->face_yaw = m_atan2f(to.x, to.z);
    }
  }
}

void stalker_schedule(EcsWorld *w) {
  ecs_schedule_single(w, stalker_begin_system);
  ecs_schedule_single(w, stalker_gather_system);
  ecs_schedule(w, stalker_perceive_system);
  ecs_schedule(w, stalker_decide_system);
}

void stalker_schedule_move(EcsWorld *w) {
  ecs_schedule(w, stalker_path_system);
  ecs_schedule(w, stalker_move_system);
}
