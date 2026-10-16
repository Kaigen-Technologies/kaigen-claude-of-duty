// Day/night cycle: a clock, the sun and moon over a latitude, and a palette of moods keyed by the sun's elevation.

// It drives the scene's key light, ambient and fog, and the stylized sky (hz_stylized_sky) or the physical one (hz_atmosphere).

// DayNightCycle goes on the scene's sun, a root EcsDirectionalLight: the block owns its rotation, colour, intensity and casting.

// Setup: day_night_init in app_init; day_night_schedule in the scope before ecs_transform_schedule; day_night_sync after.

// Stylized: the palette is the look (light, sky colours, stars, fog, ambient, exposure).

// Physical: the atmosphere colours the sky and the light; the palette gives fog and exposure.

// The block owns the exposure: the palette's EV100 plus night_exposure (with hz_exposure enabled: its limits).

// The key light turns in light_step_deg steps: each refreshes the cached shadows once; at rest it settles exactly.
#pragma once

#include "assets/assets.h"
#include "ecs/ecs.h"
#include "lib/blob_asset.h"
#include "lib/color.h"
#include "lib/hzmath.h"
#include "lib/reflect.h"
#include "rendering/renderer.h"
#include "rendering/stylized_sky.h"

HZ_REFLECT()
typedef enum {
  DAY_NIGHT_SKY_NONE = 0, // the light, ambient and fog only; the app owns the sky
  DAY_NIGHT_SKY_STYLIZED, // hz_stylized_sky, coloured by the palette
  DAY_NIGHT_SKY_PHYSICAL, // hz_atmosphere
} DayNightSky;

// one mood, held at its sun elevation and blended smoothly to its neighbours; colours sRGB, display values at EV100 15
HZ_STRUCT_TYPEDEF(DayNightMood) {
  f32 sun_elevation;     // degrees
  ColorF32 light_color;  // the key light (the sun, or the moon at night)
  f32 light_intensity;   // stylized: lux
  ColorF32 zenith;       // stylized: the sky
  ColorF32 horizon;
  ColorF32 glow;         // stylized: the halo around the sun
  ColorF32 cloud_lit;    // stylized
  ColorF32 cloud_shade;
  f32 stars;             // 0..1: the stylized star field; physical: x DayNightCycle.physical_stars_luminance
  ColorF32 fog;          // the scene's fog and the stylized sky's haze
  ColorF32 ambient;      // the scene's uniform ambient, on top of the sky's environment
  f32 ambient_intensity; // cd/m2
  f32 ev100;             // the exposure (without auto-exposure) or the middle of its limits
};

// moods in ascending sun elevation; below the first and above the last the nearest holds
HZ_STRUCT_TYPEDEF(DayNightPalette) {
  BlobAssetHeader header;
  BlobArray(DayNightMood) moods;
};

#define DAY_NIGHT_PALETTE_STYLIZED "gameplay_blocks/day_night/stylized.hzt"
#define DAY_NIGHT_PALETTE_PHYSICAL "gameplay_blocks/day_night/physical.hzt"

// on the scene's sun; zero fields marked "0 =" take the default, day_night_cycle_default() is a summer day at 45 N
HZ_REFLECT()
ECS_COMPONENT(DayNightCycle) {
  f32 time_of_day;      // hours [0, 24): the start, then the running clock (solar time: the sun is highest at 12)
  f32 day_length;       // real seconds per 24 hours; 0 = the clock stands
  b32 paused;
  f32 latitude_deg;     // the sun's arc: 0 the equator, 90 the north pole
  f32 day_of_year;      // 0..365, the season; 172 is the northern summer solstice. never advanced by the block
  f32 north_yaw_deg;    // north is world -Z turned this far about +Y; east is +X at 0
  f32 moon_phase;       // 0 new, 0.25 first quarter, 0.5 full: the moon's place and brightness. never advanced
  f32 night_exposure;   // stops brighter at full night than the palette (negative: darker); fades in with dusk
  BlobAssetRef(DayNightPalette) palette; // empty = the module's palette for the sky
  f32 light_step_deg;   // the key light turns once it is this far behind; 0 = 0.25
  b32 no_shadows;       // the key light never casts
  f32 physical_sun_illuminance;  // DAY_NIGHT_SKY_PHYSICAL only: lux; 0 = 120000 (stylized: the palette's light)
  f32 physical_moon_illuminance; // DAY_NIGHT_SKY_PHYSICAL only: lux of the full moon; 0 = 0.25
  f32 physical_stars_luminance;  // DAY_NIGHT_SKY_PHYSICAL only: the brightest star, cd/m2; 0 = 4
  b32 moon_declination_fixed;    // the moon keeps moon_declination_deg instead of riding the ecliptic (its phase still sets its hour)
  f32 moon_declination_deg;
  HZ_RUNTIME u32 day;   // whole days the clock has run
};

