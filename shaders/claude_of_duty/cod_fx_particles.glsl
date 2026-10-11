// Claude of Duty particles: one GPU material for every effect element, additive and lit alike. Each element is a row of the `table`
// selected by the emitter's user; a particle is a closed-form ballistic solution (v(t) = v0 e^-kt + g/k (1 - e^-kt)) plus decorrelated sine turbulence, so
// nothing is integrated and a frozen capture steps 0. Lit particles are shaded by a fake sphere normal bent by the sprite's own density gradient.
#include "common.glsl"
#include "particle_common.glsl"

// rows of one element in the table
#define CF_ROW_VEC4S 20u
#define CF_R_FORCES 0u   // drag, gravity, size curve, alpha curve
#define CF_R_C0 1u       // rgb, intensity
#define CF_R_C1 2u
#define CF_R_SIZE 3u     // begin min, max, end min, max
#define CF_R_SIZE2 4u    // end mode (1 = a ratio of the begin), stretch, turbulence amplitude, frequency
#define CF_R_ALPHA 5u    // alpha min, max, soft fade metres, tile
#define CF_R_CONE 6u     // half-angle, power, hollow pitch min, max
#define CF_R_SPEED 7u    // min, max, axis (0 normal, 1 alt, 2 up), flags
#define CF_R_NOFF 8u     // offset along the normal min, max, disc radius, height min
#define CF_R_VADD 9u     // world velocity added, height max
#define CF_R_ROT 10u     // mode (0 random, 1 fixed), value, spin min, max
#define CF_R_TIME 11u    // delay min, max, additive, lit
#define CF_R_SUN 12u     // sun-side tint base, gain, lobe root offset, offset along the direction min
#define CF_R_ASCALE 13u  // per-axis scale of the direction, offset along the direction max
#define CF_R_MISC 14u    // horizontal scatter m/s, hemisphere bias (< 0 off), clamp cosine (0 off), bounces
#define CF_R_BOUNCE 15u  // restitution, life min, max, intensity multiplier
#define CF_R_BCOL 16u    // colour a bounced spark cools to
#define CF_R_MORE2 18u   // intensity jitter, play gate
#define CF_R_MORE 17u   // second tile, its share, drag max, rotation jitter

#define CF_FLAG_FLICKER 1u
#define CF_FLAG_LOBE 2u
#define CF_FLAG_GAIN 4u        // intensity follows the burst's gain
#define CF_FLAG_EMBER 8u       // intensity follows the ember gain
#define CF_FLAG_EMBER_ALPHA 16u // opacity follows the ember gain
#define CF_FLAG_SPEED_DEBRIS 32u // speed follows the explosion's radius law 0.6 + R / 8
#define CF_FLAG_TURB_ABS 64u    // turbulence amplitude in metres, not scaled with the size
#define CF_FLAG_ALPHA_GAIN 128u // opacity follows the burst's gain

struct Particle {
    HZ_PARTICLE_PREFIX
    vec3 origin;
    float delay;    // seconds before it shows
    vec3 vel0;      // the segment's launch velocity
    float init;
    vec3 vel;       // velocity now (stretch, lobe heading)
    float t0;       // age at the segment start (a bounce restarts the curve)
    vec4 color;     // rgb HDR in display units, a opacity
    vec4 shape;     // x tile, y soft metres, z lit 0..1, w additive 0..1
    vec4 misc;      // x spin rate, y rotation at the segment start, z segment life, w bounces left
    vec4 draw;      // x size begin, y size end, z alpha multiplier, w turbulence phase
    vec4 look;      // x stretch, y lobe flag, z view flag, w cone seed
    vec4 tint;      // rgb the burst's tint times the sun-side scale, w the burst's gain
    vec4 extra;     // x turbulence amplitude with the burst's size scale
};

@vs
#define HZ_PS_RENDER_VS_SHADER
#include "particle_shaders.glsl"

out vec2 v_uv;
out vec2 v_q;
out vec4 v_color;
out float v_viewz;
flat out vec4 v_shape;

