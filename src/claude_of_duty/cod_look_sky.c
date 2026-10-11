// The sky's photometry over the day_night block's sun and moon: key, ambient, fill budgets.

#define COD_SCENE_LUX 25000.0 // 1 scene light unit in lux, 1 framebuffer unit in cd/m2
#define COD_SUN_ILLUMINANCE_TOP (128000.0 / COD_SCENE_LUX)
#define COD_MOON_ILLUMINANCE_NIGHT 0.30
#define COD_SUN_LUM_FLOOR 0.35
#define COD_SUN_KEY_GAIN 1.55
#define COD_SKY_AMBIENT_FRACTION 0.15
#define COD_ATMO_GROUND_R 6.36 // Mm
#define COD_ATMO_TOP_R 6.46
#define COD_ATMO_VIEW_ALT 0.0002
#define COD_SKY_DEG (3.141592653589793 / 180.0)
#define COD_SUN_RADIUS 0.004654 // angular radii (rad)
#define COD_MOON_RADIUS 0.004516
#define COD_TURBIDITY 1.35
#define COD_SUN_TINT_R 1.0
#define COD_SUN_TINT_G 0.975
#define COD_SUN_TINT_B 0.94
// weather
#define COD_CLOUD_COVERAGE 0.30
#define COD_CLOUD_DENSITY 1.9
#define COD_CIRRUS_COVERAGE 0.21
#define COD_CIRRUS_OPACITY 0.30
#define COD_WIND_SPEED 0.0042 // km/s at the deck
#define COD_WIND_ANGLE 0.7
#define COD_HORIZON_MURK 0.13
#define COD_CLOUD_DECK_KM 1.5

// settings the budgets read
#define COD_SKY_FILL 0.32
#define COD_GROUND_FILL 0.013
#define COD_BOUNCE_FILL 0.008
#define COD_IBL_DIFFUSE 0.030
#define COD_INTERIOR_INDIRECT 0.035

typedef struct CodSky {
  v3 sun, moon;             // toward each body (the block's astronomy)
  f64 sun_alt, moon_alt;    // radians
  f64 moon_phase;           // illuminated fraction from the elongation
  v3 sun_color, moon_color; // normalized to their max channel
  f64 base_sun_intensity, sun_intensity, moon_intensity, beam_gain;
  f64 night_ramp;
  v3 ambient; // the sky's whole-sky irradiance model, scene units
  f64 exposure_bias, indirect_scale;
  b32 key_is_moon;
  v3 key_irradiance; // the key's colour x intensity, scene units
  f64 beam_luminance;   // the beam's luminous level, scene units
  f64 moon_irradiance;  // the dome's moonlight before the cool tint
  f64 rolloff_knee;     // the sky shoulder, scene units
  f64 cloud_occlusion;  // eased direct sunlight through the deck at the camera, 1 = clear
} CodSky;

hz_internal f64 cod_sky_smoothstep(f64 x, f64 lo, f64 hi) {
  if (x <= lo)
    return 0.0;
  if (x >= hi)
    return 1.0;
  x = (x - lo) / (hi - lo);
  return x * x * (3.0 - 2.0 * x);
}

// the viewer to the top of the atmosphere at elevation sine mu
hz_internal v3 cod_transmittance_to_space(f64 mu, f64 mie_scale) {
  const f64 R = COD_ATMO_GROUND_R + COD_ATMO_VIEW_ALT, top = COD_ATMO_TOP_R;
  f64 disc = R * R * mu * mu - R * R + top * top;
  if (disc <= 0.0)
    return (v3){0};
  f64 t_top = -R * mu + m_sqrt(disc);
  f64 g_disc = R * R * mu * mu - R * R + COD_ATMO_GROUND_R * COD_ATMO_GROUND_R;
  if (mu < 0.0 && g_disc > 0.0)
    return (v3){0};
  const u32 n = 48;
  f64 dt = t_top / n, od0 = 0.0, od1 = 0.0, od2 = 0.0;
  for (u32 i = 0; i < n; i++) {
    f64 t = (i + 0.5) * dt;
    f64 h = m_sqrt(R * R + t * t + 2.0 * R * t * mu);
    f64 alt_km = m_max((h - COD_ATMO_GROUND_R) * 1000.0, 0.0);
    f64 r_den = m_exp(-alt_km / 8.0), m_den = m_exp(-alt_km / 1.2);
    f64 mie = (3.996 + 4.4) * mie_scale * m_den;
    f64 oz = m_max(0.0, 1.0 - m_abs(alt_km - 25.0) / 15.0);
    od0 += (5.802 * r_den + mie + 0.65 * oz) * dt;
    od1 += (13.558 * r_den + mie + 1.881 * oz) * dt;
    od2 += (33.1 * r_den + mie + 0.085 * oz) * dt;
  }
  return (v3){(f32)m_exp(-od0), (f32)m_exp(-od1), (f32)m_exp(-od2)};
}

