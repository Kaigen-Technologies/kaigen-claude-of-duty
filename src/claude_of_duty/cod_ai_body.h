// Claude of Duty soldier bodies, the brain/body seam: the brain writes CodSoldierDrive, the body writes CodSoldierPose.
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "claude_of_duty.h"

// the rig bones
typedef enum CodRigBone {
  COD_BONE_HIPS = 0,
  COD_BONE_SPINE,
  COD_BONE_SPINE1,
  COD_BONE_SPINE2,
  COD_BONE_NECK,
  COD_BONE_HEAD,
  COD_BONE_HEAD_TOP,
  COD_BONE_CLAVICLE_R,
  COD_BONE_UPPER_ARM_R,
  COD_BONE_FOREARM_R,
  COD_BONE_HAND_R,
  COD_BONE_FINGERS_R,
  COD_BONE_CLAVICLE_L,
  COD_BONE_UPPER_ARM_L,
  COD_BONE_FOREARM_L,
  COD_BONE_HAND_L,
  COD_BONE_FINGERS_L,
  COD_BONE_UP_LEG_R,
  COD_BONE_LEG_R,
  COD_BONE_FOOT_R,
  COD_BONE_TOE_R,
  COD_BONE_UP_LEG_L,
  COD_BONE_LEG_L,
  COD_BONE_FOOT_L,
  COD_BONE_TOE_L,
  COD_BONE_COUNT,
} CodRigBone;

// bind positions, actor space at the 1.8 m rest height (elbows solved, fingers from the 0.075 m leaf stub)
hz_internal const v3 cod_rig_bind[COD_BONE_COUNT] = {
    {0.0f, 0.98f, -0.005f}, // Hips
    {0.0f, 1.09f, -0.012f}, // Spine
    {0.0f, 1.215f, 0.0f}, // Spine1
    {0.0f, 1.345f, 0.006f}, // Spine2
    {0.0f, 1.475f, -0.008f}, // Neck
    {0.0f, 1.552f, 0.004f}, // Head
    {0.0f, 1.8f, 0.012f}, // HeadTop
    {-0.038f, 1.408f, 0.016f}, // ClavicleR
    {-0.172f, 1.425f, 0.004f}, // UpperArmR
    {-0.221645f, 1.139583f, -0.009141f}, // ForearmR
    {-0.118441f, 1.277297f, 0.179032f}, // HandR
    {-0.095993f, 1.251107f, 0.245629f}, // FingersR
    {0.038f, 1.408f, 0.016f}, // ClavicleL
    {0.172f, 1.425f, 0.004f}, // UpperArmL
    {0.078326f, 1.229738f, 0.196867f}, // ForearmL
    {-0.096841f, 1.303514f, 0.366864f}, // HandL
    {-0.120812f, 1.281041f, 0.434283f}, // FingersL
    {-0.092f, 0.945f, 0.0f}, // UpLegR
    {-0.098f, 0.505f, 0.02f}, // LegR
    {-0.103f, 0.088f, -0.022f}, // FootR
    {-0.103f, 0.03f, 0.108f}, // ToeR
    {0.092f, 0.945f, 0.0f}, // UpLegL
    {0.098f, 0.505f, 0.02f}, // LegL
    {0.103f, 0.088f, -0.022f}, // FootL
    {0.103f, 0.03f, 0.108f}, // ToeL
};

// each bone's parent, -1 for the root
hz_internal const i8 cod_rig_parent[COD_BONE_COUNT] = {-1, 0, 1, 2, 3, 4, 5, 3, 7, 8, 9, 10, 3, 12, 13, 14, 15, 0, 17, 18, 19, 0, 21, 22, 23};

// clips the brain selects
typedef enum CodSoldierClip {
  COD_CLIP_IDLE = 0,
  COD_CLIP_HURT_IDLE,
  COD_CLIP_WALK,
  COD_CLIP_RUN,
  COD_CLIP_CROUCH_IDLE,
  COD_CLIP_CROUCH_WALK,
  COD_CLIP_COUNT,
} CodSoldierClip;

// the brain's animator input every frame and its one-shot triggers as serials: a trigger fires when its serial changes
HZ_REFLECT()
ECS_COMPONENT(CodSoldierDrive) {
  v3 position; // feet, world
  f32 yaw;     // radians, 0 faces +Z
  f32 scale;   // the variant's body scale
  u32 clip;    // CodSoldierClip
  f32 speed;   // m/s
  b32 crouch;
  b32 has_aim;
  v3 aim_target; // world
  b32 has_look;
  v3 look_target; // world
  f32 aim_weight;
  f32 suppress; // 0..1
  f32 hurt;     // 0..1
  u32 fire_serial; // bumps on a shot, with its strength
  f32 fire_strength;
  u32 hit_serial;  // bumps on a hit: zone, side, strength
  u32 hit_zone;    // CodHitZone
  f32 hit_side;    // -1 left, 1 right
  f32 hit_strength;
  u32 reload_serial; // bumps on a reload, with its duration
  f32 reload_duration;
  u32 vault_serial;  // bumps on a vault, with its duration
  f32 vault_duration;
  u32 turn_serial;   // bumps on a turn in place, with its direction
  f32 turn_dir;
  b32 lod_irrelevant; // nothing can see it: the body evaluates one frame in three on the accumulated dt
  u32 death_serial;   // agent.die: the body hands the pose to its ragdoll
  v3 death_impulse;
  v3 death_point;
  v3 velocity;        // the ragdoll's starting velocity
  f32 hit_limb;       // which limb side took the hit, agent._sideOf: < 0 right, > 0 left; the region is hit_zone
};

// what the body publishes every frame
HZ_REFLECT()
ECS_COMPONENT(CodSoldierPose) {
  b32 ready;       // the body is built and posed at least once
  b32 reloading;
  b32 vaulting;
  b32 ragdoll;     // the ragdoll drives the bones
  v3 muzzle;       // world
  v3 muzzle_dir;   // the bore, world
  v3 eject;        // world
  v3 bones[COD_BONE_COUNT]; // world positions, animated or ragdolled
};

// the body's build progress: the soldiers load stage is the lesser of it and the nav build (cai_load_system)
ECS_COMPONENT_TYPEDEF(CodBodyProgress);
typedef struct CodBodyProgress {
  f32 progress; // 0..1: geometry, textures, GPU upload, materials
  b32 ready;    // soldiers can be attached
} CodBodyProgress;

// main lane, app_init: soldier assets, materials, the ragdoll settings
void cod_ai_body_init(EcsWorld *w);
// inside the scope after cod_ai_schedule: bodies for new soldiers, animation from CodSoldierDrive, ragdolls, CodSoldierPose
void cod_ai_body_schedule(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_ai_body_lanes(EcsWorld *w);
// main lane after the scope: GPU uploads, renderer instances, ragdoll creation
void cod_ai_body_frame(EcsWorld *w);
