// Composed effects: N elements (particles, a pooled light, a trail, a beam, a decal) fired as one thing on a stagger.
// vfx_init, vfx_register, then vfx_request from any system and vfx_frame once a frame. Default particle elements share
// one table-driven material per texture, so a hundred effects draw in a handful of calls.
#pragma once

#include "lib/typedefs.h"
#include "lib/hzmath.h"
#include "lib/color.h"
#include "lib/handle.h"
#include "lib/array.h"
#include "ecs/ecs.h"
#include "rendering/particle_system.h"
#include "rendering/trail_system.h"
#include "lib/blob_asset.h"
#include "lib/reflect.h"
#include "lib/string.h"
#include "rendering/material.h"  // TextureBlobAsset
#include "rendering/vfx_asset.h" // VfxBlobAsset, the authored form

typedef struct VfxAsset VfxAsset;
typedef struct VfxLoop VfxLoop;
HANDLE_TYPEDEF(VfxAsset);
HANDLE_TYPEDEF(VfxLoop);

// 32, not 8: an imported pack already holds 10-emitter effects, and truncating drops authored layers silently
#define VFX_ELEM_MAX 32
#define VFX_STOP_MAX 8
#define VFX_DECAL_VARIANTS 4

// one keypoint of a colour ramp; colour and transparency combined
typedef struct VfxStop {
  f32 t;
  ColorF32 color;
} VfxStop;

typedef enum VfxElemKind {
  VFX_ELEM_PARTICLES = 0, // the particle fields of VfxElemDesc
  VFX_ELEM_LIGHT,         // `light`: a pooled point light that fades over its life
  VFX_ELEM_TRAIL,         // `trail`: a strip over TrailSystem, fed by a loop's points
  VFX_ELEM_BEAM,          // `trail`: a segment between two points (a tracer, a rail beam, a tether), played or looped
  VFX_ELEM_DECAL,         // `decal`: a projected mark from a pooled ring of slots
} VfxElemKind;

// how a particle draws (VFX_ELEM_PARTICLES without its own material)
typedef enum VfxRender {
  VFX_RENDER_SPRITE = 0, // the element's texture, else a soft disc
  VFX_RENDER_GLOW,       // no texture: a soft halo around a hot core
  VFX_RENDER_SPARK,      // no texture: a streak; stretch it along travel with velocity_stretch
  VFX_RENDER_RING,       // an expanding band lying on the ground; size_ease 3 and a 0.15 start give a shockwave
  VFX_RENDER_POINT,      // a square of `size` pixels whatever the depth (renderer path only)
} VfxRender;

// what a play's `scale` multiplies; 0 on an element = all of them
#define VFX_SCALE_SPEED 1u  // the speed along the shape (`speed`), not vspeed or scatter
#define VFX_SCALE_SIZE 2u   // the quad's size
#define VFX_SCALE_HEIGHT 4u // the spawn height
#define VFX_SCALE_RADIUS 8u // the spawn shape and the spawn offset

// the sRGB hue of a linear HDR colour, and its peak: color = hue, intensity *= peak
force_inline v3 vfx_light_hue(v3 linear_hdr, f32 *peak) {
  f32 p = m_maxf(linear_hdr.x, m_maxf(linear_hdr.y, linear_hdr.z));
  *peak = p;
  if (p <= 0.0f) return (v3){1, 1, 1};
  return (v3){linear_to_srgb(linear_hdr.x / p), linear_to_srgb(linear_hdr.y / p), linear_to_srgb(linear_hdr.z / p)};
}

typedef struct VfxLightDesc {
  v3 color;      // sRGB, as RendererLocalLightDesc.color (vfx_light_hue folds a linear HDR colour into hue and peak)
  f32 intensity; // at birth, times the world's light unit (vfx_set_light_unit) to give candela
  f32 radius;    // metres, times a play's scale
  f32 life;      // seconds
  f32 curve;     // intensity follows (1 - age / life)^curve; 0 = 2
  v3 offset;     // from the play position
} VfxLightDesc;