// the cloud deck over the camera dims the sun within 0.58..1
hz_internal void cod_sky_apply_intensities(CodSky *s) {
  s->sun_intensity = s->base_sun_intensity * (0.58 + 0.42 * s->cloud_occlusion) * COD_SUN_KEY_GAIN;
  s->key_is_moon = s->moon_intensity > s->sun_intensity;
  v3 c = s->key_is_moon ? s->moon_color : s->sun_color;
  s->key_irradiance = v3_scale(c, (f32)(s->key_is_moon ? s->moon_intensity : s->sun_intensity));
}

// the CPU twin of the shader's sk_cloud_macro
hz_internal f64 cod_cloud_macro(f64 x, f64 y) {
  f64 a = m_sin(x * 0.412 + 0.7) * m_cos(y * 0.331 - 0.4);
  f64 b = m_sin(x * 0.173 - y * 0.209 + 1.9);
  f64 c = m_cos(x * 0.0871 + y * 0.1123 - 0.6);
  return m_clamp(0.5 + 0.5 * (0.42 * a + 0.36 * b + 0.3 * c), 0.0, 1.0);
}

// direct sunlight surviving the deck above a world point (m), the macro field only
hz_internal f64 cod_cloud_sun_occlusion(f64 world_x, f64 world_z, v3 sun, f64 time) {
  f64 k = COD_CLOUD_DECK_KM / m_max(0.1, (f64)sun.y);
  f64 wx = m_cos(COD_WIND_ANGLE) * COD_WIND_SPEED, wz = m_sin(COD_WIND_ANGLE) * COD_WIND_SPEED;
  f64 px = world_x * 0.001 + sun.x * k + wx * time;
  f64 pz = world_z * 0.001 + sun.z * k + wz * time;
  f64 macro = cod_cloud_macro(px * 0.22, pz * 0.22);
  f64 cov = m_clamp(COD_CLOUD_COVERAGE * (0.34 + 1.3 * macro), 0.0, 1.0);
  f64 d = m_clamp((cov - 0.42) / 0.62, 0.0, 1.0);
  return m_exp(-d * COD_CLOUD_DENSITY * 1.55);
}

