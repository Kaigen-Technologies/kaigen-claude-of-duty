// Claude of Duty AI: procedural skinned soldiers, navigation, perception, cover, squads, combat, ragdoll deaths.
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"
#include "cod_weapons.h"

typedef enum CodHitZone {
  COD_ZONE_HEAD = 0,
  COD_ZONE_NECK,
  COD_ZONE_TORSO,
  COD_ZONE_PELVIS,
  COD_ZONE_ARM,
  COD_ZONE_LEG,
  COD_ZONE_COUNT,
} CodHitZone;

typedef enum CodTeam {
  COD_TEAM_FRIENDLY = 0,
  COD_TEAM_ENEMY = 1,
} CodTeam;

// one soldier hit volume: a ColliderBody on the hitbox layer, posed from the skeleton each frame
HZ_REFLECT()
ECS_COMPONENT(CodHitbox) {
  EcsEntity soldier;
  u32 zone; // CodHitZone
};

// a soldier's events this frame, for fx, audio and the HUD
typedef struct CodSoldierEvents {
  u32 frame;            // CodClock.frame these were written on; stale otherwise
  b32 fired;
  CodShotEvent shot;
  CodRound round;       // the round's impacts and tracer
  b32 ejected;
  CodEjectEvent eject;
  b32 hit;              // took damage this frame
  f32 damage;
  b32 headshot;
  b32 armor;            // the hit was soaked by armour (the HUD's armour hitmarker)
  v3 hit_point;
  b32 killed;           // actor:death
  b32 killed_by_player;
  v3 death_impulse;
  v3 death_point;
  b32 footstep;
  u32 footstep_surface; // CodSurface
  b32 voice;            // a callout line started
  u32 voice_line;
  b32 threw_grenade;
  b32 reloaded;         // weapon:reload phase start
} CodSoldierEvents;

typedef enum CodAiMode {
  COD_AI_IDLE = 0,
  COD_AI_PATROL,
  COD_AI_ALERT,
  COD_AI_COMBAT,
  COD_AI_SUPPRESSED,
  COD_AI_FLANK,
  COD_AI_RETREAT,
  COD_AI_DEAD,
  COD_AI_MODE_COUNT,
} CodAiMode;

// the soldier variants, in spawn cycle order
typedef enum CodSoldierVariant {
  COD_VARIANT_VANGUARD = 0,
  COD_VARIANT_IRREGULAR,
  COD_VARIANT_BREACHER,
  COD_VARIANT_COUNT,
} CodSoldierVariant;

// the per-soldier public state: everything another package may read about a soldier
HZ_REFLECT()
ECS_COMPONENT(CodSoldier) {
  u32 team;     // CodTeam
  u32 variant;  // soldier archetype
  String32Bytes name; // the killfeed name
  f32 health;
  b32 alive;
  b32 ragdoll;  // dead and simulated as a ragdoll
  v3 position;  // feet
  f32 yaw;
  v3 head;      // world space, for markers and audio
  u32 mode;     // CodAiMode
  u32 id;       // the agent id (1, 2, ...), also the voice seed
};

ECS_COMPONENT_TYPEDEF(CodSoldierEventsComp);
typedef struct CodSoldierEventsComp {
  CodSoldierEvents ev;
} CodSoldierEventsComp;

// the match score and the AI totals, consolidated each frame
typedef struct CodAiState {
  u32 alive;
  u32 agents;
  u32 kills_by_player;  // the friendly score
  u32 player_deaths;    // the enemy score
  b32 player_was_dead;  // last frame's player death state (counts the edge)
  b32 ready;            // navigation and the garrison are built
  u32 walkable;         // ai.stats: walkable nav cells
  u32 cover_points;
  u32 lod_irrelevant;   // soldiers nothing can see this frame
  f64 nav_ms;           // grid + cover build wall time
  f32 brain_us;         // this frame's summed per-soldier update cost, microseconds
  u32 next_id;          // the next agent id
  b32 has_nearest;      // a live soldier exists; nearest_chest is the one closest to the player (dev aim_soldier)
  v3 nearest_chest;
  f32 nearest_d2;
} CodAiState;
ECS_COMPONENT_TYPEDEF(CodAiState);

// the hitbox collision layer the ballistics query (registered by the AI package)
typedef struct CodHitboxes {
  u64 layer; // HzHandleT(CollisionLayerHandle) as raw bits; 0 until registered
} CodHitboxes;
ECS_COMPONENT_TYPEDEF(CodHitboxes);

// main lane, app_init: resources, the hitbox layer, the soldier asset build
void cod_ai_init(EcsWorld *w);
// inside the scope after the weapons: spawns, perception, brains, movement, animation, damage, ragdolls; consolidates
void cod_ai_schedule(EcsWorld *w);
// inside the fixed loop: soldier character steps
void cod_ai_schedule_fixed(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_ai_lanes(EcsWorld *w);
// main lane after the scope: GPU uploads and renderer work
void cod_ai_frame(EcsWorld *w);