typedef struct VfxTrailDesc {
  f32 width;       // metres across at birth, times a play's scale
  v2 width_scale;  // width multiplier at {the start, the end}: of a beam's life, or {head, tail} of a strip; 0 = 1
  ColorF32 color;  // linear HDR; a play's color overrides it. zero = white
  f32 life;        // beam: seconds visible. strip: seconds a point lives, 0 = until cut
  f32 fade;        // beam: alpha follows (1 - age / life)^fade; 0 = opaque until the end
  f32 length;      // beam moving at `speed`: metres of tail behind the head; 0 = the whole way
  f32 speed;       // beam: m/s the head travels from the start to the end; 0 = the whole segment at once
  f32 min_distance; // strip: a point closer than this to the last is dropped; 0 = 0.01
  u32 max_points;  // strip: points kept per strip; 0 = 64
  TrailOrientation orientation; // strip: camera, rotation-minimising or authored (vfx_loop_trail_point's normal)
  b32 fade_over_age; // strip: a point fades out over its life instead of popping
  f32 fade_hold;     // strip: the share of a point's life it stays at full alpha before fading (fade_over_age)
  f32 uv_tile;       // strip: metres along per texture repeat; 0 = the texture spans the strip
  b32 multiply;      // multiply blend (darkens what is under it, alpha fades to white); wins over additive
} VfxTrailDesc;

typedef struct VfxDecalDesc {
  HzHandleT(TextureBlobAsset) albedo[VFX_DECAL_VARIANTS]; // loaded before the play; one is drawn per play
  u32 variants;     // how many of `albedo` are set; 0 = an untextured decal
  v2 size;          // {min, max} metres across, drawn per play, times a play's scale
  f32 depth;        // metres through the surface
  ColorF32 color;   // sRGB tint, the decal api converts
  f32 emission;     // cd/m2 at birth, times the world's light unit; 0 = not emissive
  f32 heat_decay;   // emission and tint alpha follow exp(-heat_decay * age); 0 = constant
  f32 fade_start;   // seconds; the tint alpha goes 1 -> 0 on a smoothstep to fade_end; 0 = never fades
  f32 fade_end;
  b32 roll;         // spin each play by a random angle about the surface normal
  u8 priority;      // higher draws on top
} VfxDecalDesc;

