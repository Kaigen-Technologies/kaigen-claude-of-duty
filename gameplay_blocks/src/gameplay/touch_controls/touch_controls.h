// ControlModule/TouchGui analog: on-screen thumbstick + jump button and
// the touch/camera exclusion rule. Pure view over InputSystem.touches (no
// ecs_system, no allocations, fixed slots), main lane only.

// Camera exclusion: a touch STARTING inside the thumbstick region is excluded
// from the camera for its lifetime; of the rest, 1 -> pan, 2 -> pinch by
// diameter delta, reset whenever the participant count changes.

// Usage: touch_controls_init(cfg), then per frame after input_update call
// touch_controls_update and sum its move/jump into CharacterInput (reclamping)
// and its pan/pinch into CameraRigInput; draw with touch_controls_draw or your own look over the views.
#pragma once

#include "input.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "ui/ui.h"

// FIXED is reserved and NOT implemented (the reference aliases every touch scheme to
// dynamic); touch_controls_update asserts DYNAMIC in debug builds.
typedef enum {
  TOUCH_THUMBSTICK_DYNAMIC = 0,
  TOUCH_THUMBSTICK_FIXED,
} TouchThumbstickMode;

// which of the two layouts a frame uses: landscape when screen width >=
// height, portrait otherwise. Also the index space of TouchControls.resolved.
typedef enum {
  TOUCH_ORIENTATION_LANDSCAPE = 0,
  TOUCH_ORIENTATION_PORTRAIT,
  TOUCH_ORIENTATION_COUNT,
} TouchOrientation;

// action buttons per TouchControls: fire, aim, reload... indexed by the caller's own enum
#define TOUCH_BUTTON_MAX 8

// the safe-area corner a button's offset is measured from
typedef enum {
  TOUCH_ANCHOR_BOTTOM_RIGHT = 0,
  TOUCH_ANCHOR_BOTTOM_LEFT,
  TOUCH_ANCHOR_TOP_RIGHT,
  TOUCH_ANCHOR_TOP_LEFT,
} TouchAnchor;

// one action button's place in one orientation; diameter 0 = absent there
typedef struct {
  TouchAnchor anchor;
  v2 corner_offset_px; // anchor corner -> button CENTER, both axes inward
  f32 diameter_px;
} TouchButtonPlacement;

// every widget placement/size/feel number, per orientation. px fields are
// LOGICAL px scaled by the resolved ui_scale at use; _pct pairs are fractions
// of the safe-area rect and are NOT scaled.
typedef struct {
  // explicit opt-in, not a 0-sentinel: 0 is legal for region_pos_pct and
  // deadzone_radius_px. false = ignore every field below, use the defaults
  b32 custom;

  v2 region_pos_pct;  // stick region origin, fraction of the safe-area rect
  v2 region_size_pct; // region size; also the camera-pan dead region

  f32 stick_outer_radius_px;  // draw-only, no effect on the move vector
  f32 stick_ring_radius_px;
  f32 stick_middle_radius_px; // thumb dot

  f32 deadzone_radius_px;  // finger travel below this -> move stays 0
  f32 max_speed_radius_px; // travel at/above this -> |move| == 1

  f32 jump_diameter_px;
  v2 jump_corner_offset_px; // safe-area BOTTOM-RIGHT corner -> button CENTER
                            // (x inward, y upward); the only anchor

  // pinch rescue window: a stick claim is REVOKED if another touch STARTED
  // within this long of it (either order), so both fingers pinch instead.
  // start times, not liveness, so an old camera drag never revokes. 0 = off
  f32 pinch_grace_s;

  TouchButtonPlacement buttons[TOUCH_BUTTON_MAX]; // by TouchControlsConfig.buttons index
} TouchControlsLayout;

// what an action button does; zero-init = the slot is unused
typedef struct {
  b32 enabled;
  // the finger holding it also turns the camera (fire while aiming); its drag adds to camera_pan_px
  b32 look_through;
  CString label; // drawn centred with TouchControlsConfig.label; NULL = none
} TouchButtonDesc;

typedef struct {
  b32 held;     // a touch that started on it is down
  b32 pressed;  // that touch started this frame
  b32 released; // that touch ended this frame
} TouchButtonState;

// defaults for one orientation, with `custom` set so the result can be
// assigned into TouchControlsConfig and partially edited.
TouchControlsLayout touch_controls_layout_default(TouchOrientation orientation);

typedef struct {
  TouchThumbstickMode thumbstick_mode; // zero-init = DYNAMIC (the only impl)
  b32 force_enabled;       // active regardless of the input category
  b32 disable_jump_button; // zero-init = jump button enabled
  f32 ui_scale;            // <=0 = auto: small_screen_ui_scale below
                           // small_screen_max_dim_px, else 1.0
  f32 small_screen_max_dim_px; // <=0 = 500 (auto-scale threshold)
  f32 small_screen_ui_scale;   // <=0 = 0.8 (auto-scale below the threshold)
  UISystem *ui;                // touches starting over pointer-capturing UI stay with the UI; NULL = no check
  // two free fingers keep panning with the earliest one instead of pinching (camera_pinch_px stays 0)
  b32 disable_pinch;

  TouchButtonDesc buttons[TOUCH_BUTTON_MAX];
  HzUITextConfig label; // button label style; fontSize 0 = 14 at ui_scale, white

  // zero-init = built-in defaults for both; see TouchControlsLayout.custom
  TouchControlsLayout landscape;
  TouchControlsLayout portrait;
} TouchControlsConfig;

