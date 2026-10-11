// Claude of Duty world: the procedural market street, its static colliders, practicals and spawns.
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"

// level -> world: the street is authored down -Z, this yaw and offset place it in the world
#define COD_LEVEL_YAW 0.5877f
#define COD_LEVEL_TX 0.9f
#define COD_LEVEL_TZ 1.34f

#define COD_WORLD_MAX_SPAWNS 16
#define COD_WORLD_MAX_BUILDINGS 32
#define COD_WORLD_MAX_ALLEYS 8

typedef struct CodSpawnPoint {
  v3 position; // world space, feet
  f32 yaw;     // radians, 0 faces -Z
} CodSpawnPoint;

// a building's footprint and floors for the AI's garrison and cover queries, world space
typedef struct CodBuildingInfo {
  v3 center;
  v2 half_size;  // level-space footprint half extents
  f32 yaw;       // footprint rotation in world space
  u32 floors;
  f32 floor_height;
  b32 enterable;
  v4 level_rect;   // level space spec: (x, z, w, d)
  b32 collapsed;   // spec.collapse or spec.ruin
  f32 room_top;    // world-space y room lights clip at: roof (or the setback floor) minus 6 cm
} CodBuildingInfo;

// what the world publishes once built; read-only for every other package
typedef struct CodWorld {
  b32 ready;            // geometry, colliders and lights are spawned
  f32 progress;         // 0..1 build progress
  aabb bounds;          // the playable area, world space
  CodSpawnPoint spawns[COD_WORLD_MAX_SPAWNS];
  u32 spawn_count;
  CodBuildingInfo buildings[COD_WORLD_MAX_BUILDINGS];
  u32 building_count;
  v4 street;            // level space: x kerb half width, y z min, z z max
  v4 alleys[COD_WORLD_MAX_ALLEYS]; // level space x0, z0, x1, z1 of the open alleys
  u32 alley_count;
  u32 static_tris;      // stats, logged at build
  u32 instance_tris;
  u32 instances;
  f64 build_ms;         // wall time of the build
  struct CodWorldBuild *build; // the world's private build state
  v3 cull_eye;          // the camera the prop distance cull measures from, written by the world's frame hook
  HzHandleT(RendererMaterial) lamp_lens; // the street-lamp diffuser, its emission follows the solar altitude
  f32 lamp_mix;         // the lamps' last applied night mix
} CodWorld;
ECS_COMPONENT_TYPEDEF(CodWorld);

// a prop instance hidden beyond max_dist metres of the camera
HZ_REFLECT()
ECS_COMPONENT(CodPropCull) {
  f32 max_dist;
  f32 radius; // the prototype's bounding radius, so a big prop stays until it is wholly out
};

// a practical the world owns: 0 = interior bulb, 1 = street lamp; its intensity follows the solar altitude
HZ_REFLECT()
ECS_COMPONENT(CodWorldLight) {
  u32 kind;
};

// level space to world space
force_inline v3 cod_level_to_world(v3 p) {
  f32 c = m_cosf(COD_LEVEL_YAW), s = m_sinf(COD_LEVEL_YAW);
  return (v3){c * p.x + s * p.z + COD_LEVEL_TX, p.y, -s * p.x + c * p.z + COD_LEVEL_TZ};
}

// main lane, app_init: resources; the build starts on the first frame
void cod_world_init(EcsWorld *w);
// inside the scope, early: advances the build (generation in parallel, uploads, collider and light spawns), publishes CodWorld
void cod_world_schedule(EcsWorld *w);
// the analytic floor height under a world point; physics is exact
f32 cod_world_ground_height(const CodWorld *world, f32 x, f32 z);
// the level's world-space bounds, known before the world is built (CodWorld.bounds once it is)
aabb cod_world_level_bounds(void);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_world_lanes(EcsWorld *w);
// main lane after the scope: GPU uploads and renderer work
void cod_world_frame(EcsWorld *w);
