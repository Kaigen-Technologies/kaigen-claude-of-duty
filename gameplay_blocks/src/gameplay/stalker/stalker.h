// Stalker: a character that sees (range, cone, line of sight) and hears (stalker_noise_emit) StalkerTarget entities,
// decides IDLE / INVESTIGATE / CHASE / SEARCH, paths over the engine navmesh and steers through CharacterInput.
// Setup: ecs_navigation_init, then stalker_init(w, agent) in app_init; stalker_add on a character entity.
// Scope, before character_schedule_fixed: stalker_schedule (perceive, decide), the game's own states, then
// stalker_schedule_move (path, steer). A game owns a stalker by setting Stalker.held: decide skips it and the game
// writes state, goal, speed and halt; perception, paths and steering keep running.
#pragma once

#include "ecs/ecs.h"
#include "ecs/ecs_navigation/ecs_navigation.h"
#include "gameplay/character/character.h"
#include "lib/reflect.h"

#define STALKER_MAX_TARGETS 16
#define STALKER_MAX_NOISES 64

typedef enum StalkerState { STALKER_IDLE, STALKER_INVESTIGATE, STALKER_CHASE, STALKER_SEARCH } StalkerState;

HZ_REFLECT()
ECS_COMPONENT(StalkerSenses) {
  f32 sight_range;    // metres
  f32 sight_cone_deg; // full cone; a chase sees all around within range
  f32 hear_scale;     // a noise of radius r is heard within r * hear_scale
  f32 lose_after_s;   // an unseen chased target is still tracked this long, then the stalker searches
  f32 search_s;       // seconds a search lasts before idling
  f32 walk_speed;     // m/s outside a chase
  f32 chase_speed;
  f32 repath_s;       // minimum seconds between path requests
  u32 sight_layer;    // physics category mask of what blocks sight on the static and kinematic layers; 0 = all
};

typedef struct Stalker {
  StalkerState state;
  EcsEntity target; // the chased (or last seen) target
  v3 goal;          // where it paths; with halt, what it faces
  f32 alert;        // 0..1: 1 on sight, 0.5 on a noise, decaying
  f32 timer;        // seconds in the state's current phase
  NavRequestId path;
  b32 held;         // the game drives state, goal, speed and halt
  b32 halt;         // stand still facing the goal
  f32 speed;        // m/s this frame
  b32 sees;         // a target in sight this frame
  v3 target_pos;    // the chased target's feet (tracked while chasing)
  v3 last_seen;
  b32 heard;
  v3 noise;         // the loudest noise heard this frame
  b32 arrived;      // within reach of the goal
  v3 steer;         // the point the move heads for
  NavRequestId pending;
  v3 path_goal;     // the goal `path` (or `pending`) was asked for
  i32 waypoint;
  f32 repath_t;
  b32 unreachable;  // the last request found no path: it heads straight for the goal
} Stalker;
ECS_COMPONENT_TYPEDEF(Stalker);

ECS_TAG(StalkerTarget);

typedef struct StalkerTargetInfo {
  EcsEntity entity;
  v3 feet;
  f32 eye_height;
} StalkerTargetInfo;

typedef struct StalkerNoise {
  v3 pos;
  f32 radius;
} StalkerNoise;

// the block's per-frame state: the gathered targets and the noise double buffer
typedef struct StalkerWorld {
  NavAgentKey agent;
  u32 target_count;
  StalkerTargetInfo targets[STALKER_MAX_TARGETS];
  u32 write;          // the noise buffer emits go to; perception reads the other
  u32 noise_count[2]; // atomic
  StalkerNoise noises[2][STALKER_MAX_NOISES];
} StalkerWorld;
ECS_COMPONENT_TYPEDEF(StalkerWorld);

// main lane, once, after ecs_navigation_init: the resource, and the agent's navmesh class built from now
void stalker_init(EcsWorld *w, NavAgentKey agent);
// main lane: StalkerSenses and a halted idle Stalker on a character entity; the move sets its tuning to face the input
void stalker_add(EcsWorld *w, EcsEntity e, const StalkerSenses *senses);
// inside the scope: gather targets, perceive (parallel), decide (parallel)
void stalker_schedule(EcsWorld *w);
// inside the scope, after the game's own states and before character_schedule_fixed: path, then steer (parallel)
void stalker_schedule_move(EcsWorld *w);
// any lane: a noise heard next frame by stalkers within radius * hear_scale
void stalker_noise_emit(EcsWorld *w, v3 pos, f32 radius);