// One element of an effect. Zero means "off" for every field, so a literal only spells out what it uses. `count`
// alone is a burst, `rate` alone is a loop, both together is an impact that leaves something behind.
typedef struct VfxElemDesc {
  VfxElemKind kind; // 0 = particles; the fields from `tex` to `friction` and `render` to `user` belong to it

  GpuTexture tex;
  f32 additive; // 0 alpha-blended .. 1 pure add

  VfxStop color[VFX_STOP_MAX]; // straight rgba, HDR rgb allowed
  u32 stops;
  b32 ramp_tint; // the ramp multiplies the play's colour (a loop's vfx_loop_set_params) instead of being replaced by it

  f32 delay; // seconds after vfx_play, for every kind
  f32 count; // emitted in one go at `delay`
  f32 rate;  // emitted per second, forever, while a loop runs

  v2 life;      // {min, max} seconds
  v2 speed;     // {min, max} m/s along the cone, or along the heading of an arc
  v2 size;      // {begin, end} half-extent metres (pixels for VFX_RENDER_POINT)
  v2 size_max;  // {begin, end} upper bound of a per-particle uniform draw from `size`; 0 -> exact
  b32 size_linked; // begin and end share one draw (a random overall scale) instead of one each
  f32 size_ease; // 0/1 linear, 3 ease-out

  v3 dir;        // cone axis; zero -> +Y. an arc's heading (horizontal); zero -> +Z
  v2 spread_deg; // half-angle per axis; 180 = full ball. an arc: the first is the half-angle, 180 = ring
  v3 gravity;    // m/s^2
  f32 drag;      // per-frame-at-30fps decay; 0 -> none
  f32 radius;    // spawn disc at the cone mouth

  v3 plane_right, plane_up; // lock the quad to a world plane; zero -> camera
  f32 velocity_align;       // pull the quad's up toward travel
  v2 spin;                  // {min, max} rad/s
  v2 rotation;   // {min, max} rad the quad is STAMPED at on spawn
  f32 z_offset;  // metres toward (+) / away from (-) the camera
  u32 shape;     // ParticleShapeType; 0 -> cone, which carries dir + spread. PARTICLE_SHAPE_ARC is the ring and the aimed spray

  u32 pool; // particles, beam segments or decal slots held for this element; 0 = derived from count and rate

  f32 emissive;         // HDR multiplier on the colour; bloom picks up past 1
  f32 velocity_stretch; // sparks: stretch along screen-space travel by 1 + speed * this
  f32 soft_fade;        // metres over which the quad fades into opaque geometry it crosses (smoke); 0 = hard edge
  b32 collide;          // bounce on the floor published with particle_system_set_floor (sparks, debris)
  f32 restitution;      // the normal speed kept per bounce
  f32 friction;         // the share of the tangential speed lost per contact, 0..1

  // drawn with instead of a built default; none of the material-side fields above are written to it
  HzHandleT(ParticleMaterial) material;
  b32 fixed; // the count is placed exactly, not thinned by the quality budget (the element's own material reads particle_system_budget)

  VfxRender render;
  f32 ring_width;    // VFX_RENDER_RING: the band's width as a share of its radius; 0 = 0.14
  f32 lit;           // 0..1: shade by the world's atmosphere (vfx_set_atmosphere): sun, ambient and height fog
  f32 damping;       // exponential velocity decay per second, on top of drag
  v2 vspeed;         // {min, max} m/s along world up, on top of `speed`
  v2 height;         // {min, max} spawn height above the play position, metres
  f32 back;          // an arc spawns this far behind the play position along its heading
  f32 scatter;       // +/- m/s of random horizontal speed
  b32 uniform_angle; // an arc spaces one burst's particles evenly across it instead of at random
  u32 scale_mask;    // VFX_SCALE_*: what a play's scale multiplies; 0 = all of them
  u32 user;          // an element with its own material: read as ed.user by its simulate

  VfxLightDesc light; // VFX_ELEM_LIGHT
  VfxTrailDesc trail; // VFX_ELEM_TRAIL and VFX_ELEM_BEAM
  VfxDecalDesc decal; // VFX_ELEM_DECAL
} VfxElemDesc;

typedef struct VfxDesc {
  const char *name;
  VfxElemDesc elem[VFX_ELEM_MAX];
  u32 count;
  u32 loops; // concurrent loops of this effect its pools are sized for; 0 = 1
} VfxDesc;

typedef struct AssetSystem AssetSystem;
typedef struct VfxWorld VfxWorld;
typedef struct RendererState RendererState;

typedef struct VfxWorldDesc {
  ParticleSystem *particles; // required: the system the effects' particles live in
  RendererState *renderer;   // pooled lights and decals; NULL = those elements log an error and do nothing
  TrailSystem *trails;       // trail and beam elements, likewise. the game still calls trail_system_update/prepare
  u32 max_loops;             // concurrently running loops; 0 = 32
  u32 max_requests;          // plays buffered per frame, and delayed elements waiting; 0 = 4096
  u32 max_lights;            // pooled transient lights; 0 = 16
  u32 particles_per_material; // pool of each default material (one per texture, another when one fills); 0 = 8192
} VfxWorldDesc;

// the world without ECS (tests, tools): the VfxRequests resource and vfx_frame are the ECS form
VfxWorld *vfx_world_create(const VfxWorldDesc *desc);
void vfx_world_destroy(VfxWorld *w);

