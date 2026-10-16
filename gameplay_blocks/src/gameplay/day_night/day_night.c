#include "day_night.h"
#include "ecs/ecs_assets/ecs_assets.h"
#include "ecs/renderer/ecs_renderer.h"
#include "ecs/transform/ecs_transform.h"
#include "lib/assert.h"
#include "rendering/features/atmosphere.h"
#include "rendering/features/exposure.h"
#include "rendering/features/stylized_sky.h"

#define DN_OBLIQUITY_DEG 23.44f
#define DN_EQUINOX_DAY 79.0f // the March equinox, day of year
#define DN_YEAR_DAYS 365.2422f
#define DN_LIGHT_STEP_DEG 0.25f
#define DN_SUN_LUX 120000.0f
#define DN_MOON_LUX 0.25f
#define DN_STARS_CD 4.0f
// the stylized key light stays this far above the horizon (sin 0.08), so shadows never stretch to infinity
#define DN_STYLIZED_KEY_MIN_SIN 0.08f
// a body this far below the horizon lends no direct light: the key light stops casting
#define DN_CAST_MIN_DEG -2.0f
// auto-exposure meters within this many stops of the palette's EV100
#define DN_AUTO_EXPOSURE_RANGE 1.5f

DayNightCycle day_night_cycle_default(void) {
  return (DayNightCycle){.time_of_day = 10.0f,
                         .day_length = 24.0f * 60.0f,
                         .latitude_deg = 45.0f,
                         .day_of_year = 172.0f,
                         .moon_phase = 0.5f};
}

// a body at right ascension `ra` and declination `dec` (radians) in the celestial frame: +Y the pole, ra 0 on +Z
hz_internal v3 dn_equatorial(f32 ra, f32 dec) {
  return (v3){m_cosf(dec) * m_sinf(ra), m_sinf(dec), m_cosf(dec) * m_cosf(ra)};
}

// the ecliptic longitude `lon` (radians) as right ascension and declination
hz_internal void dn_ecliptic(f32 lon, f32 *ra, f32 *dec) {
  f32 e = m_radians(DN_OBLIQUITY_DEG);
  *dec = m_asinf(m_sinf(e) * m_sinf(lon));
  *ra = m_atan2f(m_cosf(e) * m_sinf(lon), m_cosf(lon));
}

hz_internal f32 dn_elevation_deg(v3 dir) { return m_degrees(m_asinf(m_clampf(dir.y, -1.0f, 1.0f))); }

DayNightBodies day_night_astronomy(const DayNightCycle *c) {
  f32 sun_lon = 2.0f * (f32)M_PI * (c->day_of_year - DN_EQUINOX_DAY) / DN_YEAR_DAYS;
  f32 sun_ra, sun_dec, moon_ra, moon_dec;
  dn_ecliptic(sun_lon, &sun_ra, &sun_dec);
  // the moon on the ecliptic, its elongation east of the sun set by the phase (its 5 degree inclination left out)
  f32 phase = c->moon_phase - m_floorf(c->moon_phase);
  dn_ecliptic(sun_lon + 2.0f * (f32)M_PI * phase, &moon_ra, &moon_dec);
  if (c->moon_declination_fixed)
    moon_dec = m_radians(m_clampf(c->moon_declination_deg, -90.0f, 90.0f));
  // local sidereal angle: the sun's hour angle (0 at solar noon) plus its right ascension
  f32 sidereal = 2.0f * (f32)M_PI * (c->time_of_day - 12.0f) / 24.0f + sun_ra;
  // celestial -> world: the sky turns west about the pole, the pole tilts to the latitude, north turns by the yaw
  quat spin = quat_axis_angle(V3_UP, -sidereal);
  quat tilt = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, m_radians(c->latitude_deg - 90.0f));
  quat yaw = quat_axis_angle(V3_UP, m_radians(c->north_yaw_deg));
  quat celestial = quat_normalize(quat_mul(yaw, quat_mul(tilt, spin)));
  DayNightBodies b = {.celestial = celestial};
  b.sun_dir = v3_normalize(quat_rotate(celestial, dn_equatorial(sun_ra, sun_dec)));
  b.moon_dir = v3_normalize(quat_rotate(celestial, dn_equatorial(moon_ra, moon_dec)));
  b.sun_elevation_deg = dn_elevation_deg(b.sun_dir);
  b.moon_elevation_deg = dn_elevation_deg(b.moon_dir);
  // the lunar phase law (Allen): magnitude 0.026 a + 4e-9 a^4 at phase angle a (degrees) relative to full
  f32 a = m_absf(180.0f - phase * 360.0f);
  b.moon_brightness = m_exp2f(-0.4f * (0.026f * a + 4e-9f * a * a * a * a) * 3.321928f);
  return b;
}

