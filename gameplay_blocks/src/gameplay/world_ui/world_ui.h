// world UI: world-anchored screen widgets (nametags, popups, prompts, pins) driven by ECS systems, plus the
// UISurface-based BillboardGui / SurfaceGui primitives that put a UI in the 3D scene.
//
// Widgets (Block 2) are entities: world_ui_init once, world_ui_schedule(w) inside the frame's ecs_scope after
// ecs_camera_schedule (and ecs_physics_schedule when occluding), world_ui_layout on the main lane inside the UI
// frame. The surface blocks (1, 3) are plain main-lane functions: every ui_surface_* call asserts single-thread,
// so they run end-of-frame BEFORE renderer_update, OUTSIDE ui_begin_frame/ui_finish_layout (a world surface drawn
// inside that window renders nothing, silently), as ui_surface_update_all(ui, &cam) then the ui_surface_scope draws.
#pragma once

#include "ecs/camera/ecs_camera.h"
#include "ecs/ecs.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "ecs/transform/ecs_transform.h"
#include "input.h"
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/string.h"
#include "rendering/camera.h"
#include "rendering/renderer.h"
#include "ui/ui.h"


// ---- Block 1: BillboardGui (primitive) ----
// billboard_create(ui, r, &desc); then per frame:
//   if (billboard_place(&bg, &cam, screen_h, anchor)) ui_surface_scope(ui, bg.surface) { ... }

typedef enum {
  BILLBOARD_SIZE_STUDS = 0,  // world-sized: shrinks with distance
  BILLBOARD_SIZE_PIXELS = 1, // screen-constant: same on-screen height at any distance
} BillboardSizeMode;

typedef struct BillboardGuiDesc {
  u32 canvas_w, canvas_h;  // RT pixel space
  f32 pixels_per_unit;     // STUDS mode: world size = canvas/ppu; also texel density
  BillboardSizeMode size_mode;
  f32 pixel_height;        // PIXELS mode: target on-screen height in device px
  v3 studs_offset_world;   // world-space offset: world-axis add to anchor
  v3 studs_offset_camera;  // camera-space offset: camera-relative (right/up/fwd) add
  f32 max_distance;        // max distance; <=0 = never cull by distance
  b32 always_on_top;       // always on top -> depth_test off
  ColorF32 clear_color;
} BillboardGuiDesc;

typedef struct BillboardGui {
  UISurface *surface;
  BillboardSizeMode size_mode;
  f32 pixel_height;
  f32 max_distance;
  v3 off_world;
  v3 off_camera;
  b32 visible;
} BillboardGui;

BillboardGui billboard_create(UISystem *ui, RendererState *r, const BillboardGuiDesc *desc);
void billboard_destroy(UISystem *ui, RendererState *r, BillboardGui *bg);

// places + sizes + distance-culls one billboard at `anchor`; returns false if
// culled. call BEFORE ui_surface_update_all — that is what applies the
// camera-facing rotation, so without it the billboard keeps its last one.
b32 billboard_place(BillboardGui *bg, const Camera *cam, u32 screen_h_px, v3 anchor);

// ---- Block 2: WorldUi (world-anchored screen-space widgets) ----
// Nametags, damage/value numbers, proximity prompts and hotspot pins are one component set: WorldUi places the
// widget, WorldUiLabel is its content. A systems pass projects every anchor to the screen, culls by distance,
// ray-tests occlusion against the occluder layer, pins to the edge, stacks overlapping widgets and picks the
// nearest in-range prompt; world_ui_layout draws what the style asks for. A game that draws a kind itself (a
// hotspot pin with its own look) leaves it out of WorldUiStyle.kinds and reads WorldUiScreen.

HZ_REFLECT()
typedef enum {
  WORLD_UI_NAMETAG = 0, // name above a head, optional fill bar (health)
  WORLD_UI_POPUP,       // a number or word that rises and fades (WorldUiPopup)
  WORLD_UI_PROMPT,      // "[key] action", shown for the nearest in-range prompt
  WORLD_UI_PIN,         // a dot with a short label
  WORLD_UI_KIND_COUNT,
} WorldUiKind;

