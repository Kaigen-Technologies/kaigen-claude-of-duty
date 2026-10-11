// Claude of Duty AI, package-private: the types the navigation, perception, brain, squad and spawn files share.
#pragma once
#include "ecs/ecs.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "ecs/ecs_navigation/ecs_navigation.h"
#include "rendering/gizmos_utils/gizmos_utils.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "cod_ai.h"
#include "cod_ai_body.h"
#include "cod_rng.h"
#include "cod_world.h"

// ---- cover analysis: NavGrid and CoverMap, built from the static physics layer (routes come from the engine navmesh) ----

#define CAI_CELL 0.8f
#define CAI_NAV_RADIUS 0.36f
#define CAI_NAV_HEIGHT 1.78f
#define CAI_NAV_CROUCH_HEIGHT 1.15f
#define CAI_NAV_MAX_STEP 0.45f
#define CAI_NAV_SLOPE_COS 0.69465837f // cos 46 degrees
#define CAI_COVER_REACH 1.3f
#define CAI_SOLDIER_RADIUS 0.34f   // capsule, at scale 1
#define CAI_SOLDIER_HEIGHT 1.78f
#define CAI_SOLDIER_SCALE_MAX 1.025f // the largest of cai_variant_scale: the navmesh class fits every variant
#define CAI_MAX_PATH 32
#define CAI_COVER_MEMORY 8 // cover points the navmesh could not route to, skipped by later picks
#define CAI_MAX_LANES 64

typedef struct CaiCover {
  v3 pos;
  f32 dx, dz; // the direction the cover faces, toward the blocker
  b32 high;   // the blocker stops a standing shot
  f32 dist;
} CaiCover;

// flags: 0 blocked, 1 walkable standing, 2 walkable crouched only
typedef struct CaiNav {
  b32 ready;
  f32 min_x, min_z, top_y;
  u32 nx, nz;
  u8 *flags;
  f32 *floor;
  u8 *enclosure;
  u32 walkable;
  CaiCover *cover;
  u32 cover_count;
  u32 *claim; // per cover point: the id of the agent holding it, 0 when unheld; rebuilt each frame by the roster system
  CaiCover *cover_cell; // build scratch: one candidate per cell
  u8 *cover_has;
  u32 lane_walk[CAI_MAX_LANES];
  u32 lane_cover[CAI_MAX_LANES];
  u32 lanes;
  f64 grid_ms, cover_ms;
} CaiNav;

// the nav build state machine: the scope's gate system requests it, the lanes hook runs it
typedef enum CaiNavStage {
  CAI_NAV_WAIT = 0,
  CAI_NAV_BUILD,
  CAI_NAV_READY,
} CaiNavStage;

typedef struct CodAiNavGate {
  u32 stage; // CaiNavStage
  u32 settled; // consecutive frames the world has been ready
  ArenaAllocator *arena;
  CaiNav *nav;
  u64 t0;
} CodAiNavGate;
ECS_COMPONENT_TYPEDEF(CodAiNavGate);

// the world queries: the static layer filtered to world and sight blockers (glass blocks neither)
typedef struct CaiQ {
  const EcsCollisionLayer *layer;
  PhysicsQueryFilter filter;
} CaiQ;

typedef struct CaiRay {
  v3 point;
  v3 normal;
  f32 distance;
} CaiRay;

// ---- the brain ----

#define CAI_MAX_SOLDIERS 64
#define CAI_MAX_SQUADS 8
#define CAI_MAX_HITS 96
#define CAI_MAX_NOISE 192
#define CAI_MAX_PATROL 4
#define CAI_HITBOX_COUNT 7

typedef struct CodSoldierBrain {
  u32 id;
  u32 slot;
  u32 squad; // squad index + 1, 0 none
  u32 mode;  // CodAiMode
  f32 state_time;
  CodRng rng;
  u32 variant;
  f32 scale, height, radius, eye_height;
  b32 alive;
  f32 health;
  v3 position;
  f32 yaw, target_yaw;
  v3 velocity;
  b32 grounded;
  b32 blocked; // the character was held back by something
  b32 character_ready;

  f32 awareness, alertness, suppression;
  b32 has_target, target_visible;
  v3 last_known;
  f32 last_known_age;

  f32 fire_rate, fire_cooldown, burst_cooldown;
  i32 burst_left;
  u32 ammo;
  f32 spread, weapon_damage, weapon_range;
  v3 aim_target;
  f32 aim_weight;
  b32 want_fire;
  f32 peek_side;
  b32 peeking;
  f32 peek_timer;
  f32 grenade_cooldown;
  b32 has_grenade;

  v3 path[CAI_MAX_PATH];
  u32 path_len, path_index;
  f32 repath_timer;
  NavRequestId nav_req; // the path request in flight, 0 when none
  b32 nav_cover;        // that request goes to the held cover point
  v3 move_target;
  b32 has_move_target;
  f32 desired_speed, speed;
  b32 crouch;
  i32 cover;
  v3 cover_pos;
  i32 cover_unreachable[CAI_COVER_MEMORY]; // a ring; -1 empty
  u32 cover_unreachable_next;
  v3 patrol[CAI_MAX_PATROL];
  u32 patrol_count, patrol_index;
  f32 stuck_timer, vault_cooldown;

  f32 vault_t;
  f32 vault_left;
  b32 vaulting;
  v3 vault_from, vault_to;
  f32 reload_left;
  b32 lod_irrelevant;

  b32 peek_ask; // asked to lean out at cover last frame; the squad answers before the next update
  b32 flank_ask;
  v3 flank_dest;
  b32 grenade_ask;

  v3 cmd_vel;       // m/s the character is steered at
  f32 cmd_height;   // capsule height wanted
  b32 cmd_teleport; // vault root motion places the capsule
  v3 cmd_teleport_pos;
  b32 grenade_req; // a lobbed grenade for the consolidation to spawn
  v3 grenade_from, grenade_to;

  f32 cost_us;
  u32 logged_mode;
} CodSoldierBrain;
ECS_COMPONENT_TYPEDEF(CodSoldierBrain);

