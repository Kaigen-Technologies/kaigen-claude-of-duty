// Claude of Duty: a first-person shooter in a procedural market street; app entry points.
#include "app/app.h"
#include "assets/assets.h"
#include "ecs/ecs.h"
#include "ecs/ecs.gen.h"
#include "ecs/camera/ecs_camera.h"
#include "ecs/ecs_assets/ecs_assets.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "ecs/ecs_navigation/ecs_navigation.h"
#include "ecs/ecs_rigidbody/ecs_rigidbody.h"
#include "ecs/input/ecs_input.h"
#include "ecs/renderer/ecs_renderer.h"
#include "ecs/renderer/ecs_renderer.gen.h"
#include "ecs/transform/ecs_transform.h"
#include "ecs/transform/ecs_transform.gen.h"
#include "hz_engine.h"
#include "input.h"
#include "lib/color.h"
#include "lib/hzmath.h"
#include "lib/string.h"
#include "lib/thread_context.h"
#include "lib/typedefs.h"
#include "os/os.h"
#include "perf_stats.h"
#include "physics/physics.h"
#include "rendering/camera.h"
#include "rendering/features/atmosphere.h"
#include "rendering/features/clouds.h"
#include "rendering/features/bloom.h"
#include "rendering/features/gtao.h"
#include "rendering/features/height_fog.h"
#include "rendering/features/shadows.h"
#include "rendering/features/ssr.h"
#include "rendering/mesh_simplify.h"
#include "rendering/renderer.h"
#include "scene/scene.h"
#include "ui/ui.h"
#include "generated/types_app.gen.h"
#include "gameplay/camera_rig/camera_rig.h"
#include "gameplay/character/character.h"
#include "gameplay/test_clock/test_clock.h"
#include "gameplay/day_night/day_night.h"
// the editor is a native desktop tool: no wasm or iOS editor
#if !defined(WASM) && !defined(IOS)
#define COD_EDITOR 1
#include "ecs/ecs_editor/ecs_editor.h"
#include "rendering/gizmos.h"
#else
#define COD_EDITOR 0
#endif
#include "claude_of_duty.h"
#include "claude_of_duty/claude_of_duty.gen.h"
#include "cod_rng.h"
#include "cod_masks.h"
#include "cod_world.h"
#include "cod_materials.h"
#include "cod_player.h"
#include "claude_of_duty/cod_player.gen.h"
#include "cod_player_internal.h"
#include "cod_weapons.h"
#include "claude_of_duty/cod_weapons.gen.h"
#include "cod_weapons_internal.h"
#include "cod_weapons_rig.h"
#include "cod_weapons_hands.h"
#include "cod_ai.h"
#include "claude_of_duty/cod_ai.gen.h"
#include "cod_ai_body.h"
#include "cod_ai_body_internal.h"
#include "claude_of_duty/cod_ai_body.gen.h"
#include "cod_ai_internal.h"
#include "cod_fx.h"
#include "cod_hud.h"
#include "cod_hud_internal.h"
#include "cod_audio.h"
#include "cod_look.h"

#define COD_SCENE_PATH "claude_of_duty/main.hzt"

ECS_COMPONENT_TYPEDEF(AppMemory);