DayNightCycle day_night_cycle_default(void);

// what gameplay reads, written once a frame by the block
typedef struct DayNightState {
  b32 ready;            // a DayNightCycle ran with its palette loaded
  f32 time_of_day;      // hours
  u32 day;
  v3 sun_dir;           // toward the sun
  v3 moon_dir;          // toward the moon
  v3 key_dir;           // toward the key light, as the light holds it (stepped, and clamped for the stylized sky)
  b32 key_is_moon;
  f32 key_lux;          // the key light's illuminance as applied
  f32 ev100;            // the exposure as applied (the middle of the auto-exposure limits)
  f32 sun_elevation_deg;
  f32 moon_elevation_deg;
  f32 moon_brightness;  // 0 new .. 1 full
  f32 daylight;         // 0 night .. 1 day: smoothstep(-6, 6, sun elevation)
  f32 night;            // 0 with the sun up .. 1 once it is 12 degrees down (nautical dusk)
  b32 sunrise, sunset;  // the sun crossed the horizon this frame
  quat celestial;       // the star sphere's rotation into the world
  DayNightMood mood;    // the palette at this sun elevation (sRGB)
} DayNightState;
ECS_COMPONENT_TYPEDEF(DayNightState);

// the sun and moon of one instant; pure
typedef struct DayNightBodies {
  v3 sun_dir, moon_dir;
  f32 sun_elevation_deg, moon_elevation_deg;
  f32 moon_brightness; // 0 new .. 1 full: the lunar phase law
  quat celestial;
} DayNightBodies;
DayNightBodies day_night_astronomy(const DayNightCycle *c);

// the palette at a sun elevation: smoothstep between the bracketing moods, blended in linear light; zero moods = zero
DayNightMood day_night_palette_sample(Array(DayNightMood) moods, f32 sun_elevation_deg);

// the key light's handoff where twilight is about as bright as moonlight: the sun fades over [-10, -11], the moon over [-11, -12]
#define DAY_NIGHT_HANDOFF_DEG -11.0f
#define DAY_NIGHT_HANDOFF_WIDTH_DEG 1.0f

// the key light of one instant, before stepping: which body, its weight and the direction toward it
typedef struct DayNightKey {
  b32 is_moon;
  f32 weight;        // 0..1: the handoff, and for the moon its elevation fade (stylized)
  v3 dir;            // toward the body; stylized: held at least 4.6 degrees above the horizon
  f32 elevation_deg; // the body's own elevation
} DayNightKey;
DayNightKey day_night_key(const DayNightBodies *b, b32 stylized);

// the step: the held direction follows `target` once it is past step_deg, or exactly when the target rests (== prev)
v3 day_night_step_light(v3 held, v3 target, v3 prev_target, f32 step_deg);

// main lane, outside the frame, after ecs_renderer_init and ecs_assets_init: DayNightState, the sky's feature, the palette
void day_night_init(EcsWorld *w, RendererState *r, Allocator *a, DayNightSky sky);
// inside the scope, before ecs_transform_schedule: the clock, the sun entity, the scene settings, DayNightState
void day_night_schedule(EcsWorld *w);
// main lane, after the scope: the sky, the stars and the exposure (renderer settings are main-lane); r NULL with DAY_NIGHT_SKY_NONE leaves the exposure alone
void day_night_sync(EcsWorld *w, RendererState *r);
// the stylized sky's base (clouds, wind, haze, disc, exposure, env) the block layers the time of day over; main lane
StylizedSkyParams *day_night_stylized_base(EcsWorld *w);

// the block's own state
typedef struct DayNightRuntime {
  DayNightSky sky;
  HzHandleT(DayNightPalette) palette; // the module's palette for the sky
  StylizedSkyParams base;
  v3 prev_target; // last frame's unstepped key direction
  b32 prev_key_is_moon;
  f32 prev_sun_elevation_deg;
  b32 have_prev;
  f32 stars_luminance; // physical: this frame's, cd/m2
  u32 cycles;          // DayNightCycles seen this frame
  b32 warned_cycles;
} DayNightRuntime;
ECS_COMPONENT_TYPEDEF(DayNightRuntime);
