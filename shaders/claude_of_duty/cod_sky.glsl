// The sky: the engine's atmosphere with the dome over it: aureoles, the cumulus and cirrus decks, horizon murk, the highlight roll-off.
#include "renderer.glsl"
#include "sky_atmosphere_common.glsl"
#include "cod_sky_block.glsl"

uniform _HzAtmosphere { SkyAtmosphere hz_atmosphere; };
uniform _HzAtmosphereSky { mat4 hz_atmosphere_inv_view_proj_rot; };
uniform _DrawParams { DrawParams hz_sky_draw; };

@vs
#define HZ_RENDERER_VERTEX_SHADER
#include "renderer_shaders.glsl"
out vec3 v_dir;

void main() {
    vec2 uv = fullscreen_triangle_uv(uint(gl_VertexIndex));
    gl_Position = fullscreen_triangle_pos(uv, 0.0);
    v_dir = (hz_atmosphere_inv_view_proj_rot * gl_Position).xyz;
}
@end

@fs
#undef HZ_RENDERER_VERTEX_SHADER
#define HZ_RENDERER_FRAGMENT_SHADER
#define HZ_RENDERER_LIGHTING_BASIC
#define HZ_RENDERER_IBL
#define HZ_RENDERER_FOG
#define HZ_RENDERER_OUTPUT
#include "renderer_shaders.glsl"
#include "hz_noise.glsl"
#include "cod_sky_block.glsl"
in vec3 v_dir;

@material
struct MaterialFS {
    @texture2D @black
    uint hz_sky_view_lut;
    @texture2D @white
    uint hz_sky_transmittance_lut;
};

// the LUTs have one mip: explicit lod 0 is the same sample and legal in the decks' non-uniform branches
uint g_sky_view_slot;
uint g_transmittance_slot;
#define SKY_SAMPLE_SKY_VIEW(uv) texture_sample_lod(hz_sky_view_lut, g_sky_view_slot, uv, 0.0)
#include "sky_atmosphere_functions.glsl"
#include "sky_stars.glsl"

const float SKY_HDR_MAX = 64512.0;
const float SK_PI = 3.141592653589793;
const mat2 SK_ROT = mat2(0.8, 0.6, -0.6, 0.8);

float sk_ray_sphere(vec3 ro, vec3 rd, float rad);

// the atmosphere's transmittance from p (km, planet-centred) toward dir; zero through the planet, as the LUT integrates it
vec3 cod_transmittance(vec3 p, vec3 dir) {
    if (sk_ray_sphere(p, dir, hz_atmosphere.bottom_radius) > 0.0) return vec3(0.0);
    float h = length(p);
    vec2 uv = sky_transmittance_params_to_uv(h, dot(dir, p / h));
    return texture_sample_lod(hz_sky_transmittance_lut, g_transmittance_slot, sky_clamp_uv(uv, hz_atmosphere.transmittance_size), 0.0).rgb;
}

