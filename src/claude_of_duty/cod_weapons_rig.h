// Claude of Duty weapons, package-private: definitions, clips and the viewmodel rig state.
#pragma once
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "cod_rng.h"
#include "cod_weapons.h"
#include "cod_weapons_internal.h"

#define CWP_PATTERN_MAX 32
#define CWP_CLIMB_MAX 5

typedef struct CwpRecoilDef {
  f32 pitch;     // radians of camera climb per shot
  f32 yaw;
  f32 kick_back; // metres the viewmodel travels rearward
  f32 kick_up;
  f32 roll;
  f32 punch;
  f32 freq;
  f32 damping;
  u32 pattern_length;
  u32 pattern_seed;
  f32 climb[CWP_CLIMB_MAX]; // first-shots multiplier
  u32 climb_count;
  f32 drift;
} CwpRecoilDef;

// every number of a weapon definition
typedef struct CwpDef {
  const char *label;
  u32 mode_count;
  u32 modes[3]; // CodFireMode
  f32 rpm;
  u32 burst_count;
  f32 burst_rpm;
  f32 burst_delay;
  u32 mag_size;
  u32 reserve;
  f32 muzzle_velocity;
  f32 damage;
  f32 penetration;
  f32 dropoff;
  f32 max_range;
  f32 drag_k;
  u32 tracer_every;
  f32 spread_hip;
  f32 spread_ads;
  f32 spread_per_shot;
  f32 spread_max;
  f32 spread_decay;
  CwpRecoilDef recoil;
  f32 ads_time;
  f32 view_fov;
  f32 reload_tac;
  f32 reload_empty;
  f32 inspect_time;
  f32 draw_time;
  f32 holster_time;
  v3 hip_pos;
  v3 hip_rot;
  v3 ads_cant;
  f32 eye_relief;
  v3 sprint_pos;
  v3 sprint_rot;
  v3 low_ready_pos;
  v3 low_ready_rot;
  f32 sway_scale;
  f32 bob_scale;
  f32 mag_len;
} CwpDef;

// spread multipliers
#define CWP_SPREAD_CROUCH 0.78f
#define CWP_SPREAD_PRONE 0.6f
#define CWP_SPREAD_STILL 0.82f
#define CWP_SPREAD_WALKING 1.15f
#define CWP_SPREAD_SPRINTING 2.2f
#define CWP_SPREAD_AIRBORNE 2.0f

const CwpDef *cwp_def(u32 weapon);
// the deterministic per-shot camera climb: pairs of (pitch, yaw) radians
void cwp_build_recoil_pattern(const CwpDef *def, f32 *out);

// ---- clips ----

typedef enum CwpClipId {
  CWP_CLIP_NONE = -1,
  CWP_CLIP_RELOAD_TAC = 0,
  CWP_CLIP_RELOAD_EMPTY,
  CWP_CLIP_INSPECT,
  CWP_CLIP_DRAW,
  CWP_CLIP_HOLSTER,
  CWP_CLIP_COUNT,
} CwpClipId;

typedef enum CwpEase {
  CWP_EASE_SMOOTH = 0,
  CWP_EASE_LINEAR,
  CWP_EASE_OUT,
  CWP_EASE_BACK,
} CwpEase;

// the generic hand poses a clip names; the weapon's fitted clamp is a separate slot the viewmodel owns
typedef enum CwpHandPose {
  CWP_POSE_GRIP = 0,
  CWP_POSE_WRAP,
  CWP_POSE_CLAMP,
  CWP_POSE_CUP,
  CWP_POSE_OPEN,
  CWP_POSE_PINCH,
  CWP_POSE_COUNT,
} CwpHandPose;

typedef enum CwpClipEvent {
  CWP_EV_START = 0,
  CWP_EV_MAGOUT,
  CWP_EV_MAGDROP,
  CWP_EV_MAGIN,
  CWP_EV_SLAP,
  CWP_EV_CHARGE,
  CWP_EV_BOLTRELEASE,
  CWP_EV_END,
} CwpClipEvent;

#define CWP_CLIP_KEYS 16
#define CWP_CLIP_EVENTS 8

// the head every key struct starts with
typedef struct CwpKeyHead {
  f32 t;
  u32 ease;
} CwpKeyHead;

typedef struct CwpKeyWeapon {
  f32 t;
  u32 ease;
  v3 p;
  v3 r;
} CwpKeyWeapon;

typedef struct CwpKeyHand {
  f32 t;
  u32 ease;
  v3 p;
  v3 finger;
  v3 back;
  u32 pose;
  f32 weight;
} CwpKeyHand;

typedef struct CwpKeyParts {
  f32 t;
  u32 ease;
  f32 mag;
  f32 mag_visible;
  f32 charge;
  f32 bolt;
  f32 slide;
} CwpKeyParts;

typedef struct CwpClipEventKey {
  f32 t;
  u32 name; // CwpClipEvent
} CwpClipEventKey;