// One play of an asset. `dir`, `yaw` and the scales turn and size every element the same.
typedef struct VfxPlayDesc {
  v3 at;
  v3 dir;          // where the effect's authored +Y points; zero = as authored. a decal's surface normal
  f32 yaw;         // radians about +Y on top of dir: the heading of an arc
  f32 scale;       // spawn offsets, speeds, size, light radius, decal size, beam width; 0 = 1
  f32 count_scale; // thins (or thickens) every burst; 0 = 1
  ColorF32 color;  // spawn colour for elements without a ramp, a beam's colour; zero = authored
  v4 user;         // a custom material's per-burst payload (ed.burst_user)
  f32 speed_scale; // each on top of `scale`, for its own thing; 0 = 1
  f32 size_scale;
  f32 height_scale;
  f32 radius_scale;
  f32 intensity_scale; // lights and decal emission; 0 = 1
  v3 to;           // a beam's far end
} VfxPlayDesc;

// a play waiting for the main lane
typedef struct VfxRequest {
  HzHandleT(VfxAsset) asset;
  VfxPlayDesc play;
} VfxRequest;
ARRAY_TYPEDEF_ALL(VfxRequest);

// the buffer plays append to from any lane; vfx_frame drains it on the main lane
typedef struct VfxRequestQueue {
  ConcurrentArray(VfxRequest) q;
  i32 dropped_atomic; // requests past the capacity this frame
  VfxWorld *world;
} VfxRequestQueue;

// The ECS resource vfx_init sets: a system that wants an effect declares EcsResIn(VfxRequests) and calls vfx_request.
typedef struct VfxRequests {
  VfxRequestQueue *queue;
} VfxRequests;
ECS_COMPONENT_TYPEDEF(VfxRequests);

// ECS form of vfx_world_create: main lane, once. Sets the VfxRequests resource.
void vfx_init(EcsWorld *w, const VfxWorldDesc *desc);
VfxWorld *vfx_world(EcsWorld *w);

// Any lane, any number of times a frame, from a system or plain code. false when the buffer was full.
b32 vfx_request(const VfxRequests *rq, HzHandleT(VfxAsset) asset, const VfxPlayDesc *play);

// Main lane, once a frame after the flush: drains the buffer, feeds loops, ages lights, beams, decals and strips. Call
// particle_system_update after it. (Not an ecs_system: trails, lights, decals and particle bursts are main-lane state.)
void vfx_frame(EcsWorld *w, const Camera *camera, f32 dt);
void vfx_world_frame(VfxWorld *w, const Camera *camera, f32 dt);

// Builds the effect: an emitter and a table row per particle element, the pooled lights, segments and decals the
// rest need. ONCE PER EFFECT, at start-up, never per play: the pools are sized for it and stay reserved until
// vfx_unregister. Main lane or one caller.
HzHandleT(VfxAsset) vfx_register(VfxWorld *w, const VfxDesc *desc);
// Gives the effect's emitters, trails and decals back; their pool ranges return once their particles are gone.
void vfx_unregister(VfxWorld *w, HzHandleT(VfxAsset) asset);

// Same, from a cooked asset: resolves each element's texture reference to a live GpuTexture and clamps to
// VFX_ELEM_MAX / VFX_STOP_MAX.
HzHandleT(VfxAsset) vfx_register_from_blob(VfxWorld *w, const AssetSystem *assets, VfxBlobAsset *blob);

// Plays now, main lane: every element on its own delay. Systems use vfx_request instead.
void vfx_play(VfxWorld *w, HzHandleT(VfxAsset) asset, v3 at);
void vfx_play_ex(VfxWorld *w, HzHandleT(VfxAsset) asset, const VfxPlayDesc *play);

// The light and decal-emission unit: candela per unit of VfxLightDesc.intensity and cd/m2 per unit of
// VfxDecalDesc.emission. A game with its own exposure convention passes it once; default 1.
void vfx_set_light_unit(VfxWorld *w, f32 candela_per_unit);

// What lit particles are shaded by, scene-linear in the caller's exposure units: the sun scattered forward, sky
// ambient and exponential height fog with a fog colour that warms toward the sun.
typedef struct VfxAtmosphere {
  v3 sun_dir;        // toward the sun
  v3 sun_color;
  v3 ambient;
  v3 fog_color;
  v3 fog_sun_color;  // added to the fog colour looking toward the sun
  f32 fog_density;   // at y = 0
  f32 fog_height_falloff;
} VfxAtmosphere;
void vfx_set_atmosphere(VfxWorld *w, const VfxAtmosphere *atmosphere);

