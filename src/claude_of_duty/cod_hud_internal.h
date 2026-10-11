// Claude of Duty HUD internals: the model the schedule consolidates for the UI frame, and the UI frame's retained state.
#pragma once
#include "ecs/ecs.h"
#include "gameplay/hud/hud_map.h"
#include "gameplay/hud/hud_messages.h"
#include "gameplay/world_ui/world_ui.h"
#include "lib/string.h"
#include "ui/ui.h"
#include "claude_of_duty.h"

// widget pool sizes
#define COD_HUD_HITS 10
#define COD_HUD_ARCS 6
#define COD_HUD_KILLFEED 6
#define COD_HUD_NUMBERS 16
#define COD_HUD_PINS 6
#define COD_HUD_COMPASS_PINS 5
#define COD_HUD_NADES 4
#define COD_HUD_BLIPS 48
#define COD_HUD_EVENT_HITS 12
#define COD_HUD_EVENT_HURTS 8
#define COD_HUD_EVENT_KILLS 8
#define COD_HUD_MAX_PIPS 30

typedef enum CodHitKind {
  COD_HIT_HIT = 0,
  COD_HIT_ARMOUR,
  COD_HIT_HEAD,
  COD_HIT_KILL,
  COD_HIT_KIND_COUNT,
} CodHitKind;

typedef enum CodNumberKind {
  COD_NUM_HIT = 0,
  COD_NUM_HS,
  COD_NUM_ARMOUR,
  COD_NUM_KILL,
} CodNumberKind;

// one killfeed push
typedef struct CodHudKillRow {
  String32Bytes attacker;
  String32Bytes victim;
  b32 headshot;
  b32 mine;
  b32 attacker_enemy; // the attacker is an enemy
} CodHudKillRow;

// one incoming hit; dx, dz is the world direction to the source
typedef struct CodHudHurt {
  f32 dx;
  f32 dz;
  b32 damage;        // a damage hit: amount from dx, dz
  f32 amount;
} CodHudHurt;

// one minimap contact
typedef struct CodHudBlip {
  f32 x;
  f32 z;
  f32 heading_deg;
  b32 friendly;
} CodHudBlip;

// an objective marker as the world_ui projection placed it this frame
typedef struct CodHudPinView {
  b32 active;
  String32Bytes label;
  String32Bytes name;
  v2 world_xz;
  v2 screen;      // UI px, clamped to the margin ring when off screen
  f32 distance;
  b32 offscreen;
  f32 angle_deg;  // of the chevron
} CodHudPinView;

// a grenade danger marker
typedef struct CodHudNadeView {
  b32 active;
  v2 screen;
  f32 distance;
  f32 age;
  f32 life;
} CodHudNadeView;

// a floating damage number
typedef struct CodHudNumberView {
  b32 active;
  String32Bytes text;
  u32 kind;       // CodNumberKind
  v2 screen;
  f32 distance;
  b32 in_front;
  f32 age;
  f32 life;
  f32 drift;      // px at k = 1, signed
  f32 rise_scale; // 0.9 .. 1.15
} CodHudNumberView;

// the interaction prompt
typedef struct CodHudPromptView {
  b32 active;
  String32Bytes key;
  String32Bytes text;
  String32Bytes sub;
  b32 has_bar;
  f32 progress;
} CodHudPromptView;

// the HUD's view of the match for one frame: written by the HUD's systems, read by the UI frame
typedef struct CodHudModel {
  // ---- what the widgets draw ----
  f32 health, max_health, armour, max_armour;
  b32 regen;
  u32 ammo, reserve, mag_size;
  b32 reloading;
  f32 reload_progress;
  u32 weapon;       // CodWeaponId (the name)
  u32 fire_mode;    // CodFireMode
  u32 lethal, tactical;
  f32 move;         // 0..1 against tactical sprint
  b32 sprint, crouch, ads, airborne;
  f32 base_spread;  // reticle px at k = 1: 4 + spread * 40
  u32 score_us, score_them;
  f32 time_left;
  f32 time;         // the match clock

  // ---- the camera after the frame's update ----
  v2 player_xz;
  v3 cam_pos, cam_fwd, cam_right;
  f32 heading_deg;  // 0 = north, clockwise
  f32 fov_deg;
  f32 rx, rz, fx, fz; // right and forward flattened to xz and normalised
  b32 camera_valid;

  // ---- frame clock ----
  f32 dt;           // the game dt the widgets integrate (0 while paused or frozen)
  f32 raw_dt;       // unscaled, for the menu fade
  u32 frame;
  b32 hud_off;
  b32 stats;        // draw the engine's perf overlay
  b32 trace;        // log the model every frame

  // ---- this frame's events ----
  u32 fire_count;
  f32 flinch;       // explosion flinch amount (max over the frame)
  u32 hit_count;
  u8 hits[COD_HUD_EVENT_HITS]; // CodHitKind
  u32 hurt_count;
  CodHudHurt hurts[COD_HUD_EVENT_HURTS];
  u32 kill_count;
  CodHudKillRow kills[COD_HUD_EVENT_KILLS];

  // ---- projected world items ----
  u32 blip_count;
  CodHudBlip blips[COD_HUD_BLIPS];
  CodHudPinView pins[COD_HUD_PINS];
  CodHudNadeView nades[COD_HUD_NADES];
  CodHudNumberView numbers[COD_HUD_NUMBERS];
  CodHudPromptView prompt;

  // ---- spawn bookkeeping ----
  EcsEntity number_ring[COD_HUD_NUMBERS];
  u32 number_next;
} CodHudModel;
ECS_COMPONENT_TYPEDEF(CodHudModel);