// the sun and moon state and light intensities for the given bodies
hz_internal void cod_sky_set_bodies(CodSky *s, v3 sun, v3 moon) {
  s->sun = sun;
  s->moon = moon;
  s->sun_alt = m_asin(m_clamp((f64)sun.y, -1.0, 1.0));
  s->moon_alt = m_asin(m_clamp((f64)moon.y, -1.0, 1.0));
  f64 elong = m_acos(m_clamp((f64)v3_dot(sun, moon), -1.0, 1.0));
  s->moon_phase = 0.5 * (1.0 - m_cos(elong));

  // sun: the disc, its aureole-shifted beam, the luminance floor
  f64 mu_s = m_sin(s->sun_alt), alt_deg = s->sun_alt / COD_SKY_DEG;
  f64 disc_sun = m_clamp(0.5 + mu_s / (2.0 * COD_SUN_RADIUS), 0.0, 1.0);
  v3 t = cod_transmittance_to_space(m_max(mu_s, 0.0008), COD_TURBIDITY);
  f64 p = m_lerp(0.55, 1.0, cod_sky_smoothstep(alt_deg, 0.0, 16.0));
  f64 sr = m_pow(t.x, p) * COD_SUN_TINT_R, sg = m_pow(t.y, p) * COD_SUN_TINT_G, sb = m_pow(t.z, p) * COD_SUN_TINT_B;
  f64 smax = m_max(1e-6, m_max(sr, m_max(sg, sb)));
  s->sun_color = (v3){(f32)(sr / smax), (f32)(sg / smax), (f32)(sb / smax)};
  f64 lum_t = 0.2126 * sr + 0.7152 * sg + 0.0722 * sb;
  f64 beam_alive = cod_sky_smoothstep(alt_deg, -6.0, -1.0);
  s->beam_gain = m_max(1.0, COD_SUN_LUM_FLOOR * beam_alive / m_max(lum_t, 1e-5));
  s->base_sun_intensity = COD_SUN_ILLUMINANCE_TOP * smax * disc_sun * s->beam_gain;
  f64 key_ramp = cod_sky_smoothstep(-alt_deg, -3.0, 5.0);
  s->night_ramp = cod_sky_smoothstep(-alt_deg, 0.0, 9.0);

  // moon
  f64 mu_m = m_sin(s->moon_alt);
  f64 disc_m = m_clamp(0.5 + mu_m / (2.0 * COD_MOON_RADIUS), 0.0, 1.0);
  v3 mt = cod_transmittance_to_space(m_max(mu_m, 0.0008), COD_TURBIDITY);
  f64 mr = mt.x * 0.66, mg = mt.y * 0.80, mb = mt.z * 1.0;
  f64 mmax = m_max(1e-6, m_max(mr, m_max(mg, mb)));
  s->moon_color = (v3){(f32)(mr / mmax), (f32)(mg / mmax), (f32)(mb / mmax)};
  f64 moon_i = COD_MOON_ILLUMINANCE_NIGHT * s->moon_phase * mmax * disc_m * key_ramp;
  if (m_max(s->base_sun_intensity, moon_i) < 0.03)
    moon_i = 0.03;
  s->moon_intensity = moon_i;

  // the published ambient, the exposure bias, the indirect budget
  f64 warm = (1.0 - cod_sky_smoothstep(alt_deg, 1.0, 22.0)) * beam_alive, night = 1.0 - beam_alive;
  f64 ar = m_lerp(m_lerp(0.36, 0.35, night), s->sun_color.x, warm);
  f64 ag = m_lerp(m_lerp(0.56, 0.5, night), s->sun_color.y, warm);
  f64 ab = m_lerp(1.0, s->sun_color.z, warm);
  f64 a_level = COD_SKY_AMBIENT_FRACTION * s->base_sun_intensity + 0.9 * moon_i;
  s->ambient = (v3){(f32)(ar * a_level), (f32)(ag * a_level), (f32)(ab * a_level)};
  s->exposure_bias = 1.35 * (1.0 - cod_sky_smoothstep(alt_deg, 1.0, 13.0)) * beam_alive + 0.55 * (1.0 - beam_alive);
  s->indirect_scale = m_lerp(2.2, m_lerp(0.45, 1.0, cod_sky_smoothstep(alt_deg, 0.0, 14.0)), beam_alive);

  s->beam_luminance = COD_SUN_ILLUMINANCE_TOP * m_max(lum_t * s->beam_gain, 1e-6) * disc_sun;
  s->moon_irradiance = COD_MOON_ILLUMINANCE_NIGHT * s->moon_phase * key_ramp;
  f64 knee_frac = m_lerp(0.045, 0.11, cod_sky_smoothstep(alt_deg, 2.0, 15.0));
  s->rolloff_knee = m_max(knee_frac * s->beam_luminance, 0.02 + 6.0 * moon_i);
  cod_sky_apply_intensities(s);
}

// the bounce fill: the two hemispheric bands, scene irradiance units
hz_internal void cod_sky_fill(const CodSky *s, v3 *sky_fill, v3 *ground_fill) {
  f64 hr = s->ambient.x, hg = s->ambient.y, hb = s->ambient.z, level;
  if (m_max(hr, m_max(hg, hb)) > 1e-5) {
    level = m_max(hr, m_max(hg, hb));
  } else {
    hr = 0.36, hg = 0.56, hb = 1.0;
    level = 0.15 * m_max(0.0, (f64)v3_length(s->key_irradiance));
  }
  f64 mx = m_max(hr, m_max(hg, hb));
  hr /= mx, hg /= mx, hb /= mx;
  // a little extra chroma so the sky band reads blue against the neutral env
  f64 l = 0.2126 * hr + 0.7152 * hg + 0.0722 * hb, k = 1.18;
  hr = m_max(0.0, l + (hr - l) * k), hg = m_max(0.0, l + (hg - l) * k), hb = m_max(0.0, l + (hb - l) * k);
  mx = m_max(1e-6, m_max(hr, m_max(hg, hb)));
  f64 sky_level = COD_SKY_FILL * (level / 0.15);
  *sky_fill = (v3){(f32)(hr / mx * sky_level), (f32)(hg / mx * sky_level), (f32)(hb / mx * sky_level)};
  // warm street bounce from the key, tinted by the ground albedo
  v3 kc = s->key_is_moon ? s->moon_color : s->sun_color;
  f64 key_i = s->key_is_moon ? s->moon_intensity : s->sun_intensity;
  f64 gr = kc.x * 0.33, gg = kc.y * 0.29, gb = kc.z * 0.225;
  f64 gm = m_max(1e-6, m_max(gr, m_max(gg, gb)));
  f64 ground_level = COD_GROUND_FILL * m_max(0.0, key_i);
  *ground_fill = (v3){(f32)(gr / gm * ground_level), (f32)(gg / gm * ground_level), (f32)(gb / gm * ground_level)};
}