void main() {
    Billboard bb = particle_billboard(uint(gl_VertexIndex));
    Particle p = particle_load(bb.particle_idx);
    v_uv = vec2(0.0);
    v_q = vec2(0.0);
    v_color = vec4(0.0);
    v_viewz = 1.0;
    v_shape = vec4(0.0);
    if (p.color.a <= 0.0) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0); // behind the far plane: clipped
        return;
    }
    vec3 right = render_params.camera_right;
    vec3 up = render_params.camera_up;
    vec2 c = bb.offset * 0.5;
    vec2 off;
    float size = p.extra.w;
    if (p.look.x > 0.001) {
        // the sprite's +Y runs along screen-space velocity, lengthened by the speed
        vec2 sv = vec2(dot(p.vel, right), dot(p.vel, up));
        float sl = length(sv);
        vec2 along = sl > 1e-5 ? sv / sl : vec2(0.0, 1.0);
        vec2 perp = vec2(-along.y, along.x);
        float len = size * (1.0 + p.look.x * length(p.vel));
        off = along * (c.y * len) + perp * (c.x * size);
    } else {
        float rot = p.misc.y + p.misc.x * (p.max_life - p.life - p.t0);
        if (p.look.y > 0.5) {
            // a lobe's root-to-tip +X follows the screen angle of its world heading
            rot += atan(dot(p.vel0, up), dot(p.vel0, right));
        }
        float cs = cos(rot), sn = sin(rot);
        off = vec2(c.x * cs - c.y * sn, c.x * sn + c.y * cs) * size;
    }
    vec3 world = p.position + right * off.x + up * off.y;
    vec4 clip = render_params.view_proj * vec4(world, 1.0);
    gl_Position = clip;
    v_viewz = clip.w;
    v_q = off / max(size, 1e-4) * 2.0;
    float tile = p.shape.x;
    vec2 cell = vec2(mod(tile, 4.0), floor(tile * 0.25));
    v_uv = (cell + bb.uv) * 0.25;
    v_color = p.color;
    // x soft fade metres (0 off), y lit, z additive
    v_shape = vec4(p.look.z > 0.5 ? 0.0 : p.shape.y, p.shape.z, p.shape.w, 0.0);
}
@end

@fs
#undef HZ_PS_RENDER_VS_SHADER
#define HZ_PS_RENDER_FS_SHADER
#include "particle_shaders.glsl"

in vec2 v_uv;
in vec2 v_q;
in vec4 v_color;
in float v_viewz;
flat in vec4 v_shape;
out vec4 frag_color;

@material
struct MaterialCfx {
    @texture2D uint albedo;
    float unit;        // display units of one scene unit: scene_per_unit x pre-exposure
    vec3 sun_dir;      // toward the sun, world
    vec3 sun_color;    // scene units
    vec3 amb_top;
    vec3 amb_bottom;
    // CF_ROW_VEC4S per element; the material parser reads only a number literal
    vec4 table[4000];
};