// ---- squads, consolidated in the shared resource ----

typedef struct CaiSquad {
  b32 used;
  u32 id;
  CodRng rng;
  u32 member_count;
  u32 peek_tokens;
  u32 holders[CAI_MAX_SOLDIERS];
  u32 holder_count;
  f32 peek_timer;
  u32 peek_cursor; // the roster slot the next grant scan starts from
  f32 grenade_cooldown;
  u32 flanker; // agent id, 0 none
  v3 contact;
  b32 has_contact;
  f32 contact_age;
  u32 combat_count; // members in combat or suppressed
  u32 grenade_grant; // agent id granted this frame
} CaiSquad;

typedef struct CaiRoster {
  EcsEntity entity;
  b32 used;
  b32 alive;
  u32 id;
  u32 squad;
  u32 mode;
  v3 position;
  f32 radius;
  b32 has_target, target_visible;
  v3 last_known;
  b32 peek_ask, flank_ask, grenade_ask;
  i32 cover;      // the cover point held, -1 none
  b32 cover_lost; // another soldier took it first this frame: pick again
  NavRequestId nav_req; // the brain's unanswered path request, released when the soldier is torn down
} CaiRoster;

// one hitbox hit on a soldier this frame
typedef struct CaiHit {
  EcsEntity soldier;
  u32 zone;
  f32 damage; // the round's damage at the layer times the zone scale
  v3 point;
  v3 dir;
  b32 from_player;
} CaiHit;

typedef struct CaiShot {
  v3 origin, dir;
} CaiShot;

// everything the per-soldier systems read, written only by the consolidation systems before them
typedef struct CodAiShared {
  NavAgentKey nav_key; // the soldiers' navmesh class (the largest variant)
  b32 navmesh_ready;   // that class's navmesh is built
  f32 dt;
  f32 time;
  u32 frame;
  b32 live;
  v3 player_chest;
  b32 player_ready;
  b32 footstep;
  v3 footstep_pos;
  b32 footstep_running;
  v4 planes[6];
  b32 frustum_ok;
  v3 sun;
  b32 trace;
  i32 debug;
  CaiNav *nav;
  u32 roster_count;
  CaiRoster roster[CAI_MAX_SOLDIERS];
  CaiSquad squads[CAI_MAX_SQUADS];
  u32 hit_count;
  CaiHit hits[CAI_MAX_HITS];
  u32 shot_count;
  CaiShot shots[COD_WEAPON_MAX_SHOTS];
  u32 noise_count;
  v3 noise[CAI_MAX_NOISE];
  u32 explosion_count;
  CodExplosionEvent explosions[COD_MAX_EXPLOSIONS];
  u32 populated_serial; // the match start the garrison was last built for
  b32 applied_no_ai;
  u32 next_slot;
  CodRng rng; // ai.rng
  u32 variant_seen; // a bit per variant: the ai rng forks the first time a variant is built
} CodAiShared;
ECS_COMPONENT_TYPEDEF(CodAiShared);

// a hit capsule: posed from two bones of its soldier each frame
typedef struct CodHitboxLink {
  u32 bone_a, bone_b;
  f32 radius;
} CodHitboxLink;
ECS_COMPONENT_TYPEDEF(CodHitboxLink);

// ---- helpers shared by the files ----

hz_internal f32 cai_hypotf(f32 a, f32 b) { return m_sqrtf(a * a + b * b); }

// a vector rotated about Y by yaw (yaw 0 faces +Z)
hz_internal v3 cai_rot_y(v3 v, f32 yaw) {
  f32 c = m_cosf(yaw), s = m_sinf(yaw);
  return (v3){c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

// a bind-pose vector in the soldier's frame: scale, yaw, then the feet position
hz_internal v3 cai_bind_world(v3 feet, f32 yaw, f32 scale, v3 bind) { return v3_add(feet, cai_rot_y(v3_scale(bind, scale), yaw)); }

// a bone's world position from last frame's pose, or the bind pose before the body has posed once
hz_internal v3 cai_bone(const CodSoldierPose *pose, v3 feet, f32 yaw, f32 scale, u32 bone) {
  return pose->ready ? pose->bones[bone] : cai_bind_world(feet, yaw, scale, cod_rig_bind[bone]);
}
