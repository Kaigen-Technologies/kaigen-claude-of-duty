---
title: Day/Night Cycle
description: day_night block — a clock, the astronomical sun and moon (latitude, season, north), a palette of moods keyed by sun elevation, the key light with a sun/moon handoff and stepped turning, driving the stylized sky or the physical atmosphere; DayNightCycle, DayNightState, DayNightPalette
resource: src/gameplay/day_night/day_night.h
tags: [gameplay, sky, lighting, day-night, atmosphere]
updated: 2026-10-06
---

# What it provides

- `DayNightCycle` (reflected, scene-authored) on the scene's sun, a ROOT `EcsDirectionalLight`: `time_of_day`
  (solar hours), `day_length` (real seconds per day, 0 stands), `paused`, `latitude_deg`, `day_of_year`,
  `north_yaw_deg`, `moon_phase`, `night_exposure`, `palette`, `light_step_deg`, `no_shadows`, and
  `physical_sun_illuminance` / `physical_moon_illuminance` / `physical_stars_luminance`, which only the physical sky
  reads. Fields documented "0 =" take the default, so a component added in the editor works.
- What each field visibly does: `time_of_day` everything; `latitude_deg` / `day_of_year` the sun's height at a given
  hour (and so the mood), `north_yaw_deg` the compass direction of sun, moon and shadows; `moon_phase` where the moon
  is and how bright its light is (new moon: no key light, ambient only); `night_exposure` how bright night reads;
  `day_length` / `paused` the clock; `light_step_deg` only how often shadows refresh. Below -14 deg sun elevation
  the stylized palette holds one mood, so at night only the moon (phase, position) and `night_exposure` change.
- The block OWNS the sun entity's `WorldTransform`/`LocalTransform` rotation and the light's colour, intensity and
  `cast_shadow`. Edit `time_of_day`, never the rotation. The scene's `EcsSceneSettings` ambient and fog colour are
  also written every change.
- `DayNightState` resource for gameplay: time, day count, sun/moon/key directions and elevations, `daylight`,
  `night`, the one-frame `sunrise`/`sunset` edges, the celestial rotation and the blended `mood`.
- `DayNightPalette` asset (`.hzt`): moods keyed by sun elevation, smoothstep-blended in linear light. Module
  defaults: `gameplay_blocks/day_night/stylized.hzt` (Wanderer's six moods) and `.../physical.hzt` (fog + an EV100
  curve measured from the atmosphere, see below).
- Pure functions for tools: `day_night_astronomy`, `day_night_palette_sample`, `day_night_key`,
  `day_night_step_light`.

| | `DAY_NIGHT_SKY_STYLIZED` | `DAY_NIGHT_SKY_PHYSICAL` |
|---|---|---|
| Sky | `hz_stylized_sky`: app base params + palette colours, disc = key body, stars rotate | `hz_atmosphere` lit by the key body |
| Key light | palette colour x intensity; held >= 4.6 deg above the horizon | 120 klx sun / 0.25 lux x phase law moon, tinted by the atmosphere; never clamped |
| Ambient | palette ambient on top of the sky's cube | the atmosphere's SH (palette ambient 0) |
| Exposure | palette EV100 (15 by day, 13 at night) - `night_exposure` x night | palette EV100 (measured curve) - `night_exposure` x night |

Either way, with `hz_exposure` enabled the block sets its limits (EV +- 1.5) instead of the EV.

`DAY_NIGHT_SKY_NONE` drives the light, ambient and fog only.

# Example usage

```c
// app_init, main lane, after ecs_assets_init / ecs_renderer_init
day_night_init(w, renderer, &s->allocator, DAY_NIGHT_SKY_STYLIZED);
day_night_stylized_base(w)->haze = 0.6f;          // clouds, wind, haze, disc: the app's base

// in the scope, before ecs_transform_schedule
day_night_schedule(w);
// main lane, after the scope
day_night_sync(w, renderer);

// gameplay
const DayNightState *dn = ecs_resource_get(w, DayNightState);
if (dn->sunset) light_the_lamps();
```

Scripted runs pin the time: `--commands "1:set:DayNightCycle.paused=1;1:set:DayNightCycle.time_of_day=20.4"`.
The starting template is the worked example.

# Design

- **Astronomy**: sun on the ecliptic from `day_of_year` (declination, right ascension), hour angle from solar time,
  latitude tilts the pole, `north_yaw_deg` turns north (-Z at 0, east +X). The moon sits `moon_phase` x 360 deg east
  of the sun on the ecliptic (its 5 deg inclination left out); its brightness follows Allen's phase law (a quarter
  moon is ~9 % of full). Stars use the same celestial rotation, so they turn about the pole.
- **Handoff** at -11 deg sun elevation, where twilight is about as bright as moonlight: the sun's weight fades over
  [-10, -11], the moon's in over [-11, -12]; the key body, sky disc and halo switch while both are near zero.
- **Light stepping**: the light turns once the target is `light_step_deg` (default 0.25) away, so a moving sun costs
  one shadow-cache refresh per step; a resting target (paused clock) settles exactly. Sky, stars and astronomy stay
  continuous.
- `cast_shadow` is off while the key body is > 2 deg below the horizon or its light is 0 (no shadow pass at a
  moonless night or during blue hour in physical mode).
- Physical EV100 curve: metered with `hz_exposure` at 45 N in June (noon 15.9, sunset 10.9, -6 deg 0.5, full-moon
  night -5.8), then biased darker through twilight and night (+2.5 stops at night) so night reads as night.

# Gotchas

- A root's `LocalTransform` is never propagated (wiki transform): the block writes `WorldTransform` itself; the sun
  must be a root (asserted).
- One `DayNightCycle` drives the world; extras log a warning once and are ignored.
- `day_of_year` and `moon_phase` are never advanced by the block: seasons and lunar months are the game's choice.
- Saving the scene while the clock runs stores the current light rotation, colour, ambient and fog: the block
  overwrites them again next frame.
- The palette's colours are display values at EV100 15; a stylized light intensity is lux = display x 39321.6. The
  block owns the exposure: an app that sets `renderer_set_exposure_ev100` itself is overwritten each change.
- Night too dark or too bright: `night_exposure` (stops, + brighter), not the renderer's EV.
- Does NOT: advance seasons or the moon, own weather, or replicate.
