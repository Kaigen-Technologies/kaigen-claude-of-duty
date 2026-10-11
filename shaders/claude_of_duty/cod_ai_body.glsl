// The Claude of Duty soldier body: a skinned standard material with vertex colours, a tiling detail normal and the silhouette edge darkening, lit by the look's budgets.
// The lighting block is cod_surface.glsl's cod_light, kept in step with it by hand.
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
#ifdef HZ_RENDERER_GBUFFER
out vec4 v_clip_cur;
out vec4 v_clip_prev;
#endif

void main() {
    VertexInstanceData v = unpack_instance_data();
#ifdef HZ_RENDERER_GBUFFER
    vec3 bind_pos = v.pos;
#endif
#ifdef HZ_RENDERER_SKINNED
    apply_skinning(v.pos, v.normal, v.tangent, v.bone_offset, v.vertex_base, v.vertex_layout_flags);
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
#ifdef HZ_RENDERER_IBL
#define HZ_RENDERER_REFLECTION_PROBES
#endif
#include "renderer_shaders.glsl"
#include "cod_look.glsl"
in vec2 v_uv;
in vec4 v_color;
flat in uint v_material_offset;
#ifdef HZ_RENDERER_DECALS
flat in uint v_decal_mask;
#endif
in vec3 v_normal;
in vec3 v_world_pos;
in float v_view_depth;
#ifdef HZ_RENDERER_GBUFFER
in vec4 v_clip_cur;
in vec4 v_clip_prev;
#endif

@material
struct CodBodyMaterial {
    vec4 tint;     // linear colour multiplied into the albedo
    vec4 pbr;      // x roughness scale, y metalness scale, z normal scale, w ao intensity
    vec4 detail_p; // x detail tiles per base tile, y normal amount, z roughness amount, w 1 when the detail tile is bound
    vec4 rim_p;    // x strength, y edge, z power
    vec4 misc;     // x reflection scale, y 1 when the maps are bound
    @texture2D @white
    uint albedo;
    @texture2D @normal
    uint normal_map;
    @texture2D @white
    uint orm_map;
    @texture2D @normal
    uint detail_tex;
};

#define COD_PI 3.14159265

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

// the shade step: hz_light's structure with the look's budgets on the key light, the indirect terms and the interior gate
vec3 cod_light(HzSurface s, vec3 world_pos, float view_depth, float bake_ao, float cloth_trans, float cloth_ao, float env_scale) {
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

#ifdef HZ_RENDERER_SHADOWS
    float shadow_factor = hz_shadow(world_pos);
    vec3 shadow_color = hz_shadow_input.shadow_color;
#else
    float shadow_factor = 1.0;
    vec3 shadow_color = vec3(0.0);
#endif

    // the main light (index 0) takes the key gain and the direct AO fraction; the others are untouched
    LightParams key_only = hz_scene.lights;
    key_only.ambient_intensity = 0.0;
    key_only.directional_count = min(hz_scene.lights.directional_count, 1u);
    vec3 lit = pbr_directional_lighting(surface, li, key_only, shadow_factor, shadow_color) * key_gain *
               mix(1.0, ao_ss, cod_look.key_gain.w);
    LightParams rest = hz_scene.lights;
    rest.ambient_intensity = 0.0;
    rest.directional[0] = hz_scene.lights.directional[1];
    rest.directional[1] = hz_scene.lights.directional[2];
    rest.directional[2] = hz_scene.lights.directional[3];
    rest.directional_count = hz_scene.lights.directional_count > 0u ? hz_scene.lights.directional_count - 1u : 0u;
    lit += pbr_directional_lighting(surface, li, rest, shadow_factor, shadow_color);

#ifdef HZ_RENDERER_LIGHTING_CLUSTERS
    HZ_CLUSTER_LOOP_BEGIN(view_depth)
        lit += pbr_point_light(surface, li, light);
    HZ_CLUSTER_LOOP_END
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
    float indoor = owInteriorGate(world_pos, ao_ss);
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
    CodBodyMaterial mat = CodBodyMaterial_unpack(v_material_offset);
    vec3 owNw = normalize(v_normal);
    vec3 V = normalize(hz_scene.camera_pos - v_world_pos);
    vec4 alb = vec4(1.0);
    vec3 orm = vec3(1.0);
    vec3 mapN = vec3(0.0, 0.0, 1.0);
    if (mat.misc.y > 0.5) {
        alb = texture_sample(albedo, mat.albedo, v_uv);
        orm = texture_sample(orm_map, mat.orm_map, v_uv).rgb;
        mapN = texture_sample(normal_map, mat.normal_map, v_uv).xyz * 2.0 - 1.0;
    }
    mapN.xy *= mat.pbr.z;
    float rough = clamp(mat.pbr.x * orm.g, 0.0, 1.0);
    if (mat.detail_p.w > 0.5) {
        vec4 d = texture_sample(detail_tex, mat.detail_tex, v_uv * mat.detail_p.x);
        mapN.xy += (d.xy * 2.0 - 1.0) * mat.detail_p.y;
        rough = clamp(rough + (d.w - 0.5) * mat.detail_p.z, 0.04, 1.0);
    }
    rough = max(rough, 0.04);
    mat3 tbn = owTangentFrame(-(hz_scene.camera_pos - v_world_pos), owNw, v_uv);
    vec3 nShade = normalize(tbn * normalize(mapN));

    HzSurface s = hz_surface_default();
    s.geo_normal = owNw;
    s.normal = nShade;
    s.albedo = alb.rgb * mat.tint.rgb * v_color.rgb;
    s.metallic = clamp(mat.pbr.y * orm.b, 0.0, 1.0);
    s.smoothness = 1.0 - rough;
#ifdef HZ_RENDERER_DECALS
    hz_apply_decals(s, v_world_pos, gl_FragCoord.xy, v_decal_mask);
#endif
    float ao = (orm.r - 1.0) * mat.pbr.w + 1.0;
    vec3 lit = cod_light(s, v_world_pos, v_view_depth, ao, 0.0, clamp(orm.r, 0.0, 1.0), mat.misc.x);
    // the silhouette: darken the grazing sliver of the closed surface, on the geometric normal so it cannot crawl
    float edge = 1.0 - abs(dot(V, owNw));
    lit *= 1.0 - mat.rim_p.x * pow(smoothstep(mat.rim_p.y, 1.0, edge), mat.rim_p.z);
    hz_output_lit(vec4(lit, 1.0), s, HZ_VELOCITY(v_clip_cur, v_clip_prev));
}
@end
