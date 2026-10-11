// Claude of Duty weapons: procedural guns and hands, the viewmodel, fire, ballistics with penetration, shells and mags.
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"

typedef enum CodWeaponId {
  COD_WEAPON_RIFLE = 0, // M4A1
  COD_WEAPON_SMG = 1,
  COD_WEAPON_PISTOL = 2,
  COD_WEAPON_COUNT = 3,
} CodWeaponId;

typedef enum CodFireMode {
  COD_FIRE_AUTO = 0,
  COD_FIRE_BURST = 1,
  COD_FIRE_SEMI = 2,
} CodFireMode;

typedef enum CodReloadPhase {
  COD_RELOAD_START = 0,
  COD_RELOAD_MAG_OUT,
  COD_RELOAD_MAG_IN,
  COD_RELOAD_END,
} CodReloadPhase;

// who fired a round: the player, or a soldier entity
typedef struct CodShooter {
  b32 player;
  EcsEntity soldier;
} CodShooter;

// one surface a round entered or left
typedef struct CodImpact {
  v3 point;
  v3 normal;
  v3 incident;     // the round's direction
  u32 surface;     // CodSurface
  f32 damage;      // damage the round carried at this layer
  b32 exit;        // the exit side of a penetrated layer
  EcsEntity entity; // the collider entity hit (a world piece, a soldier hitbox, a rigid body)
} CodImpact;

// one round's flight (bullet:tracer)
typedef struct CodTracer {
  v3 from;
  v3 to;
  f32 speed; // m/s
} CodTracer;

#define COD_BALLISTICS_MAX_LAYERS 6

// a round traced through the world: impacts in order, the tracer, and whether a soldier or the player took it
typedef struct CodRound {
  CodShooter shooter;
  u32 weapon;   // CodWeaponId
  v3 origin;
  v3 dir;
  f32 dropoff;  // damage kept at max_dist, 0..1
  f32 player_miss; // an enemy round that passed the player within 1.6 m without a hit: the closest approach, metres (-1 none)
  CodImpact impacts[COD_BALLISTICS_MAX_LAYERS * 2];
  u32 impact_count;
  CodTracer tracer;
  b32 has_tracer; // the player's tracer-only round (impact_count 0) is pushed at spawn; the impact round that follows has none
  EcsEntity hit_soldier; // the soldier whose hitbox the round reached (0 none)
  u32 hit_zone;          // CodHitZone of that hitbox
  f32 hit_damage;        // damage delivered to it
  v3 hit_point;
  b32 hit_player;        // an enemy round reached the player's capsule
} CodRound;

// one weapon:fire
typedef struct CodShotEvent {
  CodShooter shooter;
  u32 weapon;  // CodWeaponId
  v3 origin;   // the muzzle, world space
  v3 dir;
  u32 seed;
  b32 viewmodel; // drawn by the first-person rig (the flash is on the viewmodel muzzle)
  v3 view_muzzle; // the viewmodel muzzle in world space, for the flash and the light
} CodShotEvent;

// one weapon:shell (a spent case, or a dropped magazine)
typedef struct CodEjectEvent {
  v3 position;
  v3 velocity;
  quat rotation;
  v3 spin;
  u32 weapon;
  b32 magazine;
  f32 case_len;    // metres of the spent case (0 for a magazine)
  f32 case_radius; // rim radius, metres
} CodEjectEvent;

#define COD_WEAPON_MAX_SHOTS 8
#define COD_WEAPON_MAX_ROUNDS 16
#define COD_WEAPON_MAX_EJECTS 8

// the player's weapon events this frame; soldiers publish theirs on CodSoldierEvents
typedef struct CodWeaponEvents {
  CodShotEvent shots[COD_WEAPON_MAX_SHOTS];
  u32 shot_count;
  CodRound rounds[COD_WEAPON_MAX_ROUNDS];
  u32 round_count;
  CodEjectEvent ejects[COD_WEAPON_MAX_EJECTS];
  u32 eject_count;
  b32 reload;            // weapon:reload this frame
  u32 reload_phase;      // CodReloadPhase
  b32 dry_fire;
  b32 swapped;           // a draw started
  b32 fire_mode_changed;
  b32 ads_in;            // the ADS edges (audio foley)
  b32 ads_out;
} CodWeaponEvents;

// the player's weapon for every other package
typedef struct CodWeaponState {
  u32 weapon;        // CodWeaponId in hand
  u32 fire_mode;     // CodFireMode
  u32 mag;           // rounds in the magazine, the chambered one not counted (the HUD shows min(mag + chambered, mag_size))
  b32 chambered;
  u32 reserve;
  u32 mag_size;
  f32 spread_deg;    // live cone half-angle: the crosshair gap
  f32 ads;           // 0..1
  b32 reloading;
  b32 firing;
  b32 switching;
  b32 inspecting;
  u32 grenades;      // frag count (HUD)
  u32 flashes;       // flashbang count (HUD)
  v3 muzzle;         // world space muzzle of the viewmodel this frame
  quat muzzle_rot;
  b32 visible;       // the viewmodel is drawn
  f32 reload_progress; // 0..1 through the reload clip, 0 otherwise
  CodWeaponEvents events;
} CodWeaponState;
ECS_COMPONENT_TYPEDEF(CodWeaponState);

// a thrown grenade, a rigid body with a fuse; the player's and the soldiers' share it (weapons owns the type)
HZ_REFLECT()
ECS_COMPONENT(CodGrenade) {
  b32 from_player;
  EcsEntity thrower; // the soldier who threw it (0 = the player)
  f32 fuse;          // seconds left
  b32 flash;         // a flashbang, not a frag
  b32 detonated;     // the fuse ran out this frame; the consolidation reads it, then the entity goes
};

// one explosion
typedef struct CodExplosionEvent {
  v3 position;
  f32 radius;
  f32 damage;
  f32 impulse;
  b32 flash;
  b32 from_player;
} CodExplosionEvent;

#define COD_MAX_EXPLOSIONS 8
// this frame's explosions from detonated grenades; each target's own system applies the damage
typedef struct CodExplosions {
  CodExplosionEvent items[COD_MAX_EXPLOSIONS];
  u32 count;
} CodExplosions;
ECS_COMPONENT_TYPEDEF(CodExplosions);

// traces one round (shooter, weapon, origin, dir set) through the world, hitboxes and, for an enemy, the player; any lane, read-only
void cod_ballistics_fire(EcsWorld *w, CodRound *round, f32 damage, f32 penetration, f32 max_dist);
// pushes a traced round's impulses into the rigid bodies it struck; one caller at a time (an ecs_schedule_single system)
void cod_ballistics_apply_impulses(EcsWorld *w, const CodRound *round);
// a thrown grenade: a rigid sphere with the fuse running; the main lane or an ecs_schedule_single system
EcsEntity cod_grenade_spawn(EcsWorld *w, v3 position, v3 velocity, EcsEntity thrower, b32 from_player, b32 flash);

// main lane, app_init: resources, the viewmodel entities
void cod_weapons_init(EcsWorld *w);
// inside the scope after the player: fire, ballistics, the viewmodel pose; publishes CodWeaponState and CodPlayerAim
void cod_weapons_schedule(EcsWorld *w);
// inside the fixed loop: the weapons' fixed-step work (spring integration that must be frame-rate independent)
void cod_weapons_schedule_fixed(EcsWorld *w);
// inside the scope after cod_player_schedule_intake: poses the viewmodel entities on the final camera
void cod_weapons_schedule_view(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_weapons_lanes(EcsWorld *w);
// main lane after the scope: GPU uploads and renderer work
void cod_weapons_frame(EcsWorld *w);