#include "cod_input.c"
#include "cod_masks.c"
#include "cod_flow.c"
#include "cod_world_geo.c"
#include "cod_world_asm.c"
#include "cod_world_layout.c"
#include "cod_world_kit.c"
#include "cod_world_prims.c"
#include "cod_world_props.c"
#include "cod_world_interiors.c"
#include "cod_world_build.c"
#include "cod_world_dress.c"
#include "cod_world_dress2.c"
#include "cod_world_ground.c"
#include "cod_world.c"
#include "cod_materials.c"
#include "cod_player_mantle.c"
#include "cod_player_move.c"
#include "cod_player_camera.c"
#include "cod_player.c"
#include "cod_weapons_geo.c"
#include "cod_weapons_parts.c"
#include "cod_weapons_models.c"
#include "cod_weapons_hands.c"
#include "cod_weapons_defs.c"
#include "cod_weapons_view.c"
#include "cod_weapons_mats.c"
#include "cod_weapons_ballistics.c"
#include "cod_weapons.c"
#include "cod_ai_nav.c"
#include "cod_ai_sense.c"
#include "cod_ai_brain.c"
#include "cod_ai_squad.c"
#include "cod_ai_spawn.c"
#include "cod_ai_debug.c"
#include "cod_ai.c"
#include "cod_ai_body_geo.c"
#include "cod_ai_body_parts.c"
#include "cod_ai_body_build.c"
#include "cod_ai_body_shade.c"
#include "cod_ai_body_tex.c"
#include "cod_ai_body_clips.c"
#include "cod_ai_body_anim.c"
#include "cod_ai_body_ragdoll.c"
#include "cod_ai_body_ecs.c"
#include "cod_ai_body.c"
#include "cod_fx.c"
#include "cod_hud_model.c"
#include "cod_hud_widgets.c"
#include "cod_hud_status.c"
#include "cod_hud_top.c"
#include "cod_hud_menu.c"
#include "cod_hud_touch.c"
#include "cod_hud.c"
#include "cod_audio.c"
#include "cod_look_sky.c"
#include "cod_look_grade.c"
#include "cod_exposure.c"
#include "cod_post_blur.c"
#include "cod_post_contact.c"
#include "cod_post_lowhealth.c"
#include "cod_post.c"
#include "cod_look.c"

// the frame clear colour; the renderer creates its HDR target to fast-clear to it
#define COD_CLEAR_COLOR ((ColorF32){0.0f, 0.0f, 0.0f, 1.0f})

// the sampler level is fixed when the GPU starts
const HzAppConfig hz_app_config = {.max_anisotropy = 16}; // the quality level sets the resolution limits (cod_look)

typedef struct AppState {
  EcsWorld *world;
  Allocator allocator;
  SceneSystem scene_sys;
  SceneHandle scene;
  EcsQuery *camera_query;
  u32 lock_seen; // the CodFlow pointer-lock request last applied
#if COD_EDITOR
  EcsEditorDefault editor;
#endif
} AppState;

HZ_APP_API void app_init(AppMemory *memory) {
  // app_init runs on every lane; world setup is main-lane work
  if (!is_main_thread())
    return;
  AppState *s = memory->state;
  s->allocator = make_arena_allocator(arena_create(MB(16), MB(1)));

  EcsWorld *w = ecs_world_create();
  s->world = w;
  ecs_resource_set_ref(w, AppMemory, memory);

  ecs_input_init(w);
  ecs_assets_init(w, NULL);
  // the procedural street outgrows the default geometry pool: commit a little, reserve the level's worth
  RendererInitDesc renderer_desc = {.vertex_buffer_size = MB(16),
                                    .vertex_buffer_reserve = MB(256),
                                    .index_buffer_size = MB(8),
                                    .index_buffer_reserve = MB(128),
                                    .max_submesh_instances = 32768,
                                    .clear_color = COD_CLEAR_COLOR,
                                    .assets = ecs_resource_get_mut(w, AssetSystem)};
  cod_look_renderer_desc(&renderer_desc);
  ecs_renderer_init(w, &renderer_desc);
  ecs_transform_init(w, NULL);
  // the street is ~120 m across; room above for grenade arcs and below for the fall reset
  PhysicsSettings physics = physics_settings_default(aabb_make((v3){-110.0f, -30.0f, -110.0f}, (v3){110.0f, 60.0f, 110.0f}), (iv3){4, 1, 4});
  physics.gravity = (v3){0.0f, -COD_GRAVITY, 0.0f};
  ecs_rigidbody_init(w, &(EcsRigidBodyInitDesc){.settings = physics});
  ecs_physics_init(w, physics.collision_layer_settings);
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);

  scene_system_create(&s->scene_sys, w, assets, &s->allocator);
  s->scene = scene_load_file(&s->scene_sys, COD_SCENE_PATH);
  s->camera_query = ecs_query_cached(w, EcsIn(Camera));

  // the camera exists from the first frame, so the load screen renders before any asset lands
  EcsEntity cam = ecs_entity_create(w);
  ecs_component_set(w, cam, EntityName, {.name = fixedstr64_from_cstr("Camera")});
  ecs_component_set(w, cam, Camera,
                    {.pos = (v3){12.0f, 1.75f, 18.0f}, .rot = quat_identity(), .fov = COD_FOV, .near_plane = COD_CAMERA_NEAR,
                     .far_plane = COD_CAMERA_FAR, .aspect = 16.0f / 9.0f});

  cod_input_init(w);
  cod_flow_init(w);
  cod_look_quality_init(w); // the bakes and the FX pools size from the start level
  cod_materials_init(w);
  cod_world_init(w);
  cod_player_init(w);
  cod_weapons_init(w);
  cod_ai_init(w);
  cod_ai_body_init(w);
  cod_fx_init(w);
  cod_audio_init(w);
  // the UI lives in its own arena
  Allocator ui_alloc = make_arena_allocator(arena_create(MB(4), KB(256)));
  cod_hud_init(w, &ui_alloc);
  cod_look_init(w, &s->allocator);