void main() {
    uint albedo_idx = MaterialCfx_albedo(HZ_PS_MATERIAL_OFFSET);
    vec4 tex = texture_sample(albedo, albedo_idx, v_uv);
    float a = tex.a * v_color.a;
    if (a < 0.0035) discard;
    vec3 c = v_color.rgb * tex.rgb;

    if (v_shape.y > 0.0) {
        uint mo = HZ_PS_MATERIAL_OFFSET;
        // a sphere normal in view space, bent by the sprite's own density gradient: a soft blob gets legible internal form
        float rr = dot(v_q, v_q);
        vec3 nrm = vec3(v_q, sqrt(max(0.03, 1.0 - rr)));
        nrm = normalize(nrm - vec3(dFdx(tex.r), dFdy(tex.r), 0.0) * 7.0);
        vec3 right = render_params.camera_right;
        vec3 up = render_params.camera_up;
        vec3 back_axis = cross(right, up);
        vec3 sun = MaterialCfx_sun_dir(mo);
        vec3 sun_v = vec3(dot(sun, right), dot(sun, up), dot(sun, back_axis));
        vec3 up_v = vec3(right.y, up.y, back_axis.y);
        float ndl = dot(nrm, sun_v);
        float wrap = max(0.0, (ndl + 0.42) / 1.42);
        float back = max(0.0, -ndl);
        float upk = 0.5 + 0.5 * dot(nrm, up_v);
        // irradiance to radiance: the 1/PI keeps a dust puff at the exposure of the wall behind it
        vec3 lit = (mix(MaterialCfx_amb_bottom(mo), MaterialCfx_amb_top(mo), upk) +
                    MaterialCfx_sun_color(mo) * (wrap * 0.9 + pow(back, 4.0) * 0.55)) * 0.3183099;
        lit *= mix(1.0, 0.55, clamp(tex.a * 1.1, 0.0, 1.0)); // self-shadowing by density
        // v_color already carries the display units; the lighting is in scene units of the same scale
        c *= mix(vec3(1.0), lit, v_shape.y);
    }

    if (v_shape.x > 0.0) {
        vec2 screen_uv = gl_FragCoord.xy * render_params.inv_viewport;
        a *= ps_soft_fade(screen_uv, gl_FragCoord.z, 1.0 / v_shape.x);
    }
    // never let a sprite smear across the lens
    a *= clamp((v_viewz - 0.05) / 0.2, 0.0, 1.0);
    // premultiplied against (ONE, ONE_MINUS_SRC_ALPHA): the additive share removes the destination-preserving term
    frag_color = vec4(c * a, min(a, 1.0) * (1.0 - clamp(v_shape.z, 0.0, 1.0)));
}
@end

@cs
#undef HZ_PS_RENDER_FS_SHADER
#define HZ_PS_SIM_SHADER
#include "particle_shaders.glsl"

vec4 cf_row(uint mo, uint elem, uint k) { return MaterialCfx_table(mo, int(elem * CF_ROW_VEC4S + k)); }

// an orthonormal pair around a unit axis
void cf_basis(vec3 n, out vec3 t, out vec3 b) {
    vec3 a = abs(n.y) > 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
    t = normalize(cross(a, n));
    b = cross(n, t);
}

// a direction in a cone of half-angle `spread` about a unit axis; `power` > 1 biases toward the axis. A pitch band makes it hollow
vec3 cf_cone(vec3 axis, float spread, float power, vec2 pitch, float u, float phi01) {
    vec3 t, b;
    cf_basis(axis, t, b);
    float ang = pitch.y > 0.0 ? mix(pitch.x, pitch.y, u) : spread * pow(u, power);
    float cs = cos(ang);
    float sn = sqrt(max(0.0, 1.0 - cs * cs));
    float phi = phi01 * 6.28318530718;
    return axis * cs + t * (cos(phi) * sn) + b * (sin(phi) * sn);
}

// a direction out of the surface: mirror whatever points into it, keeping a minimum cosine against the normal
vec3 cf_toward_hemi(vec3 d, vec3 n, float bias) {
    float dn = dot(d, n);
    if (dn < bias) {
        float k = bias - dn;
        d += n * (k - 2.0 * min(0.0, dn));
        d = normalize(d);
    }
    return d;
}

// no wider than cos_max about the normal, keeping the tangential direction it had
vec3 cf_clamp_cone(vec3 d, vec3 n, float cos_max) {
    float dn = dot(d, n);
    if (dn >= cos_max) return d;
    vec3 tan_v = d - n * dn;
    float tl = length(tan_v);
    if (tl < 1e-5) return n;
    return n * cos_max + tan_v * (sqrt(max(0.0, 1.0 - cos_max * cos_max)) / tl);
}

vec3 cf_disc(vec3 n, float radius, float u, float v) {
    vec3 t, b;
    cf_basis(n, t, b);
    float rr = sqrt(u) * radius;
    float a = v * 6.28318530718;
    return t * (cos(a) * rr) + b * (sin(a) * rr);
}