HZ_REFLECT()
ECS_COMPONENT(WorldUi) {
  WorldUiKind kind;
  v3 offset;           // world offset from the entity's WorldTransform position (the anchor itself on an entity without one)
  b32 screen_anchored; // no world anchor: position = canvas * screen_frac + screen_px
  v2 screen_frac;
  v2 screen_px;
  f32 max_distance;    // <=0: never culled by distance
  b32 occlude;         // hidden while the occluder layer blocks the ray from the camera
  b32 clamp_edge;      // off screen or behind the camera: pinned to the screen edge instead of hidden
  f32 edge_margin;     // px kept between a pinned widget's anchor and the edge
  f32 fade_s;          // alpha ease time in seconds; 0 = instant
  u32 stack_group;     // 0 = never stacked; widgets of one group push each other up when their boxes overlap
  v2 stack_size;       // px box the overlap test uses, centred on x with its bottom at the anchor
  f32 stack_relax;     // px/s a pushed widget settles back toward its anchor; 0 = stays where it was pushed
};

HZ_REFLECT()
typedef enum {
  WORLD_UI_FILL_NONE = 0,
  WORLD_UI_FILL_ALWAYS,
  WORLD_UI_FILL_ON_HIT, // shown for fill_hold seconds after the fill last dropped
} WorldUiFillMode;

HZ_REFLECT()
ECS_COMPONENT(WorldUiLabel) {
  String64Bytes text;
  String64Bytes sub;  // second line; empty = none
  ColorF32 color;     // sRGB text colour; alpha ignored
  f32 fill;           // 0..1 bar under the text (health)
  WorldUiFillMode fill_mode;
  f32 fill_hold;      // ON_HIT: seconds the bar stays after a drop
  f32 fill_age;       // runtime: seconds since the fill last dropped
  f32 fill_prev;      // runtime
};

// a prompt entity also carries WorldUi{.kind = WORLD_UI_PROMPT} and a WorldUiLabel (text = the action, sub = a second run);
// a screen-anchored prompt ignores range: it is active while enabled
HZ_REFLECT()
ECS_COMPONENT(WorldUiPrompt) {
  App_InputButtonType key; // KEY_MAX = none: no key cap, never triggers (a touch button does the action)
  f32 range;    // ground-plane metres from the WorldUiInteractor; <=0 -> 10
  b32 enabled;
};

// tags the entity range is measured from (the player); needs a WorldTransform
ECS_TAG(WorldUiInteractor);

// runtime, written by the systems; pos is the anchor in logical px before stacking
ECS_COMPONENT_TYPEDEF(WorldUiScreen);
typedef struct WorldUiScreen {
  v2 pos;
  f32 distance;   // metres from the camera
  f32 angle;      // radians of the direction from the screen centre to the anchor; the arrow of a pinned widget
  f32 alpha;      // eased visibility 0..1
  f32 stack_dy;   // px added to pos.y by stacking (<= 0)
  b32 in_front;
  b32 on_screen;
  b32 clamped;    // pinned to the edge this frame
  b32 occluded;
  b32 visible;    // target visibility before easing: in front, in range, not occluded, prompt active
  b32 occ_checked;
  b32 active;     // prompt: the nearest enabled one in range
  b32 triggered;  // prompt: active and its key went down this frame
} WorldUiScreen;

// a popup's lifetime and motion; spawn through world_ui_popup
ECS_COMPONENT_TYPEDEF(WorldUiPopup);
typedef struct WorldUiPopup {
  f32 age;
  f32 life;
  f32 rise_world;    // metres the anchor rises over the life, ease-out
  v2 drift_px;       // px offset reached at the end of the life
  v2 enter_px;       // px offset the enter phase starts from
  f32 enter_frac;    // fraction of the life the enter phase (alpha 0 -> 1, scale_from -> 1, enter_px -> 0) takes
  f32 fade_from;     // fraction of the life where alpha starts falling to 0
  f32 scale_from;
} WorldUiPopup;

