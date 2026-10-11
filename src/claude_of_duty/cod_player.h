// Claude of Duty player: movement states on the engine character, the first-person camera feel, health.
#pragma once
#include "ecs/ecs.h"
#include "gameplay/camera_rig/camera_rig.h"
#include "gameplay/character/character.h"
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"

#define COD_PLAYER_MAX_HEALTH 100.0f
// hits one frame can queue; more fold into the last slot
#define COD_PLAYER_MAX_HITS 8
// directional damage indicators alive at once
#define COD_PLAYER_MAX_INDICATORS 4

// the movement state machine; stance is separate
typedef enum CodMoveState {
  COD_MOVE_STAND = 0,
  COD_MOVE_CROUCH,
  COD_MOVE_PRONE,
  COD_MOVE_SPRINT,
  COD_MOVE_TAC_SPRINT,
  COD_MOVE_SLIDE,
  COD_MOVE_JUMP,
  COD_MOVE_FALL,
  COD_MOVE_MANTLE,
  COD_MOVE_VAULT,
  COD_MOVE_COUNT,
} CodMoveState;

typedef enum CodStance {
  COD_STANCE_STAND = 0,
  COD_STANCE_CROUCH = 1,
  COD_STANCE_PRONE = 2,
} CodStance;

// what a mantle event climbed
typedef enum CodLedgeKind {
  COD_LEDGE_NONE = 0,
  COD_LEDGE_VAULT = 1,  // over the obstacle, land on the far side
  COD_LEDGE_MANTLE = 2, // up onto the top face
} CodLedgeKind;

// one damage hit on the player
typedef struct CodPlayerHit {
  f32 amount;
  v3 from;            // world position the hit came from
  b32 directionless;  // fall damage: no indicator, no push away from `from`
} CodPlayerHit;

// the player's inbox: writers declare EcsInOut(CodPlayerIntake) and run before cod_player_schedule_intake
HZ_REFLECT()
ECS_COMPONENT(CodPlayerIntake) {
  FixedArray(CodPlayerHit, COD_PLAYER_MAX_HITS) hits;
  f32 heal;
  f32 trauma;       // 0..1 shake added (explosions, hits)
  f32 suppression;  // near misses: the bullet:impact within 3.2 m and not ahead (0..1 added)
  f32 recoil_pitch; // camera recoil impulse, radians
  f32 recoil_yaw;
  f32 recoil_roll;
  f32 recoil_punch;
  f32 kick_pitch;   // the independent weapon kick channel
  f32 kick_yaw;
  f32 kick_roll;
  f32 debug_damage; // automation (set:): one hit of this amount from ahead of the eye
  f32 debug_suppression; // automation (set:): suppression added once
  f32 debug_trauma;      // automation (set:): trauma added once
};

force_inline void cod_player_intake_damage(CodPlayerIntake *in, f32 amount, v3 from) {
  if (in->hits.len < COD_PLAYER_MAX_HITS)
    in->hits.items[in->hits.len++] = (CodPlayerHit){amount, from, false};
  else
    in->hits.items[COD_PLAYER_MAX_HITS - 1].amount += amount;
}

// the player's events this frame
typedef struct CodPlayerEvents {
  b32 reset;           // placed this frame: match start, pose, respawn
  b32 jumped;
  v3 jump_position;    // feet
  b32 landed;
  f32 land_speed;      // fall speed at the landing, m/s
  u32 land_surface;    // CodSurface under the feet
  v3 land_position;    // feet
  b32 footstep;
  b32 footstep_left;
  b32 footstep_running;
  u32 footstep_surface; // CodSurface
  f32 footstep_speed;
  u32 footstep_stance;  // CodStance
  v3 footstep_position; // the foot, on the surface
  b32 mantled;         // a mantle or vault started
  b32 vaulted;
  u32 mantle_kind;     // CodLedgeKind
  f32 mantle_height;
  b32 slide_started;
  b32 stance_changed;
  b32 state_changed;   // the discrete state (player:state): move state, stance, grounded, mantling
  u32 hit_count;       // damage:taken, one per applied hit
  CodPlayerHit hits[COD_PLAYER_MAX_HITS];
  b32 died;
  b32 respawned;
  b32 heartbeat;       // a heartbeat pulse at low health
  f32 heartbeat_strength;
} CodPlayerEvents;