hz_internal v3 dn_lin(ColorF32 c) { return vec3_srgb_to_linear((v3){c.r, c.g, c.b}); }

hz_internal ColorF32 dn_srgb(v3 c) { return (ColorF32){linear_to_srgb(c.x), linear_to_srgb(c.y), linear_to_srgb(c.z), 1.0f}; }

hz_internal ColorF32 dn_mix(ColorF32 a, ColorF32 b, f32 t) { return dn_srgb(v3_lerp(dn_lin(a), dn_lin(b), t)); }

// colour x intensity blended as one light, split back into an sRGB colour (peak 1) and the intensity
hz_internal void dn_mix_light(ColorF32 ac, f32 ai, ColorF32 bc, f32 bi, f32 t, ColorF32 *color, f32 *intensity) {
  v3 c = v3_lerp(v3_scale(dn_lin(ac), ai), v3_scale(dn_lin(bc), bi), t);
  f32 peak = m_maxf(m_maxf(c.x, c.y), c.z);
  *intensity = peak;
  *color = peak > 0.0f ? dn_srgb(v3_scale(c, 1.0f / peak)) : dn_mix(ac, bc, t);
}

DayNightMood day_night_palette_sample(Array(DayNightMood) moods, f32 sun_elevation_deg) {
  if (moods.len == 0)
    return (DayNightMood){0};
  const DayNightMood *a = &moods.items[0];
  const DayNightMood *b = &moods.items[moods.len - 1];
  if (sun_elevation_deg <= a->sun_elevation)
    b = a;
  else if (sun_elevation_deg >= b->sun_elevation)
    a = b;
  else
    for (u32 i = 0; i + 1 < moods.len; i++) {
      if (sun_elevation_deg <= moods.items[i + 1].sun_elevation) {
        a = &moods.items[i];
        b = &moods.items[i + 1];
        break;
      }
    }
  f32 t = a == b ? 0.0f : m_smoothstepf(a->sun_elevation, b->sun_elevation, sun_elevation_deg);
  DayNightMood m = {.sun_elevation = sun_elevation_deg};
  dn_mix_light(a->light_color, a->light_intensity, b->light_color, b->light_intensity, t, &m.light_color,
               &m.light_intensity);
  dn_mix_light(a->ambient, a->ambient_intensity, b->ambient, b->ambient_intensity, t, &m.ambient, &m.ambient_intensity);
  m.zenith = dn_mix(a->zenith, b->zenith, t);
  m.horizon = dn_mix(a->horizon, b->horizon, t);
  m.glow = dn_mix(a->glow, b->glow, t);
  m.cloud_lit = dn_mix(a->cloud_lit, b->cloud_lit, t);
  m.cloud_shade = dn_mix(a->cloud_shade, b->cloud_shade, t);
  m.fog = dn_mix(a->fog, b->fog, t);
  m.stars = m_lerpf(a->stars, b->stars, t);
  m.ev100 = m_lerpf(a->ev100, b->ev100, t);
  return m;
}

DayNightKey day_night_key(const DayNightBodies *b, b32 stylized) {
  f32 h = DAY_NIGHT_HANDOFF_DEG, wd = DAY_NIGHT_HANDOFF_WIDTH_DEG;
  DayNightKey k = {.is_moon = b->sun_elevation_deg < h};
  k.weight = k.is_moon ? m_smoothstepf(h, h - wd, b->sun_elevation_deg) : m_smoothstepf(h, h + wd, b->sun_elevation_deg);
  k.dir = k.is_moon ? b->moon_dir : b->sun_dir;
  k.elevation_deg = k.is_moon ? b->moon_elevation_deg : b->sun_elevation_deg;
  if (!stylized)
    return k;
  // the palette sets the light, not the atmosphere: a setting moon's light fades by hand
  if (k.is_moon)
    k.weight *= m_smoothstepf(DN_CAST_MIN_DEG, 4.0f, k.elevation_deg);
  if (k.dir.y < DN_STYLIZED_KEY_MIN_SIN) {
    v2 flat = {k.dir.x, k.dir.z};
    f32 len = v2_length(flat);
    flat = len > 1e-6f ? v2_scale(flat, 1.0f / len) : (v2){0.0f, -1.0f};
    f32 horizontal = m_sqrtf(1.0f - DN_STYLIZED_KEY_MIN_SIN * DN_STYLIZED_KEY_MIN_SIN);
    k.dir = (v3){flat.x * horizontal, DN_STYLIZED_KEY_MIN_SIN, flat.y * horizontal};
  }
  return k;
}