typedef struct {
  v2 move;              // camera-relative (x = screen-right, y = forward),
                        // |move| <= 1; additive callers must reclamp
  f32 stick_travel;     // travel / max-speed radius, unclamped: > 1 = over-push (sprint)
  b32 jump_held;        // level while the jump touch is down
  TouchButtonState buttons[TOUCH_BUTTON_MAX];
  v2 camera_pan_px;     // this-frame delta of the camera touch + look-through fingers, logical px, NOT dt-scaled
  f32 camera_pinch_px;  // this-frame 2-touch diameter delta, + = spreading
  b32 active;           // widgets should be visible/routing this frame
  f32 last_pan_time;    // timestamp of the last unclaimed 1-finger camera
                        // touch; aim-assist eligibility seam (not implemented)
} TouchControlsFrame;

typedef struct TouchControls {
  TouchControlsConfig config;

  // resolved once at init (layouts are screen-independent); ui_scale stays
  // per-frame since it depends on live screen dims
  TouchControlsLayout resolved[TOUCH_ORIENTATION_COUNT];
  f32 small_screen_max_dim_px;
  f32 small_screen_ui_scale;

  b32 stick_touch_claimed;
  u32 stick_touch_id;
  v2 stick_center_px;       // dynamic spawn origin (touch start position)
  v2 stick_thumb_offset_px; // current visual thumb offset, clamped to the
                            // max-speed radius, for touch_controls_draw

  // touch revoked by the pinch grace window; ineligible for the stick until
  // it lifts, so releasing one pinch finger cannot start a walk
  b32 stick_excluded_valid;
  u32 stick_excluded_touch_id;

  b32 jump_touch_claimed;
  u32 jump_touch_id;

  // action buttons: the claiming touch, its last position (look-through) and the app's per-frame state
  b32 button_claimed[TOUCH_BUTTON_MAX];
  u32 button_touch_id[TOUCH_BUTTON_MAX];
  v2 button_prev_px[TOUCH_BUTTON_MAX];
  b32 button_hidden[TOUCH_BUTTON_MAX];
  b32 button_lit[TOUCH_BUTTON_MAX];
  v2 button_center_px[TOUCH_BUTTON_MAX];
  f32 button_diameter_px[TOUCH_BUTTON_MAX];

  u32 cam_touch_ids[2];
  u32 cam_touch_count;
  v2 cam_touch_prev_px[2];

  // geometry cached each update() for the views and draws to reuse verbatim
  v2 stick_region_pos_px;
  v2 stick_region_size_px;
  v2 jump_button_center_px;
  f32 jump_button_diameter_px;
  f32 resolved_ui_scale;
  b32 last_landscape; // orientation update() picked, so draw() reads the same
                      // entry of `resolved`
  b32 last_active;
  f32 last_pan_time;
} TouchControls;

TouchControls touch_controls_init(TouchControlsConfig cfg);

// per-frame, main lane, after input_update and before the UI frame; claims touches (mutating `in`)
TouchControlsFrame touch_controls_update(TouchControls *tc, InputSystem *in, f32 now);

// per frame: hidden claims no new touch (a held one stays until lifted); lit draws highlighted (a toggle that is on)
void touch_controls_set_button(TouchControls *tc, u32 index, b32 visible, b32 lit);

// the stick as the last touch_controls_update left it; screen px, ui_scale applied
typedef struct {
  b32 visible; // the controls are active and a finger holds the stick (a dynamic stick exists only while held)
  v2 center_px;            // where that finger landed
  v2 thumb_px;             // thumb centre, at most max_speed_radius_px from center_px
  f32 outer_radius_px;
  f32 ring_radius_px;
  f32 thumb_radius_px;
  f32 max_speed_radius_px; // thumb distance at full speed
  v2 region_pos_px;        // where a touch can start the stick
  v2 region_size_px;
} TouchStickView;

// one button as the last touch_controls_update left it: draw over exactly this circle and it matches the hit test
typedef struct {
  b32 visible; // active, enabled, placed in this orientation, and shown (or still held)
  b32 held;
  b32 lit;
  v2 center_px;
  f32 diameter_px;
  CString label;
} TouchButtonView;

// read-only views for a game drawing its own controls
TouchStickView touch_controls_stick(const TouchControls *tc);
TouchButtonView touch_controls_button(const TouchControls *tc, u32 index);
TouchButtonView touch_controls_jump_button(const TouchControls *tc); // the built-in jump (disable_jump_button hides it)

// the default look, one widget at a time; inside a ui_begin_frame/ui_finish_layout scope, no-op when not visible
void touch_controls_draw_stick(const TouchControls *tc);
void touch_controls_draw_button(const TouchControls *tc, u32 index);
void touch_controls_draw_jump_button(const TouchControls *tc);

// every widget with the default look
void touch_controls_draw(TouchControls *tc, UISystem *ui);