// one directional damage indicator; angle is radians in view space, 0 = straight ahead, + to the right
typedef struct CodDamageIndicator {
  b32 active;
  f32 angle;
  f32 amount;
  f32 life; // seconds since it was hit
  v3 from;
} CodDamageIndicator;

// the player for every other package: written after the controller and again after the intake
typedef struct CodPlayerState {
  EcsEntity entity;   // the player entity: its CodPlayerIntake is the inbox
  b32 ready;          // placed in the match
  b32 controls;       // took input this frame
  v3 feet;            // bottom of the capsule, interpolated
  v3 eye;             // the composed camera position
  v3 velocity;
  v3 forward;         // the view forward (camera -Z)
  quat view_rot;      // the camera rotation, feel included
  f32 yaw;            // the movement basis, radians
  f32 pitch;
  f32 fov;            // world vertical degrees this frame
  f32 height;         // capsule height of the current stance
  f32 eye_height;     // the smoothed eye above the feet
  u32 state;          // CodMoveState
  u32 stance;         // CodStance
  b32 grounded;
  b32 sprinting;
  b32 tactical_sprint;
  b32 sliding;
  b32 mantling;
  f32 lean;           // -1 left .. 1 right
  f32 slide_progress; // 0..1
  f32 speed;          // m/s, horizontal
  f32 move_amount;    // 0..1 speed fraction for bob and sway
  f32 bob_phase;
  b32 ads_requested;  // the aim button held
  f32 ads;            // 0..1 the blend in use
  f32 health;
  f32 health_fraction;
  b32 low_health;
  b32 critical_health;
  b32 regenerating;
  b32 dead;
  f32 suppression;
  f32 trauma;
  f32 low_effect;     // 0..1 weight of the low-health treatment
  f32 heartbeat_pulse; // 0..1 the lub-dub envelope
  f32 hit_flash;      // 0..1
  CodDamageIndicator indicators[COD_PLAYER_MAX_INDICATORS];
  v3 view_kick;       // pitch, yaw, roll the recoil and kick channels add to the view, radians
  f32 view_punch;     // metres the camera is pushed along the view
  CodPlayerEvents events;
} CodPlayerState;
ECS_COMPONENT_TYPEDEF(CodPlayerState);

// what the weapons push into the player each frame (p.setAdsProgress, the gun's ADS fov and move speed scale)
typedef struct CodPlayerAim {
  f32 ads;             // 0..1 the weapon's ADS curve
  f32 ads_fov_scale;   // the gun's ADS fov multiplier
  f32 move_speed_scale; // the gun's mobility multiplier
  b32 driven;          // true while weapons writes `ads` every frame; false: the player blends the aim button itself
} CodPlayerAim;
ECS_COMPONENT_TYPEDEF(CodPlayerAim);

// main lane, app_init: resources, the player entity, the first-person view on the camera
void cod_player_init(EcsWorld *w);
// inside the scope after the flow: placement, the look and the frame's intent
void cod_player_schedule_input(EcsWorld *w);
// inside the fixed loop, once per step: the movement state machine, the character's fixed step, the post-move bookkeeping
void cod_player_schedule_fixed(EcsWorld *w);
// inside the scope after the fixed loop: events into the camera and health, the camera feel; publishes CodPlayerState and the camera
void cod_player_schedule(EcsWorld *w);
// inside the scope after every intake writer: damage, heals, trauma, recoil, explosions; republishes CodPlayerState
void cod_player_schedule_intake(EcsWorld *w);
