---
title: Gameplay Blocks
description: catalog of the ready-made gameplay blocks in this module — character controller, camera rigs, camera shake, day/night cycle, world-space UI, HUD messages and map, mobile touch controls, VFX, MeshBuilder, scatter, test clock — what each does and what it deliberately leaves to the app
resource: src/gameplay/character/character.h
tags: [engine, gameplay, blocks, character, camera]
updated: 2026-10-06
---

Each block is one directory under this module's `src/gameplay/`; the `.h` is the
manual (long, verified, with a Usage block at the top) — this page routes.

Shared contract: a block is one of two shapes, and the header says which.

- **Per-entity blocks are systems.** `character`, `camera_rig`, `world_ui`
  widgets and `hud_messages` expose `<block>_schedule*(w)` calls you make
  inside the frame's single `ecs_scope`. Their state lives on entities
  (`CharacterTuning`, `CameraFollow`, `WorldUi`, `WorldUiLabel`) or in a
  resource (`HudMessages`), not in side tables — a scene-authored entity needs
  no adoption step. Ordering between them is by declared resource
  dependencies, so scheduling order in your scope mostly does not matter; the
  two that do are `camera_rig_schedule` after the systems that write
  `CameraRigInput`, and `character_schedule_fixed` inside the fixed loop.
- **Per-lane blocks are main-lane functions.** `touch_controls`, the world
  surfaces of `world_ui`, `vfx` (and the one-time
  `character_spawn`/`character_avatar_add`): called by the app in explicit
  order on the main lane, before or after the scope. No block calls
  `lane_sync()`.

Edges the app consumes (`WorldUiScreen.triggered`,
`PhysicsSensorContacts.entered/exited`) are read after the flush; systems raise
them, the app acts on them.

# What it provides

Everything in that directory is catalogued here. The debug cameras
(`camera_controllers`), collider draw (`gizmos_utils`) and `cloth_utils` are
dev utilities, not mechanics: they stayed in the engine.

**character** — a walking, jumping, swimming capsule with a kinematic or a
rigidbody mover, moving and one-way platforms, conveyors, step-up and a skinned
avatar driver. `character_controller_init`, `character_spawn`/`character_avatar_add` (or an
authored `CharacterTuning`), `character_schedule_fixed` per fixed step,
`character_schedule_frame` once per frame; the game writes `CharacterInput`.
Page: [character.md](character.md). Does NOT: pathfind, drive vehicles, ragdoll,
replicate, or move a camera.

**camera_rig** — `CameraFollow`, `CameraOrbit` and `CameraFirstPerson`
components that write the entity's `Camera`, fed by the `CameraRigInput`
resource and publishing `CameraRigState`. `camera_rig_init`, then
`camera_rig_schedule(w)` after the input systems. Page:
[camera-rig.md](camera-rig.md). Does NOT: lock the cursor, read input, or
choose a target.

**day_night** — a day/night cycle on the scene's sun: a clock, the astronomical sun and moon, a palette of moods
keyed by sun elevation, the key light with a sun/moon handoff and stepped turning (one shadow refresh per step), and
the scene's ambient and fog; drives `hz_stylized_sky` or `hz_atmosphere`. `day_night_init(w, r, a, sky)`,
`day_night_schedule(w)` before `ecs_transform_schedule`, `day_night_sync(w, r)` after the scope; gameplay reads
`DayNightState` (`sunrise`/`sunset` edges). Page: [day-night.md](day-night.md). Does NOT: advance seasons or the
moon's phase, or own weather.

**vfx** — composed effects: N elements (particles, a pooled flash light, a trail, a beam, a decal) played as one
thing on a stagger, and loops for continuous ones. `vfx_init` + `vfx_register(&VfxDesc)` once; systems on any lane call
`vfx_request(rq, asset, &VfxPlayDesc)` through the `VfxRequests` resource, the main lane drains it in `vfx_frame`.
Default particle elements share one table-driven material per texture (one draw per texture), with sprite / glow /
spark / ring / point render modes, a lit option (sun, ambient, height fog), arc shapes and a quality budget; a custom
material per element stays for GPU-simulated fields. `vfx_loop_*` drive wheel dust, skid strips and tethers;
`vfx_ambient_*` fills a camera-following volume of pollen or fireflies. The old `link` block is gone: a beam element
is the tether. Page: [vfx.md](vfx.md). Does NOT: generate textures (that is procgen), own a serialisation format of
its own, or attach to a bone.