#if COD_EDITOR
  ecs_editor_default_init(&s->editor, &(EcsEditorDefaultDesc){.world = w,
                                                              .assets = assets,
                                                              .ui = ecs_resource_get(w, CodUi)->ui,
                                                              .allocator = &s->allocator,
                                                              .canvas_width = memory->canvas_width,
                                                              .canvas_height = memory->canvas_height});
  RendererState *renderer = ecs_resource_get_mut(w, RendererState);
  ecs_resource_set_ref(w, GizmoSystem, gizmos_create(&(GizmoInitDesc){.assets = assets, .renderer = renderer, .sample_count = 1}));
  gizmos_attach_to_renderer(ecs_resource_get_mut(w, GizmoSystem), renderer);
#endif

  log_info("claude of duty initialized");
}

HZ_APP_API void app_update_and_render(AppMemory *memory) {
  AppState *s = memory->state;
  EcsWorld *w = s->world;
  perf_stats_begin_frame();
  cod_hud_font_update(w); // collective, every lane, before the UI frame

  // main lane before the scope: OS input and the editor toggle; the barrier publishes them
  if (is_main_thread()) {
    InputSystem *input = ecs_resource_get_mut(w, InputSystem);
    input_update(input, &memory->input_events, memory->total_time);
    CodEditor *editor = ecs_resource_get_mut(w, CodEditor);
#if COD_EDITOR
    if (input->buttons[KEY_F1].pressed_this_frame)
      editor->open = !editor->open;
    if (editor->open && input->buttons[KEY_F5].pressed_this_frame)
      s->scene = scene_save(&s->scene_sys, s->scene, COD_SCENE_PATH);
#endif
    const CodFlow *f = ecs_resource_get(w, CodFlow);
    if (editor->open)
      cod_input_hold(ecs_resource_get_mut(w, CodInput));
    else
      cod_input_collect(ecs_resource_get_mut(w, CodInput), ecs_resource_get_mut(w, CodInputMap), ecs_resource_get_mut(w, CodTouch), input,
                        f->state, f->debug_controls, memory->input_events.focus_lost,
                        ecs_resource_get(w, CodPlayerState)->stance != COD_STANCE_STAND, m_minf(memory->dt, COD_MAX_DT), memory->total_time);
  }
  lane_sync();

  // every lane, outside the scope: the procedural builds' parallel work (each hook keeps its lane_syncs balanced)
  cod_materials_lanes(w);
  cod_world_lanes(w);
  cod_weapons_lanes(w);
  cod_ai_lanes(w);
  cod_ai_body_lanes(w);
  cod_fx_lanes(w);
  cod_audio_lanes(w);

  // ALL lanes enter the one scope; game logic is systems
  ecs_scope(w) {
    ecs_assets_schedule(w);
    cod_flow_schedule(w, m_minf(memory->dt, COD_MAX_DT)); // writes CodFlow, CodClock, CodDev
#if COD_EDITOR
    ecs_editor_default_schedule(w);
#endif
    ecs_flush(w, memory->dt);

    // every lane reads the same settled clock
    const CodClock *clock = ecs_resource_get(w, CodClock);
    cod_materials_schedule(w);
    cod_world_schedule(w);
    ecs_physics_schedule(w); // the static layer the character walks on, rebuilt when the world's colliders land
    ecs_navigation_schedule(w); // the soldiers' navmesh over the same static colliders, and their path requests
#if COD_EDITOR
    ecs_navigation_debug_schedule(w); // draws only while ai_debug >= 3; the gizmos exist only with the editor
#endif
    cod_player_schedule_input(w);
    ecs_flush(w, clock->dt);

    // the fixed step: the character, the soldiers and the weapons, then the rigid bodies, N times at 120 Hz
    for (i32 k = 0; k < clock->steps; k++) {
      cod_player_schedule_fixed(w);
      cod_ai_schedule_fixed(w);
      cod_weapons_schedule_fixed(w);
      ecs_flush_ex(w, &(EcsFlushDesc){.delta_time = COD_FIXED_DT, .steps = 1});
      ecs_rigidbody_step(w, COD_FIXED_DT); // all lanes, between flushes
    }

    cod_look_schedule(w); // the day_night block turns the sun before the transforms; fx, hud and audio read CodLook
    cod_player_schedule(w);
    cod_weapons_schedule(w); // fire, ballistics, the viewmodel; its events feed the AI's damage
    cod_ai_schedule(w);      // the soldiers; their fire writes the player's intake
    cod_ai_body_schedule(w); // the bodies follow this frame's drive
    cod_player_schedule_intake(w);
    cod_weapons_schedule_view(w); // the viewmodel entities on the final camera
    cod_fx_schedule(w);     // after every producer's events
    cod_hud_schedule(w);
    cod_audio_schedule(w);
    ecs_transform_schedule(w);
    ecs_physics_sync_schedule(w); // authored colliders and moved bodies into ColliderBody for next frame's queries
    ecs_camera_schedule(w, &(EcsCameraScheduleDesc){.transform_sync_off = true}); // the first-person rig writes Camera
    cod_hud_schedule_late(w); // the projections need the camera
    ecs_renderer_schedule(w);
    ecs_flush(w, clock->dt);
  }

  // null until the camera entity exists
  Camera *cam = ecs_query_get_singleton(Camera, s->camera_query);
  // main lane after the scope: the pointer lock, GPU bakes, the UI frame, renderer settings
  if (is_main_thread()) {
    const CodEditor *editor = ecs_resource_get(w, CodEditor);
    if (!editor->open)
      cod_flow_main(w, &s->lock_seen);
    cod_materials_frame(w);
    cod_world_frame(w);
    cod_weapons_frame(w);
    cod_ai_frame(w);
    cod_ai_body_frame(w);
    cod_fx_frame(w);
    cod_audio_frame(w);
    b32 editor_drew = false;
#if COD_EDITOR
    if (editor->open && cam) {
      const CodUi *u = ecs_resource_get(w, CodUi);
      ui_begin_frame(u->ui, ecs_resource_get(w, InputSystem), memory->dt);
      ecs_editor_default_update(&s->editor, &(EcsEditorDefaultUpdateDesc){.camera = cam,
                                                                         .canvas_width = memory->canvas_width,
                                                                         .canvas_height = memory->canvas_height});
      gizmos_prepare(ecs_resource_get_mut(w, GizmoSystem), &(GizmoFlushDesc){.camera = cam,
                                                                            .line_width = 2.0f,
                                                                            .screen_width = memory->canvas_width,
                                                                            .screen_height = memory->canvas_height});
      ui_finish_layout(u->ui);
      editor_drew = true;
    }
#endif
    if (!editor_drew)
      cod_hud_frame(w);
    cod_look_sync(w);
  }
  lane_sync();

  // ALL lanes: the trail ribbons, then the renderer records and submits in parallel
  if (cam) {
    cod_fx_prepare(w, cam);
    renderer_update(ecs_resource_get_mut(w, RendererState), &(RendererBeginFrameDesc){
                                                                .camera = cam,
                                                                .clear_color = COD_CLEAR_COLOR,
                                                                .time = ecs_resource_get(w, CodClock)->time,
                                                            });
  }
  perf_stats_end_frame();
}

HZ_APP_API size_t app_state_size(void) { return sizeof(AppState); }
