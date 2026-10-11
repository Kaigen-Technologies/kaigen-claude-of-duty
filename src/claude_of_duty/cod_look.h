// Claude of Duty look: time of day, the engine sky driving sun and moon, fog, exposure, grade and quality presets.
#pragma once
#include "ecs/ecs.h"
#include "lib/color.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "rendering/renderer.h"
#include "claude_of_duty.h"

#define COD_LOOK_ROOMS 10
#define COD_LOOK_VIEW_FLASHES 2

// the sky and light state other packages read
typedef struct CodLook {
  f32 time_of_day;      // hours 0..24
  v3 sun_dir;           // pointing at the sun
  v3 moon_dir;          // pointing at the moon
  f32 sun_altitude;     // radians above the horizon
  v3 key_dir;           // whichever of sun or moon casts the shadows, pointing at it
  b32 key_is_moon;
  ColorF32 key_color;   // linear, the key light's colour times intensity, lux
  ColorF32 ambient;     // linear whole-sky irradiance model, lux
  f32 night;            // 0 day .. 1 full night (practicals and the flashlight fade in)
  f32 exposure_bias;    // EV, + darker
  f32 indirect_scale;   // the indirect budget for this sun elevation
  f32 scene_per_unit;   // one scene radiance unit in cd/m2: emissive effects scale by it
  u32 quality;          // CodQuality in effect
  f32 view_key;         // the viewmodel rig's key, scene units (the view flash peaks at a ratio of it)
} CodLook;
ECS_COMPONENT_TYPEDEF(CodLook);

// the _CodLook block every COD surface shader applies (shaders/claude_of_duty/cod_look.glsl), vec4s only
typedef struct CodLookBlock {
  v4 key_gain;    // xyz the scene key over the engine's physical sun; w AO-on-direct fraction
  v4 sky_fill;    // xyz pre-exposed irradiance; w hemispheric gain
  v4 ground_fill; // xyz pre-exposed irradiance; w sun-bounce wrap gain
  v4 fill_dir;    // sky gate over N.y (x, y), ground gate over -N.y (z, w)
  v4 indirect;    // x diffuse ambient budget, y interior floor, z room count, w AO visibility floor
  v4 room_xf;     // world -> level 2D: cos, sin, tx, tz
  v4 sun_dir;     // xyz toward the sun, w night
  v4 rooms[COD_LOOK_ROOMS];   // level space cx, cz, hx, hz
  v4 rooms_y[COD_LOOK_ROOMS]; // y0, y1
  v4 view_key;    // the viewmodel rig, view-space directions fixed in the shader: xyz lux (pre-exposed); w fill occlusion
  v4 view_fill;   // w environment intensity (replaces the material's)
  v4 view_rim;
  v4 view_bounce;
  v4 view_hemi_sky;
  v4 view_hemi_ground;
  v4 view_flash[COD_LOOK_VIEW_FLASHES];       // xyz world, w cutoff metres
  v4 view_flash_color[COD_LOOK_VIEW_FLASHES]; // xyz candela (pre-exposed)
} CodLookBlock;

// the _CodSky block the COD sky material reads (shaders/claude_of_duty/cod_sky_block.glsl), vec4s only
typedef struct CodSkyBlock {
  v4 sun;             // xyz toward the sun, w the roll-off knee, cd/m2
  v4 sun_irradiance;  // xyz extraterrestrial irradiance x tint, lux; w the roll-off power
  v4 moon;            // xyz toward the moon, w horizon murk
  v4 moon_irradiance; // xyz lux; w the viewer's altitude, km
  v4 clouds;          // x cumulus coverage, y density, z quality, w seconds
  v4 clouds2;         // x cirrus coverage, y cirrus opacity, zw wind km/s
} CodSkyBlock;

// main lane, app_init: fills the renderer desc from the quality preset (shadows, TAA, modules)
void cod_look_renderer_desc(RendererInitDesc *desc);
// main lane, app_init: the sky, fog, exposure and grade modules
void cod_look_init(EcsWorld *w, Allocator *alloc);
// inside the scope after the camera: time of day, celestial state, post; publishes CodLook
void cod_look_schedule(EcsWorld *w);
// main lane after the scope: lights, fog, exposure, quality and the look block into the renderer
void cod_look_sync(EcsWorld *w);