typedef struct WorldUiPopupDesc {
  v3 at;                  // world position (ignored when screen_anchored)
  b32 screen_anchored;
  v2 screen_frac;
  v2 screen_px;
  String64Bytes text;
  ColorF32 color;
  f32 life;               // <=0 -> 1.3
  f32 rise_world;         // metres, ease-out; 0 = none
  v2 drift_px;
  v2 enter_px;
  f32 enter_frac;         // 0 = no enter phase
  f32 fade_from;          // <=0 -> 0.545 (alpha (1 - t) * 2.2 over the last part of the life)
  f32 scale_from;         // <=0 -> 1
  f32 scatter;            // metres of deterministic jitter around `at`
  u32 stack_group;
  v2 stack_size;
} WorldUiPopupDesc;

typedef struct WorldUiDesc {
  b32 occlusion;                                  // ray-test occluders; needs ecs_physics_init
  HzHandleT(CollisionLayerHandle) occluder_layer; // invalid -> the static layer
  u32 occlusion_every;                            // frames between one widget's ray tests, staggered by entity; 0 -> 3
  f32 occlusion_margin;                           // the ray stops this short of the anchor, metres; <=0 -> 0.02
} WorldUiDesc;

// the frame's camera, published by the view system; screen is the UI's logical px
ECS_COMPONENT_TYPEDEF(WorldUiView);
typedef struct WorldUiView {
  mat4 view_proj;
  v3 cam_pos;
  v2 screen;
  CollisionLayer *occluders; // NULL when the layer is empty or occlusion is off
  u32 frame;
  b32 valid;
} WorldUiView;

ECS_COMPONENT_TYPEDEF(WorldUiSettings);
typedef struct WorldUiSettings {
  WorldUiDesc desc;
} WorldUiSettings;

// once, main lane, after ecs_physics_init when occluding
void world_ui_init(EcsWorld *w, const WorldUiDesc *desc);
// inside the frame's ecs_scope after ecs_camera_schedule (and ecs_physics_schedule when occluding)
void world_ui_schedule(EcsWorld *w);

// adds WorldUi + WorldUiLabel to an entity; WorldUiScreen follows one flush later
void world_ui_add(EcsWorld *w, EcsEntity e, const WorldUi *ui, const WorldUiLabel *label);
// spawns a popup entity that destroys itself at the end of its life; callable from a system on any lane
EcsEntity world_ui_popup(EcsWorld *w, const WorldUiPopupDesc *desc);
// the screen state of a widget entity, NULL before its first projection
const WorldUiScreen *world_ui_screen(EcsWorld *w, EcsEntity e);

typedef struct WorldUiNametagStyle {
  HzUIFloatingAttachPointType attach; // which point of the widget sits on the anchor
  HzUITextConfig name;
  HzUITextConfig sub;
  v2 bar_size; // px; zero = no bar
  HzUIColor bar_back;
  HzUIColor bar_fill;
  f32 gap;     // px between lines
} WorldUiNametagStyle;

typedef struct WorldUiPopupStyle {
  HzUIFloatingAttachPointType attach;
  HzUITextConfig text; // textColor is replaced by the popup's colour
  f32 outline_frac;    // outline width as a fraction of the font size; 0 = none
  HzUIColor outline;
} WorldUiPopupStyle;

typedef struct WorldUiPromptStyle {
  HzUIFloatingAttachPointType attach;
  HzUITextConfig key;
  HzUITextConfig text;
  HzUITextConfig emph; // label.sub as a second run on the same line
  v2 key_size;         // px of the key cap
  HzUIColor key_ring;
  HzUIColor background;
  HzUIColor border;
  f32 corner_radius;
  v2 padding;          // x, y px
  f32 gap;
} WorldUiPromptStyle;