v3 day_night_step_light(v3 held, v3 target, v3 prev_target, f32 step_deg) {
  if (v3_eqv(target, prev_target))
    return target;
  return v3_dot(held, target) < m_cosf(m_radians(step_deg)) ? target : held;
}

// the cycle's palette, else the module's for the sky; NULL while it loads
hz_internal DayNightPalette *dn_palette(const AssetSystem *a, const DayNightRuntime *rt, const DayNightCycle *c) {
  BlobAssetRef ref = c->palette;
  if (handle_is_valid(ref.handle))
    return asset_is_ready(a, ref.handle) ? blob_asset_ref_get(DayNightPalette, a, &ref) : NULL;
  if (ref.path.len > 0)
    return NULL;
  return handle_is_valid(rt->palette) && asset_is_ready(a, rt->palette) ? asset_get(a, rt->palette, DayNightPalette)
                                                                          : NULL;
}

// once per frame before the cycle: the edges and the count of cycles reset
ecs_system(day_night_begin_system, EcsResInOut(DayNightState), EcsResInOut(DayNightRuntime)) {
  DayNightState *s = ecs_sys_get_resource_mut(DayNightState);
  s->ready = false;
  s->sunrise = false;
  s->sunset = false;
  ecs_sys_get_resource_mut(DayNightRuntime)->cycles = 0;
}