// A looping instance: elements with a `rate` emit, beams and strips draw between its points. Loops share the asset's
// emitters, so starting and stopping one costs no pool. The setters only write the loop, so any lane may call them
// for its own loop; start and stop are one caller at a time, any lane (the next vfx_frame creates and releases the strips).
HzHandleT(VfxLoop) vfx_loop_start(VfxWorld *w, HzHandleT(VfxAsset) asset, v3 at);
void vfx_loop_move(VfxWorld *w, HzHandleT(VfxLoop) loop, v3 at);
// scales every rate: 0 pauses the loop, 1 is as authored. the carry of a fractional particle is kept
void vfx_loop_set_rate(VfxWorld *w, HzHandleT(VfxLoop) loop, f32 scale);
// the dir, yaw, scales, color and user of what the loop emits from now on (a VfxPlayDesc's; `at`, `to` ignored)
void vfx_loop_set_params(VfxWorld *w, HzHandleT(VfxLoop) loop, const VfxPlayDesc *params);
// a beam element's far end; the near end is the loop's position
void vfx_loop_set_target(VfxWorld *w, HzHandleT(VfxLoop) loop, v3 to);
// a trail element's next point: the position, the surface normal for TRAIL_ORIENT_AUTHORED, an alpha multiplier.
// up to 8 a frame per loop
void vfx_loop_trail_point(VfxWorld *w, HzHandleT(VfxLoop) loop, v3 at, v3 normal, f32 alpha);
// drops every point the loop's strips hold and the points queued for them; main lane
void vfx_loop_trail_clear(VfxWorld *w, HzHandleT(VfxLoop) loop);
// the next trail point starts a new stroke
void vfx_loop_trail_cut(VfxWorld *w, HzHandleT(VfxLoop) loop);
void vfx_loop_stop(VfxWorld *w, HzHandleT(VfxLoop) loop);
u32 vfx_loop_count(const VfxWorld *w);

// A camera-following volume of ambient motes (pollen by day, fireflies by night) in a box wrapped around the camera,
// above the particle floor. One material and one draw, simulated on the GPU; the quality budget thins it.
typedef struct VfxAmbient {
  u32 slot; // 0 = none
} VfxAmbient;

typedef struct VfxAmbientDesc {
  u32 count;           // motes at full quality; 0 = 900
  f32 box;             // metres on a side; 0 = 70
  f32 height_day;      // motes rise this far above the floor by day; 0 = 5
  f32 height_night;    // and by night; 0 = 2.5
  f32 sway;            // metres of horizontal sway; 0 = 0.8
  f32 size_px;         // pixels at one metre away, shrinking with distance; 0 = 70
  f32 size_px_max;     // largest, pixels; 0 = 24
  v3 day_color;        // HDR, before the sun colour and glint
  v3 night_color;      // HDR, blinking
  f32 glint_base;      // day: the share of day_color a mote shows away from the sun
  f32 glint_gain;      // and what facing the sun adds
  f32 glint_exponent;  // how tightly; 0 = 5
} VfxAmbientDesc;

// the per-frame look of an ambient volume
typedef struct VfxAmbientLook {
  f32 time;   // seconds, any monotonic clock
  f32 night;  // 0 day .. 1 night
  f32 unit;   // exposure multiplier on the colours; 0 = 1
  v3 sun_dir; // toward the sun
  v3 sun_color;
  v2 wind;    // direction in xz
  f32 wind_strength;
} VfxAmbientLook;

VfxAmbient vfx_ambient_create(VfxWorld *w, const VfxAmbientDesc *desc);
void vfx_ambient_set(VfxWorld *w, VfxAmbient a, const VfxAmbientLook *look);
void vfx_ambient_destroy(VfxWorld *w, VfxAmbient a);

// Jitter a desc so repeated plays are not identical; past ~0.25 the elements drift apart and stop reading as one
// event.
void vfx_desc_vary(VfxDesc *desc, u64 seed, f32 amount);
