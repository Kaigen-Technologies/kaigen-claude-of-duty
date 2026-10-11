// The Claude of Duty surface material: projected texture sets with parallax, detail, macro variation, weathering, vertex masks and the look's lighting budgets.
// Keywords: COD_TRIPLANAR / COD_MESH_UV (neither: planar), COD_ALPHA_MASK, COD_LOCAL (object-space triplanar), COD_VIEWMODEL (the view rig), plus the engine's HZ_RENDERER_*.
#include "renderer.glsl"
#include "cod_look.glsl"

@vs
#define HZ_RENDERER_VERTEX_SHADER
#include "renderer_shaders.glsl"
invariant gl_Position;
out vec2 v_uv;
out vec4 v_color;
flat out uint v_material_offset;
#ifdef HZ_RENDERER_DECALS
flat out uint v_decal_mask;
#endif
out vec3 v_normal;
out vec3 v_world_pos;
out float v_view_depth;
#ifdef COD_LOCAL
out vec3 v_opos;
out vec3 v_onrm;
out vec3 v_r0;
out vec3 v_r1;
out vec3 v_r2;
#endif
#ifdef HZ_RENDERER_GBUFFER
out vec4 v_clip_cur;
out vec4 v_clip_prev;
#endif

void main() {
    VertexInstanceData v = unpack_instance_data();
#ifdef HZ_RENDERER_GBUFFER
    vec3 bind_pos = v.pos;
#endif
    vec4 world_pos = v.model * vec4(v.pos, 1.0);
    gl_Position = hz_instance_clip(v, world_pos);
    v_uv = v.uv;
    v_color = v.color;
    v_material_offset = v.material_offset;
#ifdef HZ_RENDERER_DECALS
    v_decal_mask = v.decal_mask;
#endif
    v_normal = hz_normal_to_world(v.model, v.normal);
    v_world_pos = world_pos.xyz;
    v_view_depth = -(hz_scene.view * world_pos).z;
#ifdef COD_LOCAL
    v_opos = v.pos;
    v_onrm = normalize(v.normal);
    v_r0 = normalize(v.model[0].xyz);
    v_r1 = normalize(v.model[1].xyz);
    v_r2 = normalize(v.model[2].xyz);
#endif
#ifdef HZ_RENDERER_GBUFFER
    HzVelocity vel = hz_instance_velocity(v, world_pos, hz_world_pos_prev(bind_pos, v));
    v_clip_cur = vel.clip_cur;
    v_clip_prev = vel.clip_prev;
#endif
}
@end

@fs
#undef HZ_RENDERER_VERTEX_SHADER
#define HZ_RENDERER_FRAGMENT_SHADER
#define HZ_RENDERER_LIGHTING_BASIC
#define HZ_RENDERER_OUTPUT
#define HZ_RENDERER_ANISO
#include "renderer_shaders.glsl"
#include "cod_look.glsl"
#ifndef COD_VIEWMODEL
// published by cod_post_contact.c: xy 1 / the contact texture's size
uniform _CodContact { vec4 cod_contact_size; };
#endif
in vec2 v_uv;
in vec4 v_color;
flat in uint v_material_offset;
#ifdef HZ_RENDERER_DECALS
flat in uint v_decal_mask;
#endif
in vec3 v_normal;
in vec3 v_world_pos;
in float v_view_depth;
#ifdef COD_LOCAL
in vec3 v_opos;
in vec3 v_onrm;
in vec3 v_r0;
in vec3 v_r1;
in vec3 v_r2;
#endif
#ifdef HZ_RENDERER_GBUFFER
in vec4 v_clip_cur;
in vec4 v_clip_prev;
#endif

@material
struct CodSurfaceMaterial {
    vec4 tile;        // xy tiles per metre (uv multiplier for mesh uv), zw offset
    vec4 detail_p;    // x detail tiles per base tile, y normal amount, z albedo amount, w fade distance
    vec4 macro_p;     // x world scale, y albedo amount, z roughness amount, w hue amount
    vec4 macro_big;   // x contrast, y big-band amount, z big-band scale
    vec4 patch_p;     // x coverage, y cell metres, z albedo delta, w roughness delta
    vec4 cloth_p;     // x transmission, y underside darkening, z fold amount
    vec4 parallax_p;  // x depth (m), y fade start, z fade end, w max layers
    vec4 weather_p;   // x dust, y streak, z splash height, w cavity grime
    vec4 wear_p;      // x wear, y grime, z vertex AO amounts
    vec4 tint_col;    // linear
    vec4 dust_col;
    vec4 grime_col;
    vec4 rust_col;
    vec4 wear_col;
    vec4 wear_mat;    // x roughness, y metalness, w tint amount where the wear mask is 1
    vec4 rough_p;     // x scale, y offset, z de-tile amount, w minimum
    vec4 misc_a;      // x normal strength, y ground height, z AO strength, w macro relief
    vec4 misc_b;      // x vertex masks on, y opacity, z reflection strength
    vec4 emissive_col; // linear radiance, cd/m2
    @texture2D @white
    uint albedo;      // sRGB albedo, height in alpha
    @texture2D @normal
    uint normal_map;  // tangent normal, xy in rg
    @texture2D @white
    uint orm_map;     // occlusion, roughness, metalness
    @texture2DArray @white
    uint detail_arr;  // layer 0: rgb micro albedo variation, a micro height; layer 1: the micro normal, xy in rg (only read while detail_p.y > 0)
    @texture2D @white
    uint macro_tex;   // four low-frequency bands
    @texture2D @white
    uint cod_contact; // published by cod_post_contact: x the key light's contact shadow, white while off
};

#define COD_PI 3.14159265

// the micro normal (detail_arr layer 1); a material without one (amount 0) skips the fetch
vec3 cod_detail_normal(uint idx, vec2 uv, vec2 dx, vec2 dy, float amount) {
    if (amount <= 0.0) return vec3(0.0, 0.0, 1.0);
    return unpack_normal_bc5(texture2d_array_sample_grad_aniso(detail_arr, idx, uv, 1, dx, dy).rg);
}

