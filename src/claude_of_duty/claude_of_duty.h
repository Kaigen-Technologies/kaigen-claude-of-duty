// Claude of Duty: the shared vocabulary every package reads (game state, surfaces, quality, the clock, dev switches).
#pragma once
#include "ecs/ecs.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"
#include "physics/physics.h"
#include "gameplay/touch_controls/touch_controls.h"

// the fixed step: 120 Hz, at most 8 steps a frame, frames clamped to 100 ms
#define COD_FIXED_HZ 120.0f
#define COD_FIXED_DT (1.0f / COD_FIXED_HZ)
#define COD_MAX_SUBSTEPS 8
#define COD_MAX_DT 0.1f
// the world seed: every procedural generator forks from it in the init order
#define COD_SEED 0x5eed1234u
// one scene light unit in lux and framebuffer unit in cd/m2: scene intensities scale by it
#define COD_SCENE_UNIT 25000.0f

// world units: metres, seconds, kilograms
#define COD_GRAVITY (9.81f * 2.1f)
#define COD_PLAYER_HEIGHT 1.78f
#define COD_PLAYER_CROUCH_HEIGHT 1.12f
#define COD_PLAYER_RADIUS 0.32f
#define COD_EYE_OFFSET 0.12f // below the top of the capsule
#define COD_FOV 80.0f        // vertical degrees
#define COD_CAMERA_NEAR 0.05f
#define COD_CAMERA_FAR 600.0f

// collision categories on every ColliderBody/EcsCollider filter; queries pick with PhysicsQueryFilter masks
#define COD_COLLIDE_WORLD (1u << 0)   // static level geometry
#define COD_COLLIDE_PROP (1u << 1)    // dynamic props, shells, magazines, grenades
#define COD_COLLIDE_PLAYER (1u << 2)  // the player's capsule
#define COD_COLLIDE_HITBOX (1u << 3)  // soldier hit volumes (bullets only)
#define COD_COLLIDE_RAGDOLL (1u << 4) // ragdoll bones
#define COD_COLLIDE_GLASS (1u << 5)   // panes bullets pass through (penetration) and the character collides with

// the shared surface vocabulary for impacts, decals, audio and footsteps
typedef enum CodSurface {
  COD_SURFACE_CONCRETE = 0,
  COD_SURFACE_METAL,
  COD_SURFACE_WOOD,
  COD_SURFACE_DIRT,
  COD_SURFACE_SAND,
  COD_SURFACE_GLASS,
  COD_SURFACE_WATER,
  COD_SURFACE_FOLIAGE,
  COD_SURFACE_FABRIC,
  COD_SURFACE_FLESH,
  COD_SURFACE_RUBBER,
  COD_SURFACE_PLASTER,
  COD_SURFACE_COUNT,
} CodSurface;

// the surface a collider reports to a bullet or a footstep; on every world collider entity (absent = concrete)
HZ_REFLECT()
ECS_COMPONENT(CodSurfaceTag) {
  u32 surface; // CodSurface
};

// the quality presets; ultra is the default
typedef enum CodQuality {
  COD_QUALITY_LOW = 0,
  COD_QUALITY_MEDIUM = 1,
  COD_QUALITY_HIGH = 2,
  COD_QUALITY_ULTRA = 3,
  COD_QUALITY_COUNT = 4,
} CodQuality;

typedef struct CodQualityPreset {
  RendererUpscaleQuality upscale; // the TAAU input tier (the vendor upscaler's optimal size where one runs)
  u32 shadow_map_size;
  u32 cascades;
  f32 shadow_distance;
  b32 taa;
  b32 gtao;
  b32 ssr;
  b32 volumetrics;
  b32 motion_blur;
  b32 bloom;
  u32 anisotropy;
  u32 particle_budget;
  u32 decal_budget;
  f32 max_dpr; // the frame's resolution limit, final pass and HUD included
} CodQualityPreset;

// the game state machine: the match runs at once; Esc or a lost pointer lock pauses
typedef enum CodGameState {
  COD_STATE_LOADING = 0, // procedural generation and GPU bakes; the load screen shows progress
  COD_STATE_PLAYING = 1, // the match runs; without pointer lock the "click to play" prompt shows
  COD_STATE_PAUSED = 2,  // the pause menu
} CodGameState;