float cod_sky_lum(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

float sk_fbm2(vec2 p, int oct) {
    float a = 0.5, s = 0.0, n = 0.0;
    for (int i = 0; i < oct; i++) {
        s += a * hz_value_noise(p);
        n += a;
        p = SK_ROT * p * 2.04 + 7.13;
        a *= 0.5;
    }
    return s / max(n, 1e-4);
}

float sk_ridge2(vec2 p, int oct) {
    float a = 0.5, s = 0.0, n = 0.0;
    for (int i = 0; i < oct; i++) {
        s += a * (1.0 - abs(hz_value_noise(p) * 2.0 - 1.0));
        n += a;
        p = SK_ROT * p * 2.11 + 3.71;
        a *= 0.52;
    }
    return s / max(n, 1e-4);
}

// the parabolic-crest ridge: C1 across the crest, so a fibre has a soft shoulder
float sk_smooth_ridge2(vec2 p, int oct) {
    float a = 0.62, s = 0.0, n = 0.0;
    for (int i = 0; i < oct; i++) {
        float v = hz_value_noise(p) * 2.0 - 1.0;
        s += a * (1.0 - v * v);
        n += a;
        p = SK_ROT * p * 2.17 + 3.71;
        a *= 0.45;
    }
    return s / max(n, 1e-4);
}

float sk_hg(float cos_theta, float g) {
    float g2 = g * g;
    float d = max(1e-4, 1.0 + g2 - 2.0 * g * cos_theta);
    return (1.0 - g2) / (4.0 * SK_PI * d * sqrt(d));
}

// Cornette-Shanks, g 0.8
float sk_mie_phase(float cos_theta) {
    const float g = 0.8;
    const float k = 3.0 / (8.0 * SK_PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cos_theta * cos_theta) / pow(1.0 + g * g - 2.0 * g * cos_theta, 1.5);
}

// nearest positive hit of a ray against a sphere at the origin
float sk_ray_sphere(vec3 ro, vec3 rd, float rad) {
    float b = dot(ro, rd);
    float c = dot(ro, ro) - rad * rad;
    if (c > 0.0 && b > 0.0) return -1.0;
    float d = b * b - c;
    if (d < 0.0) return -1.0;
    if (d > b * b) return -b + sqrt(d);
    return -b - sqrt(d);
}

// weather-scale coverage, km; mirrored exactly by cod_cloud_macro on the CPU
float sk_cloud_macro(vec2 p) {
    float a = sin(p.x * 0.412 + 0.7) * cos(p.y * 0.331 - 0.4);
    float b = sin(p.x * 0.173 - p.y * 0.209 + 1.9);
    float c = cos(p.x * 0.0871 + p.y * 0.1123 - 0.6);
    return clamp(0.5 + 0.5 * (0.42 * a + 0.36 * b + 0.30 * c), 0.0, 1.0);
}

// one cirrus family, p in km: an isotropic silhouette, fronts, and a fibre texture that only modulates it
float sk_cirrus_band(vec2 p, float cov, float seed, float base, float rot_km_inv, float len_km, float aniso, int oct) {
    vec2 w = vec2(hz_value_noise(p * 0.30 + seed), hz_value_noise(p * 0.30 + seed + 11.7)) - 0.5;
    float n = sk_fbm2(p * 0.78 + w * 1.3, oct + 1);
    float d = smoothstep(1.0 - cov * 1.65, 1.0 - cov * 0.60, n);
    if (d <= 0.001) return 0.0;
    d *= smoothstep(0.36, 0.66, hz_value_noise(p * 0.12 + seed * 0.5));
    if (d <= 0.001) return 0.0;
    float ang = base + (hz_value_noise(p * rot_km_inv + seed) - 0.5) * 1.1;
    float ca = cos(ang), sa = sin(ang);
    vec2 pr = vec2(p.x * ca - p.y * sa, p.x * sa + p.y * ca);
    float fa = 1.0 / max(0.4, len_km);
    float f = sk_smooth_ridge2(vec2(pr.x * fa, pr.y * fa * aniso) + vec2(seed), oct);
    return d * (0.35 + 1.05 * f);
}

// cumulus optical thickness at a point on the deck, p in km
float sk_cumulus_density(vec2 p, int oct) {
    float macro = sk_cloud_macro(p * 0.22);
    float cov = clamp(cod_sky.clouds.x * (0.34 + 1.30 * macro), 0.0, 1.0);
    vec2 w = vec2(hz_value_noise(p * 0.42), hz_value_noise(p * 0.42 + 19.7)) - 0.5;
    float n = sk_fbm2(p * 1.25 + w * 1.6, oct);
    float d = smoothstep(1.0 - cov, 1.0 - cov * 0.34 + 0.05, n);
    if (d > 0.0 && d < 0.94 && oct > 3) {
        float e = sk_ridge2(p * 5.3 + w * 2.0, 3);
        d = clamp(d - (1.0 - d) * (0.50 - 0.50 * e), 0.0, 1.0);
    }
    return d;
}

// the sunlight reaching a deck point, marched along the light's horizontal projection
float sk_cumulus_light(vec2 p, vec3 light_dir, int oct) {
    vec2 step2 = normalize(light_dir.xz + vec2(1e-4)) * (0.20 / max(0.12, abs(light_dir.y)));
    float tau = sk_cumulus_density(p + step2 * 1.0, oct) * 1.0;
    tau += sk_cumulus_density(p + step2 * 2.4, oct) * 0.7;
    tau += sk_cumulus_density(p + step2 * 4.6, oct) * 0.4;
    return exp(-tau * cod_sky.clouds.y * 2.1);
}

// both decks along a view ray: rgb radiance, a coverage; km, the viewer at the origin's zenith
vec4 sk_clouds(vec3 ray, vec3 view, float ground_r, vec3 sun_dir, vec3 sun_low, vec3 sun_high, vec3 moon_dir,
               vec3 moon_low, vec3 moon_high, vec3 ambient) {
    if (ray.y < -0.008) return vec4(0.0);
    int oct_d = cod_sky.clouds.z > 0.5 ? 6 : 3;
    int oct_l = cod_sky.clouds.z > 0.5 ? 4 : 2;
    const int oct_c = 2;
    vec2 wind = cod_sky.clouds2.zw * cod_sky.clouds.w;
    float cos_sun = dot(ray, sun_dir);
    float cos_moon = dot(ray, moon_dir);

    vec4 cirrus = vec4(0.0);
    float tc = sk_ray_sphere(view, ray, ground_r + COD_SKY_CIRRUS_KM);
    if (tc > 0.0) {
        float fade = (1.0 - smoothstep(22.0, 90.0, tc)) * (1.0 - 0.66 * smoothstep(0.55, 0.85, ray.y));
        if (fade > 0.004) {
            vec2 p = (view + ray * tc).xz + wind * 2.4;
            float cov = clamp(cod_sky.clouds2.x, 0.0, 1.0);
            float d1 = sk_cirrus_band(p, cov, 0.0, 0.24, 0.135, 1.5, 4.0, oct_c);
            float d2 = sk_cirrus_band(p + 137.4, cov * 0.92, 4.7, 1.56, 0.098, 2.0, 3.4, oct_c);
            float d = 1.0 - (1.0 - d1) * (1.0 - d2 * 0.85);
            float a = clamp(d * cod_sky.clouds2.y * fade, 0.0, 0.70);
            float fwd = sk_hg(cos_sun, 0.74) * 3.2 + 0.60;
            vec3 col = (sun_high * fwd + moon_high * (sk_hg(cos_moon, 0.68) * 2.8 + 0.55)) / SK_PI + ambient * 0.85;
            cirrus = vec4(col, a);
        }
    }

    vec4 cumulus = vec4(0.0);
    float tk = sk_ray_sphere(view, ray, ground_r + COD_SKY_CUMULUS_KM);
    if (tk > 0.0) {
        float fade = 1.0 - smoothstep(14.0, 130.0, tk);
        if (fade > 0.004) {
            vec2 p0 = (view + ray * tk).xz + wind;
            // fake vertical extent: the sample shifts along the ray by the height the cloud would have there
            float d_base = sk_cumulus_density(p0, oct_d);
            vec2 shear = ray.xz * (0.85 * d_base / max(0.10, ray.y));
            float d = max(sk_cumulus_density(p0 + shear, oct_d), d_base * 0.55);
            if (d > 0.003) {
                vec2 p = p0 + shear;
                float lit = sk_cumulus_light(p, sun_dir, oct_l);
                float lit_m = sk_cumulus_light(p, moon_dir, oct_l);
                float graze = clamp(0.09 / (abs(ray.y) + 0.09), 0.0, 1.0);
                float thick = d * cod_sky.clouds.y * mix(1.0, 1.7, graze);
                float a = clamp(1.0 - exp(-thick * 3.4), 0.0, 1.0) * fade;
                float powder = 1.0 - exp(-thick * 5.5);
                float rim = pow(clamp(1.0 - d, 0.0, 1.0), 2.0);
                float fwd_s = sk_hg(cos_sun, 0.62) * 4.0 + 0.62;
                float fwd_m = sk_hg(cos_moon, 0.60) * 3.4 + 0.55;
                vec3 direct = sun_low * (lit * (0.55 + 0.45 * powder) * fwd_s + rim * lit * 0.9);
                direct += moon_low * (lit_m * (0.55 + 0.45 * powder) * fwd_m + rim * lit_m * 0.9);
                vec3 fill = ambient * mix(0.50, 1.5, clamp(d * 1.6, 0.0, 1.0)) * (0.32 + 0.68 * lit);
                cumulus = vec4(direct / SK_PI + fill, a);
            }
        }
    }

    float out_a = cirrus.a + cumulus.a * (1.0 - cirrus.a);
    vec3 out_c = cirrus.rgb * cirrus.a + cumulus.rgb * cumulus.a * (1.0 - cirrus.a);
    if (out_a > 1e-5) out_c /= out_a;
    return vec4(out_c, out_a);
}

// the aerosol forward peak the sky-view LUT smears over its grid, restored
vec3 sk_aureole(vec3 ray, vec3 view, vec3 irradiance, float cos_theta) {
    const float cut = 0.9135;
    if (cos_theta <= cut) return vec3(0.0);
    float mie_od = hz_atmosphere.mie_scattering.x * 1.2 / max(0.055, ray.y + 0.055);
    float excess = max(0.0, sk_mie_phase(cos_theta) - sk_mie_phase(cut));
    return irradiance * cod_transmittance(view, ray) * (excess * mie_od * 4.2);
}

// the band-averaged radiance of an L2 irradiance-over-pi SH over cos(zenith) in [a, b], all azimuths
vec3 cod_sh_band_radiance(float a, float b) {
    float m1 = 0.5 * (a + b);
    float m2 = (b * b * b - a * a * a) / (3.0 * (b - a));
    vec3 l = hz_sky_sh.data[0].rgb * 0.282095 + hz_sky_sh.data[1].rgb * (1.5 * 0.488603 * m1) +
             hz_sky_sh.data[6].rgb * (4.0 * 0.315392 * (3.0 * m2 - 1.0));
    return max(l, vec3(0.0)) * hz_ambient_input.irradiance_scale;
}

void main() {
    MaterialFS mat = MaterialFS_unpack(hz_sky_draw.material_offset);
    g_sky_view_slot = mat.hz_sky_view_lut;
    g_transmittance_slot = mat.hz_sky_transmittance_lut;
    vec3 dir = normalize(v_dir);
    float pre = hz_scene.pre_exposure;

    // the engine's atmosphere (hz_atmosphere_sky), its disc and stars kept apart for the decks and the roll-off
    vec3 cam = hz_atmosphere.camera_position;
    float view_height = length(cam);
    float cos_zenith = dot(dir, cam / view_height);
    bool intersect_ground = sky_ray_sphere_nearest(cam, dir, vec3(0.0), hz_atmosphere.bottom_radius) >= 0.0;
    vec3 sun = hz_atmosphere.sun_direction;
    float px = length(fwidth(dir));
    float angle = atan(length(cross(dir, sun)), dot(dir, sun));
    float aw = max(0.5 * fwidth(angle), 1e-6);
    float r = hz_atmosphere.sun_angular_radius;
    float disk = intersect_ground ? 0.0 : 1.0 - smoothstep(r - aw, r + aw, angle);
    vec3 col = sky_view_luminance(dir) * hz_atmosphere.sun_illuminance * pre;
    vec3 disc = vec3(0.0);
    vec3 night = vec3(0.0);
    if (disk > 0.0 || (!intersect_ground && hz_atmosphere.stars.x > 0.0)) {
        vec3 t = cod_transmittance(cam, dir);
        disc = disk * t * hz_atmosphere.sun_disk_inv_solid_angle * hz_atmosphere.sun_illuminance * pre;
        if (hz_atmosphere.stars.x > 0.0)
            night = t * hz_sky_stars(dir, hz_atmosphere.stars_rotation, hz_atmosphere.stars.y, px) * hz_atmosphere.stars.x * pre;
    }

    // the dome: its viewer above the origin, so the decks stay put as the player moves
    float ground_r = hz_atmosphere.bottom_radius;
    vec3 view = vec3(0.0, ground_r + cod_sky.moon_irradiance.w, 0.0);
    vec3 sun_dir = cod_sky.sun.xyz, moon_dir = cod_sky.moon.xyz;
    vec3 sun_irr = cod_sky.sun_irradiance.xyz * pre, moon_irr = cod_sky.moon_irradiance.xyz * pre;
    vec3 amb_sky = hz_ambient_irradiance(vec3(0.0, 1.0, 0.0));
    vec3 amb_hor = cod_sh_band_radiance(-0.12, 0.35);

    col += sk_aureole(dir, view, sun_irr, dot(dir, sun_dir));
    col += sk_aureole(dir, view, moon_irr, dot(dir, moon_dir));

    vec3 p_low = vec3(0.0, ground_r + COD_SKY_CUMULUS_KM, 0.0);
    vec3 p_high = vec3(0.0, ground_r + COD_SKY_CIRRUS_KM, 0.0);
    vec4 cl = sk_clouds(dir, view, ground_r, sun_dir, sun_irr * cod_transmittance(p_low, sun_dir),
                        sun_irr * cod_transmittance(p_high, sun_dir), moon_dir, moon_irr * cod_transmittance(p_low, moon_dir),
                        moon_irr * cod_transmittance(p_high, moon_dir), amb_sky);
    // stars behind the decks: a deck thick enough to hide its own texture hides a point
    col += night * (1.0 - clamp(cl.a * 1.9, 0.0, 1.0));
    if (cl.a > 1.0e-4) {
        float bleed = 1.0 - smoothstep(0.0, 0.22, dir.y);
        col = mix(col, mix(cl.rgb, col, bleed * 0.82), cl.a);
    }

    float murk = cod_sky.moon.w * exp(-abs(dir.y) * 26.0);
    col = mix(col, amb_hor * 1.15, clamp(murk, 0.0, 0.85));

    // the power roll-off: the sunset glow becomes a gradient, not a plateau
    float knee = cod_sky.sun.w * pre;
    float l = max(cod_sky_lum(col), 1.0e-6 * pre);
    if (knee > 0.0 && l > knee)
        col *= pow(l / knee, cod_sky.sun_irradiance.w) * knee / l;

    // the disc after the roll-off: it alone clips and blooms
    col = min(col + disc * (1.0 - cl.a), vec3(SKY_HDR_MAX));
    if (hz_fog_input.mode != 0u && hz_fog_input.mode < 4u)
        col = mix(col, hz_fog_input.color, 1.0 - smoothstep(0.0, 0.25, cos_zenith));
    col = hz_fog_apply_sky(col, dir);
    HzVelocity vel = hz_velocity(vec4(dir, 0.0), vec4(dir, 0.0));
    hz_output_sky(col, vel);
}
@end