// the clock, the bodies, the palette and the sun entity's light; the first cycle wins
ecs_system(day_night_system, EcsInOut(DayNightCycle), EcsInOut(EcsDirectionalLight), EcsInOut(WorldTransform),
           EcsOptional(LocalTransform), EcsOptional(EcsParent), EcsResInOut(DayNightState), EcsResInOut(DayNightRuntime), EcsResIn(AssetSystem)) {
  DayNightState *s = ecs_sys_get_resource_mut(DayNightState);
  DayNightRuntime *rt = ecs_sys_get_resource_mut(DayNightRuntime);
  const AssetSystem *assets = ecs_sys_get_resource(AssetSystem);
  for (i32 i = 0; i < it->count; i++) {
    if (rt->cycles++ > 0) {
      if (!rt->warned_cycles)
        log_warn("day_night: more than one DayNightCycle; only the first drives the sun");
      rt->warned_cycles = true;
      continue;
    }
    assert_msg(!ecs_sys_get_component_optional(EcsParent, i), "day_night: the DayNightCycle's sun must be a root");
    const DayNightCycle *cr = ecs_sys_get_component(DayNightCycle, i);
    if (!cr->paused && cr->day_length > 0.0f) {
      DayNightCycle *c = ecs_sys_get_component_mut(DayNightCycle, i);
      f32 t = c->time_of_day + it->delta_time * 24.0f / c->day_length;
      f32 days = m_floorf(t / 24.0f);
      c->time_of_day = t - days * 24.0f;
      if (days > 0.0f)
        c->day += (u32)days;
    }
    const DayNightCycle *c = ecs_sys_get_component(DayNightCycle, i);
    DayNightPalette *pal = dn_palette(assets, rt, c);
    if (!pal)
      continue;
    b32 stylized = rt->sky != DAY_NIGHT_SKY_PHYSICAL;
    DayNightBodies b = day_night_astronomy(c);
    DayNightKey key = day_night_key(&b, stylized);
    DayNightMood mood = day_night_palette_sample(blob_array_get(DayNightMood, &pal->moods), b.sun_elevation_deg);

    // a new key body or the first frame turns at once; else the light steps, settling exactly at rest
    f32 step = c->light_step_deg > 0.0f ? c->light_step_deg : DN_LIGHT_STEP_DEG;
    b32 snap = !rt->have_prev || key.is_moon != rt->prev_key_is_moon;
    v3 held = snap ? key.dir : day_night_step_light(s->key_dir, key.dir, rt->prev_target, step);

    f32 body_lux = stylized ? mood.light_intensity
                   : key.is_moon ? (c->physical_moon_illuminance > 0.0f ? c->physical_moon_illuminance : DN_MOON_LUX)
                                 : (c->physical_sun_illuminance > 0.0f ? c->physical_sun_illuminance : DN_SUN_LUX);
    f32 lux = key.weight * body_lux * (key.is_moon ? b.moon_brightness : 1.0f);
    f32 night = m_smoothstepf(0.0f, -12.0f, b.sun_elevation_deg);
    EcsDirectionalLight light = *ecs_sys_get_component(EcsDirectionalLight, i);
    light.color = mood.light_color;
    light.intensity = lux;
    light.cast_shadow = !c->no_shadows && lux > 0.0f && key.elevation_deg > DN_CAST_MIN_DEG;
    if (mem_cmp(&light, ecs_sys_get_component(EcsDirectionalLight, i), sizeof(light)) != 0)
      *ecs_sys_get_component_mut(EcsDirectionalLight, i) = light;
    v3 travel = v3_negate(held);
    quat rot = quat_look(travel, m_absf(travel.y) > 0.99f ? V3_FORWARD : V3_UP);
    // a root's LocalTransform is never propagated: the world rotation drives the light, the local one the editor
    if (mem_cmp(&rot, &ecs_sys_get_component(WorldTransform, i)->rotation, sizeof(rot)) != 0)
      ecs_sys_get_component_mut(WorldTransform, i)->rotation = rot;
    const LocalTransform *lt = ecs_sys_get_component_optional(LocalTransform, i);
    if (lt && mem_cmp(&rot, &lt->rotation, sizeof(rot)) != 0)
      ecs_sys_get_component_optional_mut(LocalTransform, i)->rotation = rot;

    b32 was_up = rt->have_prev && rt->prev_sun_elevation_deg >= 0.0f;
    b32 is_up = b.sun_elevation_deg >= 0.0f;
    *s = (DayNightState){.ready = true,
                         .time_of_day = c->time_of_day,
                         .day = c->day,
                         .sun_dir = b.sun_dir,
                         .moon_dir = b.moon_dir,
                         .key_dir = held,
                         .key_is_moon = key.is_moon,
                         .key_lux = lux,
                         .ev100 = mood.ev100 - c->night_exposure * night,
                         .sun_elevation_deg = b.sun_elevation_deg,
                         .moon_elevation_deg = b.moon_elevation_deg,
                         .moon_brightness = b.moon_brightness,
                         .daylight = m_smoothstepf(-6.0f, 6.0f, b.sun_elevation_deg),
                         .night = night,
                         .sunrise = rt->have_prev && !was_up && is_up,
                         .sunset = was_up && !is_up,
                         .celestial = b.celestial,
                         .mood = mood};
    rt->stars_luminance = (c->physical_stars_luminance > 0.0f ? c->physical_stars_luminance : DN_STARS_CD) * mood.stars;
    rt->prev_target = key.dir;
    rt->prev_key_is_moon = key.is_moon;
    rt->prev_sun_elevation_deg = b.sun_elevation_deg;
    rt->have_prev = true;
  }
}

// the palette's ambient and fog into the scene's settings, written only when they change
ecs_system(day_night_scene_system, EcsInOut(EcsSceneSettings), EcsResIn(DayNightState)) {
  const DayNightState *s = ecs_sys_get_resource(DayNightState);
  if (!s->ready)
    return;
  for (i32 i = 0; i < it->count; i++) {
    EcsSceneSettings next = *ecs_sys_get_component(EcsSceneSettings, i);
    next.ambient_color = s->mood.ambient;
    next.ambient_intensity = s->mood.ambient_intensity;
    next.fog_color = s->mood.fog;
    if (mem_cmp(&next, ecs_sys_get_component(EcsSceneSettings, i), sizeof(next)) != 0)
      *ecs_sys_get_component_mut(EcsSceneSettings, i) = next;
  }
}