// ---- components on the world_ui entities the HUD draws itself ----

// an objective pin: its label and name
typedef struct CodHudObjective {
  String32Bytes label;
  String32Bytes name;
} CodHudObjective;
ECS_COMPONENT_TYPEDEF(CodHudObjective);

// a grenade danger marker: world_ui_popup's lifetime is the fuse
typedef struct CodHudGrenade {
  u8 _unused;
} CodHudGrenade;
ECS_COMPONENT_TYPEDEF(CodHudGrenade);

// a damage number's presentation
typedef struct CodHudNumber {
  u32 kind;       // CodNumberKind
  f32 drift;      // px at k = 1
  f32 rise_scale;
} CodHudNumber;
ECS_COMPONENT_TYPEDEF(CodHudNumber);

// the prompt's hold bar
typedef struct CodHudPromptBar {
  f32 progress;
  b32 has_bar;
} CodHudPromptBar;
ECS_COMPONENT_TYPEDEF(CodHudPromptBar);

// ---- the UI frame's retained state (main lane only) ----

typedef struct CodHudPoolItem {
  b32 alive;
  f32 t;
  f32 life;
  f32 a, b, c;
  u32 kind;
  u32 serial;
} CodHudPoolItem;

typedef struct CodHudKillItem {
  CodHudPoolItem pool;
  CodHudKillRow row;
} CodHudKillItem;

typedef struct CodHudFx {
  f32 kick, kick_vel, move_spread, ads_blend, hit_pulse, visible;
  f32 cross_gap, cross_len, cross_bright; // derived each frame
  CodHudPoolItem hits[COD_HUD_HITS];
  u32 hits_next;
  CodHudPoolItem arcs[COD_HUD_ARCS];
  u32 arcs_next;
  f32 hurt, flash_t, flash_peak, beat_phase, beat_energy, regen_t, regen_pulse, hp_shown, armour_shown;
  b32 prev_regen;
  f32 punch;
  i32 last_ammo;
  CodHudKillItem kills[COD_HUD_KILLFEED];
  u32 kills_next;
  u32 kill_serial;
  f32 prompt_shown;
  CodHudPromptView prompt_last; // kept so the prompt fades out with its text
  f32 hud_visible;
  b32 started;
} CodHudFx;

typedef struct CodHudMenu {
  b32 open;           // the flow is paused
  f32 shown;          // 0..1 fade on raw time
  i32 dragging;       // the slider being dragged, 1-based; 0 none
  b32 hover_on[8];    // last frame's hover per button, which feeds this frame's tween
  UITween hover[8];
} CodHudMenu;

// the UI system, its fonts and pipelines, and the retained state
typedef struct CodUi {
  UISystem *ui;
  HzHandleT(UIFontFamily) body;
  HzHandleT(UIFontFamily) display;
  HzHandleT(UIFontFamily) mono;
  GpuPipeline band_pipeline;
  GpuPipeline hurt_pipeline;
  GpuPipeline hurt_screen_pipeline;
  GpuPipeline map_pipeline;
  CodHudFx fx;
  CodHudMenu menu;
  OsCursor cursor;
  b32 click_hint;
  b32 loading_seen;
  b32 load_reported; // the loading screen's close was logged: the playable mark, from lane 0 where every platform's log reaches the console
  f32 fps_window_s;  // wall time accumulated toward the next counter refresh
  u32 fps_frames;
  f32 fps_shown;
  f32 frame_ms_shown;
} CodUi;
ECS_COMPONENT_TYPEDEF(CodUi);

// the frame's viewport and the --k scale
typedef struct CodHudView {
  f32 w, h;
  f32 k;          // clamp(h / 1080, 0.62, 2.4)
  f32 dt;         // the game dt
  f32 raw_dt;
  f32 time;
  f32 vis;        // the HUD's fade (hud_visible), which the custom shaders multiply in themselves
  b32 touch;      // the on-screen controls own the bottom corners and the top-right corner
  OsSafeAreaInsets safe; // the notch and home-indicator insets, canvas px; zero on desktop
} CodHudView;