typedef struct WorldUiPinStyle {
  HzUIFloatingAttachPointType attach;
  f32 diameter;
  HzUIColor background;
  HzUITextConfig text;
} WorldUiPinStyle;

typedef struct WorldUiStyle {
  u32 kinds; // bit (1 << WorldUiKind) per kind this call draws
  i16 z_index;
  WorldUiNametagStyle nametag;
  WorldUiPopupStyle popup;
  WorldUiPromptStyle prompt;
  WorldUiPinStyle pin;
} WorldUiStyle;

// main lane, inside the UI frame (ui_begin_frame .. ui_finish_layout), after the scope flushed
void world_ui_layout(EcsWorld *w, UISystem *ui, const WorldUiStyle *style);


// ---- Block 3: SurfaceGui ----
// surface_gui_add(w, e, &(SurfaceGuiDesc){.face = FACE_FRONT, .pixels_per_stud = 40});
// ui_surface_scope(ui, surface_gui_get_surface(w, e)) { ui_text(...); }

// Face convention: part-local normals are Top=+Y, Bottom=-Y, Front=+Z,
// Back=-Z, Left=-X, Right=+X. Front/Back are the OPPOSITE of NormalId
HZ_REFLECT()
typedef enum {
  FACE_TOP = 0,
  FACE_BOTTOM,
  FACE_FRONT,
  FACE_BACK,
  FACE_LEFT,
  FACE_RIGHT,
} SurfaceFace;

HZ_REFLECT()
ECS_COMPONENT(SurfaceGui) {
  SurfaceFace face;
  f32 pixels_per_stud;
  f32 face_w_studs, face_h_studs; // the part face extent -> canvas dims
  b32 always_on_top;
  // panel background behind content; alpha 0 = fully transparent. absent in
  // old .hzt content, so it defaults to {0,0,0,0}
  ColorF32 clear_color;
};

typedef struct SurfaceGuiDesc {
  SurfaceFace face;
  f32 pixels_per_stud;
  f32 face_w_studs, face_h_studs;
  b32 always_on_top;
  ColorF32 clear_color;
} SurfaceGuiDesc;

// part-local rotation for a face, composed with the adornee rotation in
// surface_gui_update. exported only so the face-convention test can probe it.
quat surface_gui_face_rotation(SurfaceFace face);

void surface_gui_init(EcsWorld *w, UISystem *ui, RendererState *r);
// entity must carry WorldTransform (the adornee the panel follows/rides).
void surface_gui_add(EcsWorld *w, EcsEntity e, const SurfaceGuiDesc *desc);
void surface_gui_destroy(EcsWorld *w, EcsEntity e);
// creates UISurfaces for scene-loaded SurfaceGui components that never went
// through surface_gui_add. Call once after the scene spawns; re-entrant.
void surface_gui_adopt(EcsWorld *w);
// per-frame, before content is drawn: follows the adornee's WorldTransform.
void surface_gui_update(EcsWorld *w, const Camera *cam);
// wrap content draws in ui_surface_scope on this surface, AFTER surface_gui_update.
UISurface *surface_gui_get_surface(EcsWorld *w, EcsEntity e);

// ---- Block 3b: SurfaceGuiText (TextLabel content for a SurfaceGui) ----
// Add to the SAME entity as a SurfaceGui; face/size come from it. Font is not
// authored per-label — always the module's default font.
HZ_REFLECT()
ECS_COMPONENT(SurfaceGuiText) {
  String64Bytes text;   // label text
  ColorF32 text_color;  // label textColor3, alpha ignored (opaque)
  b32 text_scaled;      // label textScaled
};

// draws each SurfaceGuiText centered on its panel. call AFTER
// surface_gui_update + ui_surface_update_all. no-op without a SurfaceGuiText.
void surface_gui_text_draw(EcsWorld *w);