void day_night_init(EcsWorld *w, RendererState *r, Allocator *a, DayNightSky sky) {
  assert_msg(is_main_thread(), "day_night_init: main lane only"); // ECS resources are set outside a scope
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  assert_msg(assets && (r || sky == DAY_NIGHT_SKY_NONE), "day_night_init: after ecs_assets_init and ecs_renderer_init");
  DayNightRuntime rt = {.sky = sky, .base = stylized_sky_default_params()};
  rt.base.exposure = 1.0f / renderer_pre_exposure_from_ev100(RENDERER_EV100_DEFAULT);
  rt.palette = asset_load(DayNightPalette, assets,
                          .path = sky == DAY_NIGHT_SKY_PHYSICAL ? DAY_NIGHT_PALETTE_PHYSICAL : DAY_NIGHT_PALETTE_STYLIZED);
  if (sky == DAY_NIGHT_SKY_STYLIZED)
    assert_msg(renderer_stylized_sky_add(r, a, &rt.base).id != 0, "day_night_init: the stylized sky did not register");
  else if (sky == DAY_NIGHT_SKY_PHYSICAL)
    assert_msg(renderer_sky_atmosphere_add(r, a, NULL).id != 0, "day_night_init: the atmosphere did not register");
  ecs_resource_set(w, DayNightRuntime, rt);
  ecs_resource_set(w, DayNightState, ((DayNightState){.celestial = quat_identity()}));
}

void day_night_schedule(EcsWorld *w) {
  ecs_schedule_single(w, day_night_begin_system);
  ecs_schedule_single(w, day_night_system);
  ecs_schedule_single(w, day_night_scene_system);
}

StylizedSkyParams *day_night_stylized_base(EcsWorld *w) { return &ecs_resource_get_mut(w, DayNightRuntime)->base; }

// the stylized sky at this instant: the base, with the palette's colours and the key body's disc and halo
hz_internal StylizedSkyParams dn_stylized_params(const DayNightRuntime *rt, const DayNightState *s) {
  StylizedSkyParams p = rt->base;
  const DayNightMood *m = &s->mood;
  // the palette is display values at EV100 15 = radiance at the sky's exposure; a light's lux x pre-exposure
  f32 per_lux = p.exposure > 0.0f ? 1.0f / p.exposure : 0.0f;
  p.sun_dir = s->key_is_moon ? s->moon_dir : s->sun_dir;
  p.sun_color = v3_scale(dn_lin(m->light_color), s->key_lux * per_lux);
  p.corona = s->key_is_moon ? 0.0f : rt->base.corona;
  p.zenith = dn_lin(m->zenith);
  p.horizon = dn_lin(m->horizon);
  p.mid = v3_lerp(p.horizon, p.zenith, 0.5f);
  p.ground = p.horizon;
  p.glow = dn_lin(m->glow);
  p.stars = m->stars;
  p.stars_rotation = s->celestial;
  p.cloud_lit = dn_lin(m->cloud_lit);
  p.cloud_shade = dn_lin(m->cloud_shade);
  p.haze_color = dn_lin(m->fog);
  return p;
}

void day_night_sync(EcsWorld *w, RendererState *r) {
  assert_msg(is_main_thread(), "day_night_sync: main lane only"); // renderer settings: lane 0 records the frame
  const DayNightState *s = ecs_resource_get(w, DayNightState);
  DayNightRuntime *rt = ecs_resource_get_mut(w, DayNightRuntime);
  if (!s->ready)
    return;
  if (!r)
    return;
  if (rt->sky == DAY_NIGHT_SKY_STYLIZED) {
    StylizedSkyParams p = dn_stylized_params(rt, s);
    renderer_set_stylized_sky(r, &p);
  } else if (rt->sky == DAY_NIGHT_SKY_PHYSICAL) {
    renderer_sky_atmosphere_set_stars(r, rt->stars_luminance, s->celestial);
  }
  f32 ev = s->ev100;
  if (renderer_auto_exposure_enabled(r)) {
    AutoExposureParams ae = renderer_get_auto_exposure(r);
    if (ae.limit_min_ev != ev - DN_AUTO_EXPOSURE_RANGE || ae.limit_max_ev != ev + DN_AUTO_EXPOSURE_RANGE) {
      ae.limit_min_ev = ev - DN_AUTO_EXPOSURE_RANGE;
      ae.limit_max_ev = ev + DN_AUTO_EXPOSURE_RANGE;
      renderer_set_auto_exposure(r, &ae);
    }
  } else if (renderer_exposure_ev100(r) != ev) {
    renderer_set_exposure_ev100(r, ev);
  }
}