// viewmodel rig settings
#define COD_REF_DAYLIGHT 4.6
#define COD_VIEW_KEY_SCALE 0.55
#define COD_VIEW_KEY_MAX 2.6
#define COD_VIEW_KEY_GAMMA 0.65
#define COD_VIEW_FILL_RATIO 0.3
#define COD_VIEW_RIM_RATIO 0.5
#define COD_VIEW_HEMI_RATIO 0.16
#define COD_VIEW_BOUNCE_RATIO 0.34
#define COD_VIEW_FILL_OCCLUSION 0.45f
#define COD_VIEW_ENV_OCCLUSION 0.24f // the viewmodel's environment intensity

// the viewmodel's 3-point rig, colour times intensity in scene units
typedef struct CodViewRig {
  f32 key_intensity;
  v3 key, fill, rim, bounce, hemi_sky, hemi_ground;
} CodViewRig;

// every light a ratio of the scene's level, sub-linear so the gun stays legible after dark
hz_internal CodViewRig cod_view_rig(const CodSky *s) {
  v3 kc = s->key_is_moon ? s->moon_color : s->sun_color;
  f64 key_i = s->key_is_moon ? s->moon_intensity : s->sun_intensity;
  f64 amb_level = m_max(s->ambient.x, m_max(s->ambient.y, s->ambient.z));
  if (amb_level <= 1e-5)
    amb_level = 0.15 * key_i;
  f64 ref = m_max(key_i, amb_level / 0.15);
  f64 shaped = COD_REF_DAYLIGHT * m_pow(m_min(ref / COD_REF_DAYLIGHT, 1.0), COD_VIEW_KEY_GAMMA);
  f32 k = (f32)m_min(shaped * COD_VIEW_KEY_SCALE, COD_VIEW_KEY_MAX);
  v3 sky_fill, ground_fill;
  cod_sky_fill(s, &sky_fill, &ground_fill);
  f32 sm = m_maxf(sky_fill.x, m_maxf(sky_fill.y, sky_fill.z)), gm = m_maxf(ground_fill.x, m_maxf(ground_fill.y, ground_fill.z));
  v3 hue = sm > 1e-6f ? v3_scale(sky_fill, 1.0f / sm) : (v3){0.36f, 0.56f, 1.0f};
  CodViewRig rig = {.key_intensity = k};
  rig.key = v3_scale(kc, k);
  rig.fill = v3_scale(hue, k * (f32)COD_VIEW_FILL_RATIO);
  rig.rim = v3_scale((v3){kc.x, kc.y * 0.94f, kc.z * 0.82f}, k * (f32)COD_VIEW_RIM_RATIO);
  // with no ground band the bounce keeps its authored 0xffb87a
  v3 g = gm > 1e-5f ? v3_scale(ground_fill, 1.0f / gm) : (v3){0};
  v3 bounce = gm > 1e-5f ? (v3){g.x, g.y * 0.86f, g.z * 0.62f} : (v3){1.0f, 0.4793f, 0.1946f};
  rig.bounce = v3_scale(bounce, k * (f32)COD_VIEW_BOUNCE_RATIO);
  f32 hemi = k * (f32)COD_VIEW_HEMI_RATIO;
  rig.hemi_sky = v3_scale((v3){srgb_to_linear(0x8f / 255.0f), srgb_to_linear(0xb6 / 255.0f), 1.0f}, hemi);
  rig.hemi_ground = v3_scale((v3){srgb_to_linear(0x36 / 255.0f), srgb_to_linear(0x30 / 255.0f), srgb_to_linear(0x2a / 255.0f)}, hemi);
  return rig;
}
