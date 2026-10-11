// The Claude of Duty grounding sprite: an alpha-over near-black radial occlusion quad, a multiply on whatever is under it.
#include "renderer.glsl"

@vs
#define HZ_RENDERER_VERTEX_SHADER
#include "renderer_shaders.glsl"
invariant gl_Position;
out vec2 v_uv;
flat out uint v_material_offset;
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
    v_material_offset = v.material_offset;
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
#include "renderer_shaders.glsl"
in vec2 v_uv;
flat in uint v_material_offset;
#ifdef HZ_RENDERER_GBUFFER
in vec4 v_clip_cur;
in vec4 v_clip_prev;
#endif

@material
struct CodGroundMaterial {
    vec4 p; // x falloff power, y opacity
};

void main() {
    CodGroundMaterial mat = CodGroundMaterial_unpack(v_material_offset);
    vec2 u = v_uv * 2.0 - 1.0;
    float r = min(1.0, length(u));
    float a = exp(-r * r * mat.p.x) * (1.0 - r * r * r) * mat.p.y;
    hz_output_unlit(vec4(0.045, 0.05, 0.062, a) * vec4(hz_scene.pre_exposure, hz_scene.pre_exposure, hz_scene.pre_exposure, 1.0), HZ_VELOCITY(v_clip_cur, v_clip_prev));
}
@end