float owHash11(float x) {
    float p = fract(x * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

// runoff staining below a source: x 0..1 along a discrete column, y the column's random, z distance below the source
vec3 owRunoff(float sAxis, float y, float wobble) {
    float u = sAxis * 1.55;
    float cell = floor(u);
    float lat = fract(u);
    float r0 = owHash11(cell * 1.37 + 3.1);
    float r1 = owHash11(cell * 2.71 + 11.7);
    float srcAmt = smoothstep(0.30, 0.62, r0) * (0.55 + 0.45 * r1);
    float bell = sin(lat * 3.14159265);
    srcAmt *= bell * bell * (0.8 + 0.45 * r0);
    const float SPACING = 2.85;
    float jitter = r1 * 1.2 + r0 * 0.5;
    float srcY = (floor((y + jitter) / SPACING) + 1.0) * SPACING - jitter + wobble * 0.2;
    float below = srcY - y;
    float run = smoothstep(0.0, 0.15, below) * (1.0 - smoothstep(0.15, 1.65, below));
    return vec3(clamp(run * srcAmt, 0.0, 1.0), r1, below);
}

mat3 owTangentFrame(vec3 eye, vec3 n, vec2 uv) {
    vec3 q0 = dFdx(eye), q1 = dFdy(eye);
    vec2 s0 = dFdx(uv), s1 = dFdy(uv);
    vec3 q1p = cross(q1, n);
    vec3 q0p = cross(n, q0);
    vec3 T = q1p * s0.x + q0p * s1.x;
    vec3 B = q1p * s0.y + q0p * s1.y;
    float det = max(dot(T, T), dot(B, B));
    float sc = (det == 0.0) ? 0.0 : inversesqrt(det);
    return mat3(T * sc, B * sc, n);
}

struct OwFrame {
    vec2 uv;
    vec3 T;
    vec3 B;
    vec3 N;
};

OwFrame owAxisFrame(vec3 p, vec3 n, int axis, vec4 tile) {
    vec3 s = mix(vec3(-1.0), vec3(1.0), step(0.0, n));
    OwFrame f;
    if (axis == 0) {
        f.uv = vec2(-p.z * s.x, p.y);
        f.T = vec3(0.0, 0.0, -s.x); f.B = vec3(0.0, 1.0, 0.0); f.N = vec3(s.x, 0.0, 0.0);
    } else if (axis == 1) {
        f.uv = vec2(p.x, -p.z * s.y);
        f.T = vec3(1.0, 0.0, 0.0); f.B = vec3(0.0, 0.0, -s.y); f.N = vec3(0.0, s.y, 0.0);
    } else {
        f.uv = vec2(p.x * s.z, p.y);
        f.T = vec3(s.z, 0.0, 0.0); f.B = vec3(0.0, 1.0, 0.0); f.N = vec3(0.0, 0.0, s.z);
    }
    f.uv = f.uv * tile.xy + tile.zw;
    return f;
}

// re-anchor an axis frame onto the true interpolated normal
void owOrthonormalise(inout OwFrame f, vec3 n) {
    f.N = n;
    f.T = normalize(f.T - n * dot(n, f.T));
    f.B = cross(n, f.T);
}

// height-preserving blend of two texture samples (kills the mushy 50% lerp)
void owHeightBlend(inout vec4 a, inout vec3 ormA, inout vec3 nA, vec4 b, vec3 ormB, vec3 nB, float t) {
    float wa = (1.0 - t) + a.a * 0.6;
    float wb = t + b.a * 0.6;
    float k = max(wa, wb) - 0.18;
    wa = max(wa - k, 0.0);
    wb = max(wb - k, 0.0);
    float inv = 1.0 / max(wa + wb, 1e-4);
    a = (a * wa + b * wb) * inv;
    ormA = (ormA * wa + ormB * wb) * inv;
    nA = normalize((nA * wa + nB * wb) * inv);
}

// parallax occlusion mapping: marches the height in albedo.a; layers follow the grazing angle and the effect fades with distance
vec2 owPOM(uint alb, vec2 uv, vec3 vt, vec2 uvdx, vec2 uvdy, float depth, float fade, float max_layers) {
    if (depth <= 0.0 || fade <= 0.001) return uv;
    float nl = mix(max_layers, 8.0, clamp(abs(vt.z), 0.0, 1.0));
    nl = max(nl * fade, 4.0);
    float layer = 1.0 / nl;
    vec2 P = (vt.xy / max(abs(vt.z), 0.30)) * depth * fade;
    vec2 dUv = P * layer;

    float cur = 0.0;
    vec2 c = uv;
    float d = 1.0 - texture_sample_grad_aniso(albedo, alb, c, uvdx, uvdy).a;
    for (int i = 0; i < 48; i++) {
        if (cur >= d || float(i) >= nl) break;
        c -= dUv;
        d = 1.0 - texture_sample_grad_aniso(albedo, alb, c, uvdx, uvdy).a;
        cur += layer;
    }
    vec2 prev = c + dUv;
    float after = d - cur;
    float before = (1.0 - texture_sample_grad_aniso(albedo, alb, prev, uvdx, uvdy).a) - cur + layer;
    float w = clamp(after / max(after - before, 1e-4), 0.0, 1.0);
    return mix(c, prev, w);
}

// 1 outdoors, down to indirect.y deep inside a coarse interior volume (level-space footprints, depth-feathered)
float owInteriorGate(vec3 worldPos, float ao) {
    float indoor = 0.0;
    if (cod_look.indirect.z > 0.5) {
        float lx = worldPos.x * cod_look.room_xf.x + worldPos.z * cod_look.room_xf.y + cod_look.room_xf.z;
        float lz = -worldPos.x * cod_look.room_xf.y + worldPos.z * cod_look.room_xf.x + cod_look.room_xf.w;
        int n = int(cod_look.indirect.z);
        for (int i = 0; i < COD_LOOK_ROOMS; i++) {
            if (i >= n) break;
            vec4 r = cod_look.rooms[i];
            vec4 ry = cod_look.rooms_y[i];
            float d = min(min(r.z - abs(lx - r.x), r.w - abs(lz - r.y)), min(worldPos.y - ry.x, ry.y - worldPos.y));
            indoor = max(indoor, smoothstep(0.06, 0.30, d));
        }
    }
    float aoGate = mix(1.0, smoothstep(0.45, 0.98, ao), 0.6);
    float g = min(1.0 - indoor, aoGate);
    return mix(cod_look.indirect.y, 1.0, clamp(g, 0.0, 1.0));
}

// wrapped diffuse from the anti-sun hemisphere: the reflected street key
float owSunBounce(vec3 worldNormal) {
    vec3 anti = normalize(vec3(-cod_look.sun_dir.x, 0.28, -cod_look.sun_dir.z) + vec3(1e-4));
    return clamp((dot(worldNormal, anti) + 0.12) / 1.12, 0.0, 1.0);
}

float owSpecularOcclusion(float ao, float rough) {
    float r2 = rough * rough;
    return clamp(pow(max(ao, 0.0), 1.0 + r2 * 2.0), 0.0, 1.0);
}

#ifdef COD_VIEWMODEL
// a view-space direction the rig's light arrives from, in world space
vec3 cod_view_dir(vec3 d) { return normalize(transpose(mat3(hz_scene.view)) * normalize(d)); }

// one rig light: lux (pre-exposed) arriving from a view-space direction, the engine's lux/pi convention
vec3 cod_view_light(SurfaceInput surface, LightingInput li, vec3 lux, vec3 dir_view) {
    return pbr_lighting_mobile(surface.albedo, surface.metallic, surface.smoothness, li.world_normal, li.world_position, li.camera_pos,
                               cod_view_dir(dir_view), lux * (1.0 / COD_PI));
}

// the viewmodel's own lights: the 3-point rig, the warm bounce, the hemisphere and the view flash pool
// unshadowed: skips every light not aligned with the sun, and the rig key never is
vec3 cod_view_direct(SurfaceInput surface, LightingInput li) {
    vec3 lit = cod_view_light(surface, li, cod_look.view_key.xyz, vec3(-0.45, 0.75, 0.55));
    lit += cod_view_light(surface, li, cod_look.view_fill.xyz, vec3(0.6, -0.15, 0.5));
    lit += cod_view_light(surface, li, cod_look.view_rim.xyz, vec3(0.2, 0.35, -0.9));
    lit += cod_view_light(surface, li, cod_look.view_bounce.xyz, vec3(-0.2, -0.86, 0.47));
    float hemi_t = clamp(li.world_normal.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 diffuse_color = surface.albedo * (1.0 - surface.metallic);
    lit += diffuse_color * mix(cod_look.view_hemi_ground.xyz, cod_look.view_hemi_sky.xyz, hemi_t) * (1.0 / COD_PI);
    for (int i = 0; i < COD_LOOK_VIEW_FLASHES; i++) {
        vec3 cd = cod_look.view_flash_color[i].xyz;
        if (max(cd.x, max(cd.y, cd.z)) <= 0.0) continue;
        vec3 to_light = cod_look.view_flash[i].xyz - li.world_position;
        float dist = length(to_light);
        // punctual falloff: inverse square with a smooth window to the cutoff
        float win = clamp(1.0 - pow(dist / cod_look.view_flash[i].w, 4.0), 0.0, 1.0);
        float atten = win * win / max(dist * dist, 0.01);
        lit += pbr_lighting_mobile(surface.albedo, surface.metallic, surface.smoothness, li.world_normal, li.world_position, li.camera_pos,
                                   to_light / max(dist, 1e-4), cd * (atten / COD_PI));
    }
    return lit;
}
#endif

// the shade step: hz_light's structure with the look's budgets on the key light, the indirect terms and the interior gate
vec3 cod_light(HzSurface s, vec3 world_pos, float view_depth, float bake_ao, float cloth_trans, float cloth_ao, float env_scale, float contact) {
    SurfaceInput surface = SurfaceInput(s.albedo, s.metallic, s.smoothness);
    LightingInput li = LightingInput(world_pos, s.normal, hz_scene.camera_pos);

    // neutral block (not yet published): the engine's own key and indirect
    vec3 key_gain = cod_look.key_gain.xyz;
    if (all(equal(key_gain, vec3(0.0)))) key_gain = vec3(1.0);
    float indirect_budget = cod_look.indirect.x == 0.0 ? 1.0 : cod_look.indirect.x;
    float vis_floor = cod_look.indirect.w == 0.0 ? 0.25 : cod_look.indirect.w;

    float ao_ss = 1.0;
#ifdef HZ_RENDERER_GTAO
    ao_ss = mix(1.0, max(hz_ao(gl_FragCoord.xy), vis_floor), 1.0);
#endif
    float ao_bake = clamp(bake_ao, 0.0, 1.0);

#ifdef COD_VIEWMODEL
    vec3 lit = cod_view_direct(surface, li) * mix(1.0, ao_ss, cod_look.key_gain.w);
#else
#ifdef HZ_RENDERER_SHADOWS
    float shadow_factor = hz_shadow(world_pos);
    vec3 shadow_color = hz_shadow_input.shadow_color;
#else
    float shadow_factor = 1.0;
    vec3 shadow_color = vec3(0.0);
#endif
    // light 0 takes the contact shadow, key gain and direct AO; a runtime-bounded loop keeps one BRDF copy (the unrolled engine helper inlined eight)
    vec3 lit = vec3(0.0);
    uint dir_count = min(hz_scene.lights.directional_count, 4u);
    for (uint i = 0u; i < dir_count; i++) {
        DirectionalLight dl = hz_scene.lights.directional[i];
        vec3 direct = pbr_lighting_mobile(surface.albedo, surface.metallic, surface.smoothness, li.world_normal, li.world_position,
                                          li.camera_pos, -dl.direction, dl.color * dl.intensity);
        vec3 l = mix(shadow_color * direct, direct, i == 0u ? shadow_factor * contact : shadow_factor);
        lit += i == 0u ? l * key_gain * mix(1.0, ao_ss, cod_look.key_gain.w) : l;
    }

#ifdef HZ_RENDERER_LIGHTING_CLUSTERS
    HZ_CLUSTER_LOOP_BEGIN(view_depth)
        lit += pbr_point_light(surface, li, light);
    HZ_CLUSTER_LOOP_END
#endif
#endif

    // fabric transmission: the beam landing on the face we are not looking at, forward-scattered toward the eye
    if (cloth_trans > 0.0) {
        vec3 V = normalize(hz_scene.camera_pos - world_pos);
        vec3 trans = vec3(0.0);
        for (int i = 0; i < 4; i++) {
            if (uint(i) >= hz_scene.lights.directional_count) break;
            DirectionalLight dl = hz_scene.lights.directional[i];
            vec3 to_light = -normalize(dl.direction);
            float back_lit = max(0.0, -dot(s.normal, to_light));
            float fwd = max(0.0, dot(V, -to_light));
            vec3 c = dl.color * dl.intensity * (i == 0 ? key_gain : vec3(1.0));
            trans += c * (back_lit * (0.30 + 0.90 * fwd * fwd));
        }
        lit += trans * s.albedo * (1.0 - s.metallic) * (cloth_trans * cloth_ao * COD_PI);
    }

    // indirect diffuse: the environment, GTAO multi-bounce, the interior gate, the two-band hemispheric fill, the sun bounce
#ifdef COD_VIEWMODEL
    // the gate is a world volume test and the gun sits at the eye: the viewmodel skips it
    float indoor = 1.0;
    // scene.environmentIntensity overrides the material's for the view scene
    env_scale = cod_look.view_fill.w;
    indirect_budget *= cod_look.view_fill.w;
#else
    float indoor = owInteriorGate(world_pos, ao_ss);
#endif
    vec3 bounce = hz_gtao_multibounce(ao_ss, s.albedo);
    vec3 diffuse_color = s.albedo * (1.0 - s.metallic);
    // no scene ambient: the indirect is the environment and the fills alone (its hemisphere light is the viewmodel's)
    vec3 indirect = vec3(0.0);
#ifdef HZ_RENDERER_IBL
    indirect += hz_ambient_diffuse(surface, li) * bounce * (indirect_budget * indoor);
#endif
    float fill_ao = sqrt(max(ao_ss, 0.0));
    float up = clamp(s.normal.y, -1.0, 1.0);
    float sky_gate = smoothstep(cod_look.fill_dir.x, max(cod_look.fill_dir.y, cod_look.fill_dir.x + 1e-4), up) * indoor;
    float gnd_gate = smoothstep(cod_look.fill_dir.z, max(cod_look.fill_dir.w, cod_look.fill_dir.z + 1e-4), -up);
    vec3 fill = (cod_look.sky_fill.xyz * sky_gate + cod_look.ground_fill.xyz * gnd_gate * indoor) * (fill_ao * cod_look.sky_fill.w);
    fill += cod_look.ground_fill.xyz * (owSunBounce(s.normal) * cod_look.ground_fill.w * fill_ao * indoor);
#ifdef COD_VIEWMODEL
    // the sky a weapon at the shoulder sees past the shooter's own body
    fill *= cod_look.view_key.w;
#endif
    indirect += fill * diffuse_color * (1.0 / COD_PI);
    lit += indirect * ao_bake;

    // indirect specular: SSR where it hit, else the environment, occluded by the screen-space and baked AO
#ifdef HZ_RENDERER_IBL
    float rough = 1.0 - s.smoothness;
    float spec_occ = mix(1.0, owSpecularOcclusion(ao_ss, rough), 0.6) * mix(1.0, owSpecularOcclusion(ao_bake, rough), 0.6);
    lit += hz_reflection(surface, li) * (spec_occ * env_scale);
#endif

    lit += s.emission * hz_scene.pre_exposure;
#ifdef HZ_RENDERER_FOG
    lit = hz_fog_apply(lit, view_depth);
#endif
    return lit;
}

void main() {
    CodSurfaceMaterial mat = CodSurfaceMaterial_unpack(v_material_offset);
    vec3 to_cam = hz_scene.camera_pos - v_world_pos;
    float owDist = length(to_cam);
#ifdef HZ_RENDERER_DOUBLE_SIDED
    float owFaceDir = gl_FrontFacing ? 1.0 : -1.0;
#else
    float owFaceDir = 1.0;
#endif
    vec3 owNw = normalize(v_normal) * owFaceDir;

#ifdef COD_LOCAL
    vec3 owP = v_opos;
    vec3 owNp = normalize(v_onrm) * owFaceDir;
    mat3 owR = mat3(v_r0, v_r1, v_r2);
#else
    vec3 owP = v_world_pos;
    vec3 owNp = owNw;
    mat3 owR = mat3(1.0);
#endif

    float owNormalAmp = mat.misc_a.x;
    float owGroundY = mat.misc_a.y;
    float owAoAmt = mat.misc_a.z;
    float owMacroRelief = mat.misc_a.w;
    vec3 owMask = v_color.rgb * mat.misc_b.x;
    float lod_scale = exp2(HZ_TEX_LOD_BIAS);

    vec4 alb;
    vec3 orm;
    vec3 nT;
    vec3 nShade;
    float owMicro = 0.0;
    float owDetFade = 0.0;
    float owHeightS;

#ifdef COD_TRIPLANAR
    vec3 an = abs(owNp);
    vec3 w = pow(an, vec3(5.0));
    w /= max(w.x + w.y + w.z, 1e-4);

    OwFrame fx = owAxisFrame(owP, owNp, 0, mat.tile);
    OwFrame fy = owAxisFrame(owP, owNp, 1, mat.tile);
    OwFrame fz = owAxisFrame(owP, owNp, 2, mat.tile);
    vec2 dxx = dFdx(fx.uv) * lod_scale, dxy = dFdy(fx.uv) * lod_scale;
    vec2 dyx = dFdx(fy.uv) * lod_scale, dyy = dFdy(fy.uv) * lod_scale;
    vec2 dzx = dFdx(fz.uv) * lod_scale, dzy = dFdy(fz.uv) * lod_scale;

    vec4 ax = texture_sample_grad_aniso(albedo, mat.albedo, fx.uv, dxx, dxy);
    vec4 ay = texture_sample_grad_aniso(albedo, mat.albedo, fy.uv, dyx, dyy);
    vec4 az = texture_sample_grad_aniso(albedo, mat.albedo, fz.uv, dzx, dzy);
    alb = ax * w.x + ay * w.y + az * w.z;

    vec3 ox = texture_sample_grad_aniso(orm_map, mat.orm_map, fx.uv, dxx, dxy).rgb;
    vec3 oy = texture_sample_grad_aniso(orm_map, mat.orm_map, fy.uv, dyx, dyy).rgb;
    vec3 oz = texture_sample_grad_aniso(orm_map, mat.orm_map, fz.uv, dzx, dzy).rgb;
    orm = ox * w.x + oy * w.y + oz * w.z;

    vec3 nx = unpack_normal_bc5(texture_sample_grad_aniso(normal_map, mat.normal_map, fx.uv, dxx, dxy).rg);
    vec3 ny = unpack_normal_bc5(texture_sample_grad_aniso(normal_map, mat.normal_map, fy.uv, dyx, dyy).rg);
    vec3 nz = unpack_normal_bc5(texture_sample_grad_aniso(normal_map, mat.normal_map, fz.uv, dzx, dzy).rg);
    nx.xy *= owNormalAmp; ny.xy *= owNormalAmp; nz.xy *= owNormalAmp;
    vec3 wnx = fx.T * nx.x + fx.B * nx.y + fx.N * nx.z;
    vec3 wny = fy.T * ny.x + fy.B * ny.y + fy.N * ny.z;
    vec3 wnz = fz.T * nz.x + fz.B * nz.y + fz.N * nz.z;
    vec3 nP = normalize(wnx * w.x + wny * w.y + wnz * w.z);

    // detail on the dominant plane only (one extra fetch)
    OwFrame fd = fz;
    vec2 uvdx_d = dzx, uvdy_d = dzy;
    if (an.y > max(an.x, an.z)) { fd = fy; uvdx_d = dyx; uvdy_d = dyy; }
    else if (an.x > an.z) { fd = fx; uvdx_d = dxx; uvdy_d = dxy; }
    vec2 detUv = fd.uv * mat.detail_p.x;
    float detFade = 1.0 - smoothstep(mat.detail_p.w * 0.45, mat.detail_p.w, owDist);
    owDetFade = detFade;
    vec3 dn = cod_detail_normal(mat.detail_arr, detUv, uvdx_d * mat.detail_p.x, uvdy_d * mat.detail_p.x, mat.detail_p.y);
    vec3 dW = fd.T * dn.x + fd.B * dn.y + fd.N * dn.z;
    nP = normalize(nP + (dW - fd.N * dot(dW, fd.N)) * mat.detail_p.y * detFade);
    vec4 dTex = texture2d_array_sample_grad_aniso(detail_arr, mat.detail_arr, detUv, 0, uvdx_d * mat.detail_p.x, uvdy_d * mat.detail_p.x);
    owMicro = (dTex.a - 0.5) * 2.0;
    alb.rgb *= 1.0 + (owMicro * 0.95 + (dTex.r - 0.5) * 1.25) * mat.detail_p.z * detFade;
    orm.r *= 1.0 - max(-owMicro, 0.0) * 0.30 * mat.detail_p.z * detFade;

    nShade = normalize(owR * nP);
    owHeightS = clamp(alb.a + owMicro * 0.16 * detFade, 0.0, 1.0);
#else
#ifdef COD_MESH_UV
    vec2 baseUv = v_uv * mat.tile.xy + mat.tile.zw;
    mat3 tbn = owTangentFrame(v_world_pos, owNw, baseUv);
    OwFrame f;
    f.uv = baseUv;
    f.T = tbn[0]; f.B = tbn[1]; f.N = tbn[2];
#else
    int axis = (abs(owNp.x) > abs(owNp.y)) ? ((abs(owNp.x) > abs(owNp.z)) ? 0 : 2) : ((abs(owNp.y) > abs(owNp.z)) ? 1 : 2);
    OwFrame f = owAxisFrame(owP, owNp, axis, mat.tile);
    owOrthonormalise(f, owNp);
#endif
    vec2 uvdx = dFdx(f.uv) * lod_scale;
    vec2 uvdy = dFdy(f.uv) * lod_scale;
    vec2 uv = f.uv;

#if !defined(COD_TRIPLANAR) && !defined(COD_ALPHA_MASK) && !defined(HZ_RENDERER_TRANSPARENT)
    // parallax is a per-material branch, so parallax and flat materials share one pipeline (fewer web compiles)
    if (mat.parallax_p.x > 0.0) {
        vec3 Vp = to_cam / max(owDist, 1e-5);
        vec3 vt = normalize(vec3(dot(Vp, f.T), dot(Vp, f.B), dot(Vp, f.N)));
        float pFade = 1.0 - smoothstep(mat.parallax_p.y, mat.parallax_p.z, owDist);
        uv = owPOM(mat.albedo, uv, vt, uvdx, uvdy, mat.parallax_p.x, pFade, mat.parallax_p.w);
    }
#endif

    alb = texture_sample_grad_aniso(albedo, mat.albedo, uv, uvdx, uvdy);
    orm = texture_sample_grad_aniso(orm_map, mat.orm_map, uv, uvdx, uvdy).rgb;
    nT = unpack_normal_bc5(texture_sample_grad_aniso(normal_map, mat.normal_map, uv, uvdx, uvdy).rg);
    nT.xy *= owNormalAmp;

    // micro detail normal, faded by distance
    float detFade = 1.0 - smoothstep(mat.detail_p.w * 0.45, mat.detail_p.w, owDist);
    owDetFade = detFade;
    vec2 detUv = uv * mat.detail_p.x;
    vec2 detDx = uvdx * mat.detail_p.x, detDy = uvdy * mat.detail_p.x;
    vec3 dn = cod_detail_normal(mat.detail_arr, detUv, detDx, detDy, mat.detail_p.y);
    nT = normalize(vec3(nT.xy + dn.xy * mat.detail_p.y * detFade, nT.z));

    // stochastic second sample, rotated and rescaled, blended by a low-frequency mask: breaks the repeat without a second texture set
    vec2 dmUv = (v_world_pos.xz + v_world_pos.y * 0.7) * mat.macro_p.x * 5.0 + 0.21;
    vec2 dmDx = dFdx(dmUv) * lod_scale, dmDy = dFdy(dmUv) * lod_scale;
    if (mat.rough_p.z > 0.0) {
        vec2 uv2 = vec2(uv.x * 0.803 - uv.y * 0.596, uv.x * 0.596 + uv.y * 0.803) * 0.617 + vec2(0.37, 0.71);
        vec2 uvdx2 = vec2(uvdx.x * 0.803 - uvdx.y * 0.596, uvdx.x * 0.596 + uvdx.y * 0.803) * 0.617;
        vec2 uvdy2 = vec2(uvdy.x * 0.803 - uvdy.y * 0.596, uvdy.x * 0.596 + uvdy.y * 0.803) * 0.617;
        vec4 alb2 = texture_sample_grad_aniso(albedo, mat.albedo, uv2, uvdx2, uvdy2);
        vec3 orm2 = texture_sample_grad_aniso(orm_map, mat.orm_map, uv2, uvdx2, uvdy2).rgb;
        vec3 n2 = unpack_normal_bc5(texture_sample_grad_aniso(normal_map, mat.normal_map, uv2, uvdx2, uvdy2).rg);
        n2.xy *= owNormalAmp;
        n2 = normalize(vec3(n2.xy + dn.xy * mat.detail_p.y * detFade, n2.z));
        float dtm = clamp((texture_sample_grad_aniso(macro_tex, mat.macro_tex, dmUv, dmDx, dmDy).g - 0.36) * 2.4, 0.0, 1.0);
        owHeightBlend(alb, orm, nT, alb2, orm2, n2, dtm * mat.rough_p.z);
    }
    // sub-millimetre aggregate: the micro height drives an albedo speckle and the cavity
    vec4 dTex = texture2d_array_sample_grad_aniso(detail_arr, mat.detail_arr, detUv, 0, detDx, detDy);
    owMicro = (dTex.a - 0.5) * 2.0;
    alb.rgb *= 1.0 + (owMicro * 0.95 + (dTex.r - 0.5) * 1.25) * mat.detail_p.z * detFade;
    orm.r *= 1.0 - max(-owMicro, 0.0) * 0.30 * mat.detail_p.z * detFade;

    owHeightS = clamp(alb.a + owMicro * 0.16 * detFade, 0.0, 1.0);
#ifdef COD_MESH_UV
    nShade = normalize(f.T * nT.x + f.B * nT.y + f.N * nT.z);
#else
    nShade = normalize(owR * (f.T * nT.x + f.B * nT.y + f.N * nT.z));
#endif
#endif

    // macro variation
    vec2 macroUv = mix(vec2(v_world_pos.x + v_world_pos.z * 0.63, v_world_pos.y), v_world_pos.xz, step(0.62, abs(owNw.y)));
    float owUpFace = step(0.62, abs(owNw.y));
    vec2 mdx = dFdx(macroUv) * lod_scale, mdy = dFdy(macroUv) * lod_scale;
    vec2 mUv1 = macroUv * mat.macro_p.x;
    vec2 mUv2 = macroUv * mat.macro_p.x * 0.211 + 0.37;
    vec4 mac1 = texture_sample_grad_aniso(macro_tex, mat.macro_tex, mUv1, mdx * mat.macro_p.x, mdy * mat.macro_p.x);
    vec4 mac2 = texture_sample_grad_aniso(macro_tex, mat.macro_tex, mUv2, mdx * mat.macro_p.x * 0.211, mdy * mat.macro_p.x * 0.211);
    // fbm never spans 0..1: the contrast expansion is what lets a 12 m facade break up
    float macro = clamp((mac1.r * 0.55 + mac2.b * 0.45 - 0.5) * mat.macro_big.x + 0.5, 0.0, 1.0);
    alb.rgb *= mix(1.0, 0.55 + 0.92 * macro, mat.macro_p.y);
    if (mat.macro_big.y > 0.0) {
        vec2 bigUv = macroUv * mat.macro_big.z;
        vec2 bdx = mdx * mat.macro_big.z, bdy = mdy * mat.macro_big.z;
        float big = texture_sample_grad_aniso(macro_tex, mat.macro_tex, bigUv, bdx, bdy).r * 0.62 +
                    texture_sample_grad_aniso(macro_tex, mat.macro_tex, bigUv * 0.37 + 0.61, bdx * 0.37, bdy * 0.37).b * 0.38;
        big = clamp((big - 0.5) * 2.3, -1.0, 1.0);
        alb.rgb *= 1.0 + big * mat.macro_big.y;
        orm.g = clamp(orm.g - big * mat.macro_big.y * 0.55, 0.0, 1.0);
    }
    alb.rgb *= mix(vec3(1.0), vec3(1.05, 1.0, 0.93), (mac2.r - 0.5) * mat.macro_p.w);
    // roughness has to vary or nothing in the frame ever glints
    orm.g = clamp(orm.g + (mac1.g - 0.5) * mat.macro_p.z + (mac1.a - 0.5) * 0.16 - owMicro * 0.07 * owDetFade, 0.0, 1.0);

    if (owMacroRelief > 0.0) {
        // ruts, drifts and shallow patches at 1-4 m: tilt the shading normal by the macro map's gradient
        vec2 mUv = macroUv * mat.macro_p.x;
        vec2 gdx = mdx * mat.macro_p.x, gdy = mdy * mat.macro_p.x;
        float mhx = texture_sample_grad_aniso(macro_tex, mat.macro_tex, mUv + vec2(0.035, 0.0), gdx, gdy).b;
        float mhy = texture_sample_grad_aniso(macro_tex, mat.macro_tex, mUv + vec2(0.0, 0.035), gdx, gdy).b;
        vec2 mg = (vec2(mhx, mhy) - mac1.b) * owMacroRelief * owUpFace;
        vec3 tiltW = vec3(-mg.x, 0.0, -mg.y);
        tiltW -= owNw * dot(owNw, tiltW);
        nShade = normalize(nShade + tiltW);
        alb.rgb *= 1.0 - (mac1.b - 0.5) * 0.16 * owUpFace;
    }

    // the horizontal coordinate along a wall, shared by the patch and runoff layers
    float owVert = smoothstep(0.72, 0.34, abs(owNw.y));
    float owSAxis = v_world_pos.z * owNw.x - v_world_pos.x * owNw.z;

    // repair patches: a rectangle of the facade a few percent off the surrounding mix, smoother, with a trowel edge
    if (mat.patch_p.x > 0.0) {
        float cw = max(mat.patch_p.y, 0.4);
        vec2 pc = vec2(owSAxis, v_world_pos.y) / cw;
        pc += (vec2(mac2.r, mac2.g) - 0.5) * 0.35;
        vec2 cid = floor(pc);
        vec2 cf = pc - cid;
        float r0 = owHash11(cid.x * 7.31 + cid.y * 13.77 + 5.1);
        float r1 = owHash11(cid.x * 3.17 + cid.y * 9.41 + 21.3);
        float r2 = owHash11(cid.x * 11.93 + cid.y * 4.73 + 37.7);
        float r3 = owHash11(cid.x * 5.51 + cid.y * 17.29 + 53.9);
        float has = step(1.0 - clamp(mat.patch_p.x, 0.0, 1.0), r0);
        vec2 lo = vec2(0.05 + r1 * 0.30, 0.05 + r2 * 0.30);
        vec2 hi = vec2(0.95 - r2 * 0.26, 0.95 - r3 * 0.26);
        float fe = 0.028 + 0.030 * r1;
        vec2 a0 = smoothstep(lo, lo + fe, cf);
        vec2 a1 = 1.0 - smoothstep(hi - fe, hi, cf);
        float pm = a0.x * a0.y * a1.x * a1.y * has * owVert;
        if (pm > 0.001) {
            float sgn = r3 > 0.48 ? 1.0 : -1.0;
            alb.rgb *= 1.0 + sgn * mat.patch_p.z * pm;
            vec3 pTint = sgn > 0.0 ? vec3(0.975, 0.988, 1.020) : vec3(1.030, 1.008, 0.968);
            alb.rgb *= mix(vec3(1.0), pTint, pm);
            orm.g = clamp(orm.g + mat.patch_p.w * pm, 0.0, 1.0);
            float lip = pm * (1.0 - pm) * 4.0;
            alb.rgb *= 1.0 + lip * 0.13;
            owHeightS = clamp(owHeightS + pm * 0.07 + lip * 0.05, 0.0, 1.0);
        }
    }

    // weathering: dust on up faces, rain runoff below sources, ground splash and the dust wedge
    if (mat.weather_p.x > 0.0 || mat.weather_p.y > 0.0 || mat.weather_p.z > 0.0) {
        float up = clamp(owNw.y, 0.0, 1.0);
        float dust = up * up * mat.weather_p.x * smoothstep(0.30, 0.80, mac1.b * 0.7 + mac2.g * 0.5);
        alb.rgb = mix(alb.rgb, mat.dust_col.rgb, dust * 0.75);
        orm.g = clamp(orm.g + dust * 0.30, 0.0, 1.0);
        orm.b *= 1.0 - dust * 0.85;
        nShade = normalize(mix(nShade, owNw, dust * 0.35));

        // streaks live on near-vertical faces only, below a source, 3-8 cm wide
        float vert = owVert;
        float sAxis = owSAxis;
        vec2 sUv0 = vec2(sAxis * 0.46, v_world_pos.y * 0.155);
        vec2 sUv1 = vec2(sAxis * 1.35 + 0.4, v_world_pos.y * 0.42);
        float sN = texture_sample_lod(macro_tex, mat.macro_tex, sUv0, 0.0).a;
        float sFine = texture_sample_lod(macro_tex, mat.macro_tex, sUv1, 0.0).g;
        vec3 runoff = owRunoff(sAxis, v_world_pos.y, sN - 0.5);
        float streak = clamp(mat.weather_p.y * 2.2, 0.0, 1.15) * vert * runoff.x * smoothstep(0.30, 0.66, sN * 0.72 + sFine * 0.38);
        streak = clamp(streak, 0.0, 1.0);
        if (mat.misc_b.x > 0.0) {
            // the world's authored runoff strips drive the run outright
            float owStainM = smoothstep(0.58, 0.98, owMask.g);
            streak = clamp(streak * (0.45 + 0.75 * clamp(owMask.g * 1.5 + owMask.b * 0.6, 0.0, 1.0)) +
                           owStainM * vert * (0.55 + 0.45 * smoothstep(0.20, 0.70, sN * 0.6 + sFine * 0.55)),
                           0.0, 1.0);
        }
        vec3 runCol = mix(alb.rgb * 0.72, mat.grime_col.rgb, 0.26);
        // rust bleed under metal fixings, strongest right under the fixing
        float rust = clamp(step(0.86, runoff.y) * 0.9 + orm.b * 0.5, 0.0, 1.0) * (0.30 + 0.70 * (1.0 - smoothstep(0.1, 0.9, runoff.z)));
        runCol = mix(runCol, mix(alb.rgb * 0.94, mat.rust_col.rgb, 0.5), rust);
        alb.rgb = mix(alb.rgb, runCol, streak);
        orm.g = clamp(orm.g + streak * 0.09, 0.0, 1.0);
        orm.b *= 1.0 - streak * 0.35;

        // ground splash: a hard dirt band in the bottom 20 cm plus thinning splatter above it
        float hAbove = v_world_pos.y - owGroundY;
        float band = 1.0 - smoothstep(0.02, 0.22, hAbove);
        float spray = 1.0 - smoothstep(0.10, max(mat.weather_p.z, 1e-3), hAbove);
        float splash = vert * max(band, spray * spray * 0.85) * step(1e-4, mat.weather_p.z);
        splash *= 0.55 + 0.45 * smoothstep(0.25, 0.72, mac1.b * 0.7 + mac2.g * 0.4);
        vec3 splashCol = mix(mat.grime_col.rgb, mat.dust_col.rgb * 0.9, 0.35);
        alb.rgb = mix(alb.rgb * (1.0 - splash * 0.35), splashCol, splash * 0.42);
        orm.g = clamp(orm.g + splash * 0.16 - band * vert * 0.10, 0.0, 1.0);
        orm.r *= 1.0 - splash * 0.18;
        orm.b *= 1.0 - splash * 0.7;

        // the wedge of the ground's own dust piled against a wall
        float wedgeH = 0.26 + 0.18 * (mac1.r * 0.6 + mac2.b * 0.7);
        float wedge = vert * (1.0 - smoothstep(wedgeH * 0.25, wedgeH, hAbove));
        wedge *= wedge * (0.7 + 0.5 * smoothstep(0.2, 0.8, mac2.g));
        wedge = clamp(wedge, 0.0, 1.0) * step(1e-4, mat.weather_p.z);
        alb.rgb = mix(alb.rgb, mat.dust_col.rgb, wedge * 0.46);
        orm.g = clamp(orm.g + wedge * 0.07, 0.0, 1.0);
        orm.b *= 1.0 - wedge * 0.9;
        nShade = normalize(mix(nShade, owNw, wedge * 0.45));
    }

    // cavity grime and the vertex masks
    float cav = 1.0 - owHeightS;
    alb.rgb = mix(alb.rgb, mat.grime_col.rgb, cav * cav * mat.weather_p.w);
    orm.r *= 1.0 - cav * mat.weather_p.w * 0.5;

    if (mat.misc_b.x > 0.0) {
        // wear is broken up by the macro noise and biased to the high points of the height field
        float wearN = smoothstep(0.25, 0.85, mac1.b * 0.65 + mac2.a * 0.55);
        float wearM = owMask.r * mat.wear_p.x * (0.55 + 0.45 * smoothstep(0.30, 0.80, owHeightS)) * (0.25 + 1.15 * wearN);
        wearM = clamp(wearM, 0.0, 1.0);
        alb.rgb = mix(alb.rgb, mat.wear_col.rgb, wearM * mat.wear_mat.w);
        orm.g = mix(orm.g, mat.wear_mat.x, wearM);
        orm.b = mix(orm.b, mat.wear_mat.y, wearM);
        float grimeM = owMask.g * mat.wear_p.y * (0.35 + 0.65 * cav) * (0.45 + 0.9 * mac2.g);
        alb.rgb = mix(alb.rgb, mat.grime_col.rgb, grimeM * 0.8);
        orm.g = clamp(orm.g + grimeM * 0.22, 0.0, 1.0);
        orm.b *= 1.0 - grimeM * 0.8;
        orm.r *= 1.0 - owMask.b * mat.wear_p.z;
    }

    float clothTrans = 0.0;
    vec2 fUv = vec2(v_world_pos.x + v_world_pos.z * 0.63, v_world_pos.y * 0.7 + v_world_pos.z * 0.4) * 3.4;
    vec2 fdx = dFdx(fUv) * lod_scale, fdy = dFdy(fUv) * lod_scale;
    if (mat.cloth_p.x > 0.0 || mat.cloth_p.y < 1.0) {
        // the underside of a stretched canopy sits in its own shadow and collects soot
        float owDown = smoothstep(0.10, -0.70, owNw.y);
        alb.rgb *= mix(1.0, mat.cloth_p.y, owDown);
        orm.g = clamp(orm.g + owDown * 0.05, 0.0, 1.0);
        if (mat.cloth_p.z > 0.0) {
            // 8-14 cm drape folds: tilt the shading normal by the gradient of the macro band
            float f0 = texture_sample_grad_aniso(macro_tex, mat.macro_tex, fUv, fdx, fdy).b;
            float fxs = texture_sample_grad_aniso(macro_tex, mat.macro_tex, fUv + vec2(0.05, 0.0), fdx, fdy).b;
            float fys = texture_sample_grad_aniso(macro_tex, mat.macro_tex, fUv + vec2(0.0, 0.05), fdx, fdy).b;
            vec3 tiltC = vec3(-(fxs - f0), -(fys - f0), 0.0) * mat.cloth_p.z * 9.0;
            vec3 cam_right = vec3(hz_scene.view[0][0], hz_scene.view[1][0], hz_scene.view[2][0]);
            vec3 cam_up = vec3(hz_scene.view[0][1], hz_scene.view[1][1], hz_scene.view[2][1]);
            nShade = normalize(nShade + cam_right * tiltC.x + cam_up * tiltC.y);
            alb.rgb *= 1.0 - (f0 - 0.5) * mat.cloth_p.z * 0.9;
        }
        clothTrans = mat.cloth_p.x;
    }

    alb.rgb *= mat.tint_col.rgb;
    // a per-surface roughness floor: tile, glass and painted metal must stay glossy enough to catch a highlight
    orm.g = clamp(orm.g * mat.rough_p.x + mat.rough_p.y, max(mat.rough_p.w, 0.015), 1.0);

#ifdef COD_ALPHA_MASK
    if (alb.a < 0.45) discard;
#endif

    HzSurface s = hz_surface_default();
    s.geo_normal = owNw;
    s.normal = nShade;
    s.albedo = alb.rgb;
    s.metallic = orm.b;
    s.smoothness = 1.0 - orm.g;
    s.emission = mat.emissive_col.rgb;
#if defined(HZ_RENDERER_TRANSPARENT)
    s.alpha = mat.misc_b.y;
#endif
#ifdef HZ_RENDERER_DECALS
    hz_apply_decals(s, v_world_pos, gl_FragCoord.xy, v_decal_mask);
#endif
    float ao_aoamt = (orm.r - 1.0) * owAoAmt + 1.0;

#ifdef COD_VIEWMODEL
    float contact = 1.0;
#else
    // the screen-space contact shadow on the key term; the viewmodel does not get it
    float contact = texture_sample_lod(cod_contact, mat.cod_contact, gl_FragCoord.xy * cod_contact_size.xy, 0.0).r;
#endif
    vec3 lit = cod_light(s, v_world_pos, v_view_depth, ao_aoamt, clothTrans, clamp(orm.r, 0.0, 1.0), mat.misc_b.z, contact);
#if defined(HZ_RENDERER_TRANSPARENT)
    float out_a = s.alpha;
#else
    float out_a = 1.0;
#endif
    hz_output_lit(vec4(lit, out_a), s, HZ_VELOCITY(v_clip_cur, v_clip_prev));
}
@end