**mesh_builder** — styled primitives (box, sphere, capsule, cone, cylinder, lathe, torus, sheet,
by transform or between two points) batched per material slot over `pg_geo`, `mesh_builder_add_pg`
for any `PgMesh`, `mesh_builder_finish` uploads the slots and spawns static colliders on a
`PhysicsFilter`. Page: [mesh-builder.md](mesh-builder.md). Does NOT: skin, simplify or own
materials.

**scatter** — pooled renderer instances plus optional colliders for placements: STATIC fields
(distance bands, LOD swap, throttled) and ENDLESS fields (a `pg_scatter` window recycled around a
focus), density and LOD distances per `renderer_quality`. Page: [scatter.md](scatter.md). Does NOT:
generate points (that is [[procgen/scatter]]) or implement endless colliders.

**camera_shake** — trauma-based shake (Eiserloh, GDC 2016), header-only:
`camera_shake_add(&s, amount)` on events, then per frame
`camera_shake_update(&s, tuning, dt, distance)` → `camera_shake_apply(&cam, o)`
on the camera you render with. Zero tuning = the tuned defaults. Page: [camera-shake.md](camera-shake.md). Does NOT: store the base
camera, run on worker lanes, or know about events.

**test_clock** — the deterministic capture clock of scripted runs:
`TestClock.steps=N` (set from `--commands`) steps N frames of exactly 1/60 s,
then freezes and logs `capture frozen at step N`. `test_clock_init(w)` once,
`test_clock_schedule(w, dt, ready)` each frame before the game's clock system,
read `TestClockFrame`. Page: [test-clock.md](test-clock.md). Does NOT: clamp,
pause or scale the game's dt, or count loading frames.


**touch_controls** — mobile thumbstick + jump button + the camera-exclusion
rule. `touch_controls_init(cfg)` → `touch_controls_update(&tc,&input,now)`
returning a `TouchControlsFrame` you ADD into your keyboard intent (`CharacterInput.move`/`jump`) and
scale into `CameraRigInput.look`/`zoom` → `touch_controls_draw`
inside the UI frame. Does NOT: feed the character itself, implement the FIXED
thumbstick (reserved), or do aim assist (only exposes `last_pan_time`).
Action buttons (`TouchControlsConfig.buttons[TOUCH_BUTTON_MAX]` + a
`TouchButtonPlacement` per orientation, anchored to a safe-area corner) report
`held`/`pressed`/`released` in `TouchControlsFrame.buttons`; a `look_through`
button's finger also adds to `camera_pan_px` (fire while aiming), and
`touch_controls_set_button(tc, i, visible, lit)` hides or highlights one per
frame. `stick_travel` > 1 is an over-push (sprint); `disable_pinch` keeps two
free fingers panning with the earliest. Drawing is separate from the logic:
`touch_controls_draw` is the default look for every widget,
`touch_controls_draw_stick`/`_button(i)`/`_jump_button` draw one, and a game with its
own look reads `touch_controls_stick`/`touch_controls_button(i)` (visible, held, lit,
centre, diameter: the exact circle the hit test uses) and draws over them.
Set `TouchControlsConfig.ui` so touches that START over pointer-capturing UI
stay with the UI (`ui_point_over_ui`); claims use `input_claim_touch`, which
cancels the touch's `MOUSE_LEFT` press so a stick drag is never a click. Its own
stick/jump circles are pointer-passthrough.

