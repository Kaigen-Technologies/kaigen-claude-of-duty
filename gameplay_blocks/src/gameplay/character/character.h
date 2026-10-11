// Character: a capsule that walks, jumps, swims, rides platforms and steps over ledges.

// Kinematic mover follows CharacterVirtual: collide-and-slide over contact planes, steep slopes as walls, WalkStairs, StickToFloor.

// Rigidbody mover follows Character: a rotation-locked dynamic body steered through its velocity.

// Ground states follow CharacterBase::EGroundState; the slope limit follows mCosMaxSlopeAngle.

// Needs WorldTransform + LocalTransform on the entity; ecs_physics_init + ecs_physics_schedule in the frame (static and kinematic layers).

// Frame: character_schedule_fixed inside each fixed step, character_schedule_frame once before ecs_transform_schedule.

// Kinematic: Character.position is authoritative and the root WorldTransform is the interpolated render pose.

// Writing the root WorldTransform from outside (the --commands teleport) teleports a kinematic character.

// Rigidbody: the body owns the pose; CharacterVisual.entity receives the interpolated pose and facing.

// Layers are a per-frame snapshot, so a moving platform's pose lags the character by one frame.
#pragma once

#include "animation/animation.h"
#include "assets/assets.h"
#include "ecs/ecs.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "ecs/renderer/ecs_renderer.h"
#include "ecs/transform/ecs_transform.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"

HZ_REFLECT()
typedef enum {
  CHARACTER_MOVER_KINEMATIC = 0, // swept capsule, no rigidbody
  CHARACTER_MOVER_RIGIDBODY,     // dynamic rotation-locked body driven before ecs_rigidbody_step
} CharacterMover;

HZ_REFLECT()
typedef enum {
  CHARACTER_MOVE_TIME_TO_SPEED = 0, // linear approach to the target velocity over accel_time / decel_time
  CHARACTER_MOVE_QUAKE,             // Quake accelerate + friction
  CHARACTER_MOVE_DAMPED,            // exponential approach to the target velocity at move_damping, air_control off the ground
} CharacterMoveModel;

HZ_REFLECT()
typedef enum {
  CHARACTER_FACING_MOVEMENT = 0, // turn toward the movement direction
  CHARACTER_FACING_HEADING,      // face away from the camera (heading)
  CHARACTER_FACING_INPUT,        // CharacterInput.face_yaw
} CharacterFacing;

typedef enum {
  CHARACTER_GROUND_ON_GROUND = 0, // standing on a walkable surface
  CHARACTER_GROUND_STEEP,         // touching a surface steeper than the slope limit: slides, cannot jump
  CHARACTER_GROUND_NOT_SUPPORTED, // in contact with something but nothing under the feet
  CHARACTER_GROUND_IN_AIR,
} CharacterGround;

// zero fields take the defaults noted; the booleans default off
HZ_REFLECT()
ECS_COMPONENT(CharacterTuning) {
  CharacterMover mover;
  CharacterMoveModel move_model;
  CharacterFacing facing;
  f32 radius;      // capsule radius; 0 -> 0.35
  f32 height;      // feet to head; 0 -> 1.8
  f32 eye_height;  // feet to eye, read by camera rigs; 0 -> 1.65
  f32 walk_speed;  // m/s; 0 -> 6
  f32 run_speed;   // m/s with CharacterInput.run; 0 -> walk_speed
  f32 accel_time;     // TIME_TO_SPEED: seconds to reach full speed on the ground; 0 -> 0.1
  f32 decel_time;     // seconds to stop on the ground; 0 -> 0.1
  f32 air_accel_time; // 0 -> 0.4
  f32 air_decel_time; // 0 -> 0.4
  f32 ground_accel;   // QUAKE: 0 -> 11
  f32 air_accel;      // 0 -> 2.2
  f32 friction;       // 0 -> 7.5
  f32 stop_speed;     // friction floor speed; 0 -> 1.5
  f32 move_damping;   // DAMPED: approach rate, 1/s; 0 -> 14
  f32 air_control;    // DAMPED: rate scale off the ground; 0 -> 0.35
  f32 gravity;        // m/s^2, positive; 0 -> 36
  f32 max_fall_speed; // 0 -> 55
  f32 jump_height;    // apex above the takeoff point; 0 -> 1.8
  f32 coyote_time;    // jump grace after leaving a ledge; 0 -> 0.1
  f32 jump_buffer;    // jump grace before landing; 0 -> 0.15
  f32 slope_limit_deg; // steepest walkable slope; 0 -> 50
  f32 step_height;     // tallest step walked over; 0 -> 0.35
  f32 snap_distance;   // stick-to-floor reach walking down slopes and ledges; 0 -> 0.35
  f32 turn_rate;       // facing turn lambda, 1/s; 0 -> 12
  f32 footstep_walk;   // metres per footstep walking; 0 -> 2.1
  f32 footstep_run;    // metres per footstep running; 0 -> 2.6
  b32 swim_enabled;
  f32 water_height;      // world y of the water surface
  f32 swim_depth;        // swim once the feet sink this far below the surface, floating at that depth; 0 -> 1.25
  f32 swim_speed_scale;  // 0 -> 0.6
  f32 buoyancy;          // 1/s rate toward the float height; 0 -> 6
  b32 disable_riding;    // true: stand still in world space on moving platforms
};