void simulate(inout Particle p, EmitterData ed, uint particle_index, float dt) {
    uint mo = ed.material_offset;
    uint el = ed.user;
    vec4 forces = cf_row(mo, el, CF_R_FORCES);
    vec4 timing = cf_row(mo, el, CF_R_TIME);

    if (p.init == 0.0) {
        p.init = 1.0;
        uint seed = particle_index * 1099087573u + uint(fract(ed.burst_user.w) * 16777216.0) * 2654435761u + 12345u;
        float r0 = rand_float(seed + 1u), r1 = rand_float(seed + 2u), r2 = rand_float(seed + 3u), r3 = rand_float(seed + 4u);
        float r4 = rand_float(seed + 5u), r5 = rand_float(seed + 6u), r6 = rand_float(seed + 7u), r7 = rand_float(seed + 8u);
        float r8 = rand_float(seed + 9u), r9 = rand_float(seed + 10u), r10 = rand_float(seed + 11u), r11 = rand_float(seed + 12u);

        vec4 cone_row = cf_row(mo, el, CF_R_CONE);
        vec4 speed_row = cf_row(mo, el, CF_R_SPEED);
        vec4 noff = cf_row(mo, el, CF_R_NOFF);
        vec4 vadd = cf_row(mo, el, CF_R_VADD);
        vec4 rot_row = cf_row(mo, el, CF_R_ROT);
        vec4 sun_row = cf_row(mo, el, CF_R_SUN);
        vec4 ascale = cf_row(mo, el, CF_R_ASCALE);
        vec4 misc = cf_row(mo, el, CF_R_MISC);
        vec4 size_row = cf_row(mo, el, CF_R_SIZE);
        vec4 size2 = cf_row(mo, el, CF_R_SIZE2);
        vec4 alpha_row = cf_row(mo, el, CF_R_ALPHA);

        // the burst's frame: its axis is the surface normal; the alternate axis (reflected ray, bore, spray) rides in the burst's user
        vec3 normal = normalize(quat_rotate(ed.rotation, vec3(0.0, 0.0, 1.0)));
        vec3 alt = ed.burst_user.xyz;
        alt = dot(alt, alt) > 1e-6 ? normalize(alt) : normal;
        vec3 axis = speed_row.z < 0.5 ? normal : speed_row.z < 1.5 ? alt : speed_row.z < 2.5 ? vec3(0.0, 1.0, 0.0) : -alt;

        vec3 dir = cf_cone(axis, cone_row.x, cone_row.y, cone_row.zw, r0, r1);
        if (misc.y >= 0.0) dir = cf_toward_hemi(dir, normal, misc.y);
        if (misc.z > 0.0) dir = cf_clamp_cone(dir, normal, misc.z);
        dir = normalize(dir * ascale.xyz);
        uint flags = uint(speed_row.w + 0.5);
        float size_mul = ed.burst_scale;
        float radius_mul = ed.shape_radius;
        float height_mul = ed.spawn.w;
        float speed = mix(speed_row.x, speed_row.y, r2) * ed.start_speed;
        if ((flags & CF_FLAG_SPEED_DEBRIS) != 0u) speed *= 0.6 + radius_mul * 0.125;
        vec3 scatter = vec3(r3 - 0.5, 0.0, r4 - 0.5) * 2.0 * misc.x;
        p.vel0 = dir * speed + vadd.xyz + scatter;

        vec3 pos = ed.position; // the burst's origin: the emit stage's shape sample is not used
        pos += normal * mix(noff.x, noff.y, r5);
        pos += cf_disc(normal, noff.z * radius_mul, r6, r7);
        pos += vec3(0.0, mix(noff.w, vadd.w, r8) * height_mul, 0.0);
        pos += dir * mix(sun_row.w, ascale.w, r9) * radius_mul;

        float s0 = mix(size_row.x, size_row.y, r10) * size_mul;
        float s1 = size2.x > 0.5 ? s0 * size_row.z : mix(size_row.z, size_row.w, r11) * size_mul;
        // a lobe's root sits on the crown: slide its centre out along its heading
        pos += normalize(p.vel0 + vec3(1e-6)) * (sun_row.z * s1);
        p.origin = pos;
        p.position = pos;
        p.draw = vec4(s0, s1, mix(alpha_row.x, alpha_row.y, r11 * 0.5 + r3 * 0.5), r4);

        float delay = mix(timing.x, timing.y, r5);
        p.delay = delay;
        p.t0 = 0.0;
        vec4 more = cf_row(mo, el, CF_R_MORE);
        float rot0 = rot_row.x < 0.5 ? r10 * 6.28318530718 : rot_row.y + (rand_float(seed + 15u) - 0.5) * 2.0 * more.w;
        p.misc = vec4(mix(rot_row.z, rot_row.w, r9), rot0, p.max_life, misc.w);
        p.look = vec4(size2.y, (flags & CF_FLAG_LOBE) != 0u ? 1.0 : 0.0, ed.burst_user.w >= 2.0 ? 1.0 : 0.0, fract(ed.burst_user.w));
        float lit_tint = (sun_row.x + sun_row.y) > 0.0 ? sun_row.x + sun_row.y * max(0.0, dot(dir, MaterialCfx_sun_dir(mo))) : 1.0;
        vec4 burst_color = unpack_color_rgba8(ed.color);
        p.tint = vec4(burst_color.rgb * lit_tint, burst_color.a * 2.0);
        vec4 more2 = cf_row(mo, el, CF_R_MORE2);
        p.extra = vec4(size2.z * ((flags & CF_FLAG_TURB_ABS) != 0u ? 1.0 : size_mul), mix(forces.x, more.z, rand_float(seed + 13u)), 1.0 + (rand_float(seed + 16u) - 0.5) * 2.0 * more2.x, 0.0);
        // a gated part exists in only a share of the plays, chosen by the burst's seed
        if (more2.y > 0.0 && fract(ed.burst_user.w * 7.31 + 0.137) > more2.y) p.draw.z = 0.0;
        float tile = rand_float(seed + 14u) < more.y ? more.x : alpha_row.w;
        p.shape = vec4(tile, alpha_row.z, timing.w, timing.z);
        p.vel = p.vel0;
        p.color = vec4(0.0);
        p.size = s1 > s0 ? s1 : s0;
    }

    // a delayed particle waits: its life holds, and it draws nothing
    if (p.delay > 0.0) {
        p.delay -= dt;
        p.color = vec4(0.0);
        if (p.delay > 0.0) {
            p.life = p.max_life;
            return;
        }
    }

    p.life -= dt;
    if (p.life <= 0.0) {
        kill_particle(p);
        return;
    }
    float age = p.max_life - p.life;
    float t = max(age - p.t0, 0.0);
    float seg_life = max(p.misc.z, 1e-3);
    float n = clamp(t / seg_life, 0.0, 1.0);

    float k = max(p.extra.y, 0.02);
    float e = exp(-k * t);
    vec3 gk = vec3(0.0, forces.y, 0.0) / k;
    vec3 pos = p.origin + (p.vel0 - gk) * ((1.0 - e) / k) + gk * t;
    vec3 vel = p.vel0 * e + gk * (1.0 - e);

    // sparks that reach the floor skitter on: the curve restarts from the contact with the energy that is left
    vec4 misc = cf_row(mo, el, CF_R_MISC);
    if (p.misc.w > 0.5 && misc.w > 0.0) {
        float floor_y = particle_floor_height(pos, p.origin.y + 0.05);
        if (pos.y < floor_y && vel.y < 0.0) {
            vec4 bounce = cf_row(mo, el, CF_R_BOUNCE);
            uint seed = particle_index * 2654435761u + uint(age * 977.0);
            vec3 reflected = vec3(vel.x, -vel.y, vel.z) * bounce.x;
            reflected.x += (rand_float(seed + 1u) - 0.5) * 0.5 * length(vel) * bounce.x;
            reflected.z += (rand_float(seed + 2u) - 0.5) * 0.5 * length(vel) * bounce.x;
            reflected.y = abs(reflected.y) * 0.8 + 0.25;
            p.origin = vec3(pos.x, floor_y + 0.01, pos.z);
            p.vel0 = reflected;
            p.t0 = age;
            p.misc.z = mix(bounce.y, bounce.z, rand_float(seed + 3u));
            p.max_life = age + p.misc.z;
            p.life = p.misc.z;
            p.misc.w -= 1.0;
            p.draw.x *= 0.8;
            p.draw.y *= 0.8;
            p.look.w = 1.0 + p.look.w; // marks a bounced segment: the cooler colour and the dimmer intensity
            pos = p.origin;
            vel = p.vel0;
            t = 0.0;
            n = 0.0;
            seg_life = p.misc.z;
        }
    }

    // turbulence: three decorrelated sines that grow in, so a particle never teleports on its first frame
    vec4 size2 = cf_row(mo, el, CF_R_SIZE2);
    float ph = p.draw.w * 6.28318530718;
    float f = size2.w;
    float amp = p.extra.x * smoothstep(0.0, 0.4, n);
    if (amp > 0.0) {
        pos += vec3(sin(t * f * 1.13 + ph), sin(t * f * 0.79 + ph * 2.1), cos(t * f * 1.31 + ph * 1.7)) * amp;
        vel += vec3(cos(t * f * 1.13 + ph), cos(t * f * 0.79 + ph * 2.1), -sin(t * f * 1.31 + ph * 1.7)) * (amp * f);
    }
    p.position = pos;
    p.vel = vel;

    // extra.w is the quad width; p.size is the cull radius, which holds the streak's length too
    p.extra.w = mix(p.draw.x, p.draw.y, pow(n, max(forces.z, 0.02)));
    p.size = p.extra.w * (size2.y > 0.001 ? 1.0 + size2.y * length(vel) : 1.0);

    vec4 c0 = cf_row(mo, el, CF_R_C0);
    vec4 c1 = cf_row(mo, el, CF_R_C1);
    vec3 col_a = c0.rgb;
    vec3 col_b = c1.rgb;
    float i_a = c0.w * p.extra.z;
    float i_b = c1.w;
    if (p.look.w >= 1.0) {
        vec4 bcol = cf_row(mo, el, CF_R_BCOL);
        col_a = bcol.rgb;
        i_a *= cf_row(mo, el, CF_R_BOUNCE).w;
    }
    vec3 col = mix(col_a, col_b, n) * p.tint.rgb;
    uint flags = uint(cf_row(mo, el, CF_R_SPEED).w + 0.5);
    // the burst's gain scales what the element opts into; an ember carries the quieter 0.22 + 0.78 min(1, gain)
    float gain = p.tint.w;
    float ember = 0.22 + 0.78 * min(1.0, gain);
    float inten = mix(i_a, i_b, n * n) * ((flags & CF_FLAG_GAIN) != 0u ? gain : 1.0) * ((flags & CF_FLAG_EMBER) != 0u ? ember : 1.0);
    if ((flags & CF_FLAG_FLICKER) != 0u) inten *= 0.72 + 0.28 * sin(t * 63.0 + ph * 9.0);
    float alpha = p.draw.z * pow(max(1.0 - n, 0.0), max(forces.w, 0.02)) * smoothstep(0.0, 0.045, n);
    if ((flags & CF_FLAG_EMBER_ALPHA) != 0u) alpha *= ember;
    if ((flags & CF_FLAG_ALPHA_GAIN) != 0u) alpha *= gain;
    p.color = vec4(col * inten * MaterialCfx_unit(mo), alpha);
}
#include "particle_sim_main.glsl"
@end