// the load stages each package reports into CodLoad; the flow leaves LOADING when every stage is done
typedef enum CodLoadStage {
  COD_LOAD_MATERIALS = 0, // procedural texture bakes
  COD_LOAD_WORLD,         // level geometry, colliders, lights
  COD_LOAD_WEAPONS,       // weapon and hand meshes
  COD_LOAD_SOLDIERS,      // soldier meshes, textures, skins
  COD_LOAD_FX,            // particle and decal atlases, effect assets
  COD_LOAD_AUDIO,         // synthesized clips
  COD_LOAD_PIPELINES,     // renderer_pipelines_ready
  COD_LOAD_STAGE_COUNT,
} CodLoadStage;

// each package's load progress: the owner writes its own slot (0..1) from its _single system; the flow reads them
typedef struct CodLoad {
  f32 progress[COD_LOAD_STAGE_COUNT];
  f64 started_s;  // os time the load began
  f64 stage_ms[COD_LOAD_STAGE_COUNT]; // wall time each stage took, logged at the end
  b32 failed;
} CodLoad;
ECS_COMPONENT_TYPEDEF(CodLoad);

// the frame clock: dt drives presentation, the fixed step drives physics and gameplay
typedef struct CodClock {
  f32 dt;      // the frame's dt, clamped to COD_MAX_DT; 0 while frozen
  f32 raw_dt;  // unscaled, unfrozen
  f32 time;    // the sum of dt: the renderer's and the effects' clock
  u32 frame;   // frames since start
  FixedTimestep fixed; // COD_FIXED_HZ, COD_MAX_SUBSTEPS
  i32 steps;   // fixed steps this frame (every lane takes the same count)
  f32 alpha;   // the fixed-step remainder for interpolation
  b32 live;    // the match simulates this frame: playing with controls (or a dev-switch run), not frozen
  b32 controls; // the player reads input this frame
} CodClock;
ECS_COMPONENT_TYPEDEF(CodClock);

// dev switches for debugging and scripted runs: `set:CodDevSwitches.<field>=<value>` from --commands
HZ_REFLECT()
ECS_COMPONENT(CodDevSwitches) {
  i32 quality;   // CodQuality; -1 = keep the settings' preset
  b32 start;     // runs the match without pointer lock (a dev-switch run never pauses on focus or lock loss)
  i32 screen;    // 0 none, 1 the pause menu
  f32 time_of_day; // hours 0..24; < 0 keeps the sky's own clock
  b32 freeze;    // the simulation pauses, rendering continues
  b32 god;       // the player takes no damage
  b32 no_ai;     // no soldiers spawn
  b32 pose;      // places the player at pose_* (feet), facing pose_yaw/pose_pitch
  f32 pose_x;
  f32 pose_y;
  f32 pose_z;
  f32 pose_yaw;   // radians, 0 faces -Z
  f32 pose_pitch; // radians, + looks up
  b32 look;       // the view looks at look_* after any pose
  b32 aim_soldier; // the view tracks the chest of the live soldier nearest the player (scripted kill runs)
  f32 look_x;
  f32 look_y;
  f32 look_z;
  f32 fov;        // vertical degrees; 0 keeps the player's
  i32 weapon;     // the weapon in hand: -1 keep, 0 rifle, 1 smg, 2 pistol
  i32 fx_dump;    // 1 bakes the decal tiles and writes them as assets/claude_of_duty/fx/decals/*.png and .hzt (the cook turns them into BC7)
  b32 hud_off;    // no HUD drawn
  b32 hud_stats;  // the engine's perf overlay
  b32 hud_trace;  // logs one line of the HUD's model and widgets every frame
  b32 nan;        // NaN/Inf pixels show in magenta
  i32 player_course; // 0 none, 1 spawns the movement test course of box colliders (vault, mantle, walls, ceiling slab, stairs)
  b32 player_trace;  // logs one line of the player's state every frame
  b32 weapon_trace;  // logs one line of the player's weapon state every frame
  b32 ai_trace;   // logs every soldier state transition, the squads and the AI cost
  i32 ai_debug;   // 0 none, 1 draws the hit capsules and paths, 2 also the cover points, 3 also the soldiers' navmesh around the player
  b32 audio_meter;   // logs the bus levels twice a second
};

