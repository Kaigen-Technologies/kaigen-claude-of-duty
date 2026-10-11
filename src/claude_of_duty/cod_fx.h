// Claude of Duty effects: atlases, muzzle flashes, impacts per surface, decals, tracers, explosions, shells, ambience.
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"

// what the effects publish for others (the look reads the flash lights' exposure scale)
typedef struct CodFxState {
  b32 ready;          // atlases and effect assets are registered
  u32 live_particles; // particles spawned in the last second (an upper bound of what is alive)
  u32 live_decals;
  f64 bake_ms;        // wall time of the atlas bake and the effect registration
  f32 unit;           // display units of one scene unit: scene_per_unit x pre-exposure, for HDR colours the effects hand to the engine
} CodFxState;
ECS_COMPONENT_TYPEDEF(CodFxState);

#define COD_FX_VIEW_FLASHES 2

// one light of the viewmodel's flash pool: lights the gun only
typedef struct CodFxViewLight {
  v3 position;     // world
  v3 color;        // linear
  f32 intensity;   // candela in scene units, this frame
  f32 distance;    // cutoff, metres
  f32 peak;
  f32 age;
  f32 duration;
  f32 decay;       // 1/s
  u32 priority;
} CodFxViewLight;

// the viewmodel flash pool the look hands to the viewmodel shading
typedef struct CodFxViewFlash {
  CodFxViewLight lights[COD_FX_VIEW_FLASHES];
} CodFxViewFlash;
ECS_COMPONENT_TYPEDEF(CodFxViewFlash);

// main lane, app_init: resources; the atlas bakes and the effects register on the first frames
void cod_fx_init(EcsWorld *w);
// inside the scope after every producer (weapons, AI, player, explosions): consolidations into vfx and decals
void cod_fx_schedule(EcsWorld *w);
// main lane after the scope: GPU work the effects record (atlas bakes), the requests played, the particle systems updated
void cod_fx_frame(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_fx_lanes(EcsWorld *w);