**world_ui** — world-anchored UI. Screen-space widgets are ECS entities:
`WorldUi` + `WorldUiLabel` (nametag with a fill bar, rising damage/value popup,
"[key] action" prompt, hotspot pin), projected, occlusion-tested, edge-clamped
and stacked by `world_ui_schedule(w)`, drawn by `world_ui_layout` with a style
or read from `WorldUiScreen`. The world surfaces stay: `BillboardGui`,
`SurfaceGui` (+`SurfaceGuiText`). Page: [world-ui.md](world-ui.md). Does NOT:
decide what to say, own game-specific widgets (crosshair, hit marker).

**hud_messages / hud_map** — banner, toast and hint channels with a priority
queue, delay, dedupe and enter/exit tweens (`hud_msg_push` from any system,
`hud_msg_layout` in the UI frame), and the map math: world to map px,
rotating or north-up, circle or rect clip, edge-clamped markers, a compass strip
placement and a polyline distance-field bake. Page: [hud.md](hud.md).

# Example usage

The canonical composition is the starter template, `templates/empty/src/empty/empty.c`
(read it, then [character.md](character.md) and [camera-rig.md](camera-rig.md)). The
main lane does OS input and the pointer lock; ONE scope schedules everything else:

```c
if (is_main_thread()) {
  input_update(...);
  *ecs_resource_get_mut(w, CameraRigInput) = (CameraRigInput){.look = ..., .zoom = ...};
}
i32 steps = fixed_timestep_begin(&s->sim_clock, dt);       // every lane, same dt
lane_sync();
ecs_scope(w) {                                              // the ONE scope
  ecs_assets_schedule(w);
  ecs_schedule(w, player_input_system);                     // writes CharacterInput
  ecs_physics_schedule(w);
  ecs_flush(w, dt);
  for (i32 k = 0; k < steps; k++) {
    character_schedule_fixed(w);
    ecs_flush_ex(w, &(EcsFlushDesc){.delta_time = fixed_dt, .steps = 1});
    ecs_rigidbody_step(w, fixed_dt);                        // all lanes
  }
  character_schedule_frame(w);
  camera_rig_schedule(w);
  ecs_transform_schedule(w);
  ecs_camera_schedule(w, &(EcsCameraScheduleDesc){.transform_sync_off = true});
  ecs_physics_sensor_schedule(w); world_ui_schedule(w); hud_msg_schedule(w);
  ecs_renderer_schedule(w);
  ecs_flush(w, dt);
}
renderer_update(...);
```

# Gotchas

- Function-shaped blocks (`touch_controls`, `world_ui`, `vfx`) are main-lane by
  contract: calling one off the main lane compiles, runs, and quietly races —
  wrap every call in `is_main_thread()` yourself. System-shaped blocks have no
  such trap.

- **Keep the whole frame inside the one `ecs_scope`, gameplay included.**
  `ecs_begin` sets `world->defer` and `ecs_end` drains it, so inside the scope
  a structural change queues and lands at the end; outside it, an add or remove
  relocates the entity immediately and every component pointer already held for
  it now points at another entity's data. The symptom is never a crash — it is
  a wrong number. `ecs_scope` is not "for inline `ecs_foreach`": it is the
  frame's defer window.

- UI text is safe to build on the stack: `ui_str_c` copies into frame-scoped
  storage. It did not always, and a dead stack buffer drew as plausible garbage
  rather than as nothing.

- **A collider does not follow its transform unless something makes it.**
  `ecs_physics_sync_schedule` is that something — it syncs pose always, and
  scale only when the transform's scale is uniform, because a shape authored
  with real half-extents already carries its size. Schedule it if anything in
  your world moves and collides.
- A system that reads `PhysicsSensorContacts` declares
  `EcsIn(PhysicsSensorContacts)`, which orders it after the contact scan; an
  undeclared read sees last frame's lists.
- Sensor results reflect poses as of the end of the previous physics step: the
  layers are rebuilt by `ecs_physics_schedule` once per frame.
- `billboard_place` must be followed by `ui_surface_update_all(ui, cam)` before
  the content scope draws, or the billboard renders at its previous rotation
  (identity on frame one) instead of camera-facing.
- Marker tags (`CharacterOneWay`, `CharacterSurface`) are discovered by
  component shape via `ecs_query`, never by entity name.