typedef struct CwpClip {
  f32 duration;
  u32 weapon_count;
  CwpKeyWeapon weapon[CWP_CLIP_KEYS];
  u32 hand_count;
  CwpKeyHand hand[CWP_CLIP_KEYS];
  u32 parts_count;
  CwpKeyParts parts[CWP_CLIP_KEYS];
  u32 event_count;
  CwpClipEventKey events[CWP_CLIP_EVENTS];
} CwpClip;

// a clip sampled at a time
typedef struct CwpClipSample {
  b32 active;
  v3 pos;
  v3 rot;
  v3 hand_pos;
  v3 hand_finger;
  v3 hand_back;
  u32 hand_pose;
  f32 hand_weight;
  f32 mag;
  b32 mag_visible;
  f32 charge;
  f32 bolt;
  f32 slide;
} CwpClipSample;

// builds the five clips of a weapon from its attachment nodes
typedef struct CwpClipNodes {
  v3 grip_pos;
  v3 grip_finger;
  v3 grip_back;
  v3 mag_seat;
  b32 has_charge;
  v3 charge_rest;
} CwpClipNodes;
void cwp_build_clips(CwpClip *out, const CwpClipNodes *nodes, const CwpDef *def);
void cwp_clip_sample(const CwpClip *c, f32 t, CwpClipSample *out);

// ---- scalar kit ----

force_inline f32 cwp_clamp01(f32 v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
force_inline f32 cwp_smootherstep(f32 a, f32 b, f32 x) {
  f32 d = b - a;
  f32 t = cwp_clamp01((x - a) / (d == 0.0f ? 1e-6f : d));
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// ---- the viewmodel rig ----

#define CWP_NOISE_SIZE 512
#define CWP_NOISE_COUNT 6

// layered value noise with cubic interpolation
typedef struct CwpNoise {
  f32 t[CWP_NOISE_SIZE];
} CwpNoise;

typedef struct CwpSpring {
  DampedSpring s;
  DampedSpringTuning tune;
} CwpSpring;

typedef struct CwpSpring3 {
  CwpSpring a, b, c;
} CwpSpring3;

// what the weapon system hands the rig each frame, plus the camera
typedef struct CwpViewInput {
  b32 ads;
  b32 sprint;
  b32 low_ready;
  f32 speed;
  b32 airborne;
  b32 trigger;
  quat cam_rot; // the world camera rotation
} CwpViewInput;

// called mid-update for a clip's named beats
typedef void (*CwpClipEventFn)(void *user, u32 event, i32 clip);

typedef struct CwpView {
  f32 ads_t;
  f32 sprint_t;
  f32 low_ready_t;
  f32 bob_phase;
  f32 noise_t;
  f32 trigger_t;
  CwpSpring3 lag;
  CwpSpring3 lag_rot;
  CwpSpring3 rec_pos;
  CwpSpring3 rec_rot;
  CwpSpring3 settle;
  CwpSpring jump;
  CwpSpring land;
  CwpNoise noise[CWP_NOISE_COUNT];
  f32 ang_yaw;
  f32 ang_pitch;
  f32 prev_yaw;
  f32 prev_pitch;
  b32 has_prev;
  i32 clip; // CwpClipId playing, CWP_CLIP_NONE
  f32 clip_t;
  f32 clip_prev_t;
  CwpClipSample res;
  f32 bolt_cycle; // 0..1, driven by firing
  f32 bolt_hold;  // 1 = locked back (empty)
  CodRng rng;
  // outputs of the last update, camera space unless noted
  v3 rig_pos;
  quat rig_rot;
  f32 fov_scale; // the viewmodel fov multiplier at this ADS
  v3 hand_pos_r;  // weapon space
  quat hand_rot_r;
  v3 hand_pos_l;
  quat hand_rot_l;
  u32 hand_pose_rest; // the support hand's pose without a clip: the weapon's fitted clamp or cup, set by the owner
  u32 hand_pose_l;    // a CwpHandPose or CWP_POSE_FITTED
  f32 trigger_out;
  v3 bolt_pos;    // part transforms, weapon space
  v3 slide_pos;
  v3 charge_pos;
  f32 trigger_rot;
  v3 mag_pos;
  quat mag_rot;
  b32 mag_visible;
  v3 shoulder_r; // arm shoulders, rig space
  v3 shoulder_l;
} CwpView;

#define CWP_POSE_FITTED 0x100u // a CwpHandPose below this; the weapon's fitted clamp at or above it

// seeds the noise tables and zeroes the springs
void cwp_view_init(CwpView *v, CodRng rng);
// per-shot viewmodel kick
void cwp_view_add_recoil(CwpView *v, const CwpDef *def, f32 pitch, f32 yaw, b32 first);
void cwp_view_jump(CwpView *v);
void cwp_view_land(CwpView *v, f32 speed);
// resets the animation stack
void cwp_view_reset(CwpView *v);
void cwp_view_play(CwpView *v, i32 clip);
void cwp_view_stop_clip(CwpView *v);
// one frame: the base pose, the additive layers, the clip, the hand targets and the part transforms
void cwp_view_update(CwpView *v, const CwpModel *model, const CwpDef *def, const CwpClip *clips, const CwpViewInput *in, f32 dt,
                     CwpClipEventFn on_event, void *user);
quat cwp_hand_basis(v3 finger, v3 back);