// the dev switches resolved once per frame, for every package's per-entity systems
typedef struct CodDev {
  u32 state;          // CodGameState
  b32 debug_controls; // a dev-switch run: input without pointer lock
  u32 restart_serial; // bumps on every match start
  b32 pose;           // a pose is set: the player stands at feet facing yaw/pitch
  v3 feet;
  f32 yaw;
  f32 pitch;
  u32 pose_serial;    // bumps when the pose (or look) switches change: the player re-places
  f32 fov;            // > 0 overrides the player's vertical fov
  f32 time_of_day;    // < 0 keeps the sky's clock
  b32 god;
  b32 no_ai;
  i32 weapon;         // -1 keep
  b32 hud_off;
  b32 weapon_trace;
} CodDev;
ECS_COMPONENT_TYPEDEF(CodDev);

// the match flow: load, pause, pointer lock policy
typedef struct CodFlow {
  u32 state;          // CodGameState
  b32 debug_controls; // a dev-switch run takes input without pointer lock and never pauses on lock or focus loss
  u32 lock_serial;    // a pointer lock/unlock request; the main lane applies os_lock_mouse(lock_value) once per serial
  b32 lock_value;
  b32 audio_suspended;
  b32 was_focused;
  u32 loaded_frame;   // the clock frame the load finished on
  b32 sw_start;       // the switches as last latched
  i32 sw_screen;
  i32 sw_quality;
} CodFlow;
ECS_COMPONENT_TYPEDEF(CodFlow);

// the player's settings, written by the pause menu; owners read serial to apply
typedef struct CodSettings {
  i32 quality;      // CodQuality
  f32 sensitivity;  // multiplier on the base mouse sensitivity
  f32 fov;          // vertical degrees
  f32 volume;       // master 0..1
  b32 invert_y;
  u32 serial;       // bumps on every change
} CodSettings;
ECS_COMPONENT_TYPEDEF(CodSettings);

// the frame's bindings on the engine InputSystem, written on the main lane before the scope
typedef struct CodInput {
  v2 move;            // x right, y forward, each -1..1 (WASD / arrows / left stick)
  v2 look;            // pointer movement this frame, pixels (+x right, +y down); zero unless captured
  i32 wheel;          // wheel notches this frame (+ up)
  b32 jump, jump_pressed;
  b32 crouch, crouch_pressed;   // Left Ctrl / C
  b32 prone, prone_pressed;     // Z
  b32 sprint, sprint_pressed;   // Left Shift
  b32 fire, fire_pressed;       // left button
  b32 ads, ads_pressed;         // right button
  b32 reload_pressed;           // R
  b32 use_pressed;              // F
  b32 melee_pressed;            // V
  b32 lean_left, lean_right;    // Q / E
  b32 swap_pressed;             // Tab
  i32 weapon_slot_pressed;      // 1, 2 or 3 pressed this frame (0 none)
  b32 fire_mode_pressed;        // B
  b32 inspect_pressed;          // I
  b32 grenade_pressed;          // G
  b32 flashlight_pressed;       // T
  b32 pause_pressed;            // Escape (the release of a captured pointer)
  b32 captured;       // the match takes input: the pointer is locked, the last input was a touch, or a dev-switch run
  b32 touch;          // the last input was a touch: the on-screen controls drive the match, no pointer lock
  b32 lock_gained;    // the lock edges this frame
  b32 lock_lost;
  b32 focused;
  b32 focus_lost;
  b32 held;           // an open editor holds the match: no input reaches it
} CodInput;
ECS_COMPONENT_TYPEDEF(CodInput);

// the on-screen buttons, indexed into TouchControlsConfig.buttons
typedef enum CodTouchButton {
  COD_TOUCH_FIRE = 0,
  COD_TOUCH_ADS,
  COD_TOUCH_JUMP,
  COD_TOUCH_CROUCH,
  COD_TOUCH_RELOAD,
  COD_TOUCH_SWAP,
  COD_TOUCH_PAUSE,
  COD_TOUCH_BUTTON_COUNT,
} CodTouchButton;

// main-lane touch state: the stick, look and buttons, the aim toggle and the over-pushed stick's sprint
typedef struct CodTouch {
  TouchControls tc;
  b32 ads;
  b32 sprint;
} CodTouch;
ECS_COMPONENT_TYPEDEF(CodTouch);

// the editor session (native dev builds): while editing the match is frozen and the camera is the editor's
typedef struct CodEditor {
  b32 open;
  v3 eye;
  quat look;
} CodEditor;
ECS_COMPONENT_TYPEDEF(CodEditor);