// the per-frame intent, written by the game
typedef struct CharacterInput {
  v2 move;         // x right, y forward in the heading frame; |move| <= 1, scales the speed
  f32 heading;     // view yaw, radians: forward = (-sin, 0, -cos), right = (cos, 0, -sin)
  f32 face_yaw;    // CHARACTER_FACING_INPUT target, in the same convention as Character.yaw
  b32 run;
  b32 jump;        // request, consumed on arming the jump buffer
  b32 teleport;    // consumed: place at teleport_pos at rest
  v3 teleport_pos; // feet
  f32 teleport_yaw; // facing at the teleport
  v3 add_velocity; // consumed: jump pad, knockback. Upward launches clear coyote and the jump buffer
} CharacterInput;
ECS_COMPONENT_TYPEDEF(CharacterInput);

// marker on a platform entity: solid from above, pass-through from below and the sides
HZ_REFLECT()
ECS_COMPONENT(CharacterOneWay) {
  u8 _;
};

// conveyor: a character standing on this entity is carried at this velocity
HZ_REFLECT()
ECS_COMPONENT(CharacterSurface) {
  v3 local_velocity; // in the entity's frame
};

// runtime state; counters only grow, a consumer diffs against the value it saw last
typedef struct Character {
  v3 position;       // feet, authoritative (kinematic) / mirrored from the body (rigidbody)
  v3 velocity;       // own velocity in world space, platform motion excluded
  f32 yaw;           // facing; yaw 0 faces +Z, rotation is quat_rotate_y(yaw)
  v3 prev_position;  // pose at the start of the last step, for interpolation
  f32 prev_yaw;
  v3 render_position; // pose last written to WorldTransform; a mismatch means someone teleported it
  CharacterGround ground;
  v3 ground_normal;
  EcsEntity ground_entity;
  v3 ground_velocity; // platform motion this step, m/s
  v3 ground_body_pos; // the ground collider's pose last step, for the carry delta
  quat ground_body_rot;
  b32 ground_body_valid;
  b32 swimming;
  b32 running;        // run input applied while moving
  f32 coyote;
  f32 jump_buffer;
  f32 footstep_dist;
  f32 land_speed;     // fall speed of the last landing
  b32 footstep_running;
  u32 jumps;
  u32 landings;
  u32 footsteps;
  u32 launches;
  u32 teleports;
  b32 placed;
} Character;
ECS_COMPONENT_TYPEDEF(Character);

// optional rigidbody-mover target: gets the interpolated feet pose and the facing
typedef struct CharacterVisual {
  EcsEntity entity;
} CharacterVisual;
ECS_COMPONENT_TYPEDEF(CharacterVisual);

// the frame's interpolation alpha, from the game's FixedTimestep; written on the main lane before the scope
typedef struct CharacterFrame {
  f32 alpha;
} CharacterFrame;
ECS_COMPONENT_TYPEDEF(CharacterFrame);

// skinned model + idle/run/fall clips driven from the Character state; models are authored at the feet
typedef struct CharacterAvatarDesc {
  CString model_path;      // NULL -> gameplay_blocks/character/default_character.glb with its three clips
  CString idle_clip_path;
  CString run_clip_path;
  CString fall_clip_path;
  v3 local_offset;
  v3 local_scale;          // zero -> 1
  quat local_rotation;     // zero quat -> identity: the model faces +Z, the character's forward
  f32 run_speed_lo;        // planar speed where idle starts blending into run; 0 -> 0.5
  f32 run_speed_hi;        // planar speed where run is full; 0 -> 4
} CharacterAvatarDesc;

// main lane, once, after ecs_assets_init and ecs_physics_init: the CharacterFrame resource
void character_controller_init(EcsWorld *w);

// main lane: spawns a character entity named `name` at `feet` (code-built alternative to authoring the scene)
EcsEntity character_spawn(EcsWorld *w, CString name, v3 feet, const CharacterTuning *tuning);

// main lane: adds the skinned avatar as a child of `character` (of its CharacterVisual pivot for a rigidbody mover, made if absent) and the animation driver state
void character_avatar_add(EcsWorld *w, EcsEntity character, const CharacterAvatarDesc *desc);

// inside the scope, once per fixed step: teleport, then move (kinematic or rigidbody)
void character_schedule_fixed(EcsWorld *w);
// inside the scope, once per frame before ecs_transform_schedule: attach (Character from the next frame), pose interpolation and the avatar animation
void character_schedule_frame(EcsWorld *w);

// feet to head of the standing capsule, defaults resolved
f32 character_stand_height(const CharacterTuning *tuning);
// tuning with every zero field replaced by its default
CharacterTuning character_tuning_resolve(CharacterTuning t);
// y of the first walkable surface at or below from_y (search up to max_drop); `fallback` when no layer is built or nothing is hit
f32 character_spawn_height(EcsWorld *w, f32 x, f32 z, f32 from_y, f32 max_drop, f32 fallback);
// horizontal speed relative to the ground the character stands on
f32 character_planar_speed(const Character *c);
// standing on a walkable surface
force_inline b32 character_grounded(const Character *c) { return c->ground == CHARACTER_GROUND_ON_GROUND; }
