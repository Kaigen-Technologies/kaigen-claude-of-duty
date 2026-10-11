---
title: Character
description: does the engine have a walking, jumping, swimming character controller with moving and one-way platforms, conveyors, step-up and a skinned avatar driver — the kinematic capsule or rigidbody mover, its tuning, input and frame order
resource: src/gameplay/character/character.h
tags: [gameplay, character, physics, controller]
updated: 2026-10-03
---

# What it provides

- Two movers on one component set: `CHARACTER_MOVER_KINEMATIC` (swept capsule, collide-and-slide over contact planes, steep slopes as
  walls, step-up, stick-to-floor; no rigidbody) and `CHARACTER_MOVER_RIGIDBODY` (a rotation-locked dynamic body steered through its
  velocity, so `PhysicsSensor` and other bodies see it).
- `CharacterTuning` (reflected, scene-authorable): speeds, `CHARACTER_MOVE_TIME_TO_SPEED` or `CHARACTER_MOVE_QUAKE` ground control,
  gravity, jump height, coyote time and jump buffer, slope limit, step height, facing mode (movement, heading, input), swim.
  Zero fields take the documented defaults, in meters.
- `CharacterInput` is the per-frame intent the game writes: `move` (in the heading frame), `heading`, `run`, `jump`, `teleport`,
  `add_velocity` (jump pad, knockback). `Character` is runtime state; its counters (`jumps`, `landings`, `footsteps`, `launches`,
  `teleports`) only grow, so a consumer diffs against the value it saw last.
- Platforms: the character rides the body it stands on and carries its pose delta; `CharacterOneWay` marks a
  platform solid from above only; `CharacterSurface{local_velocity}` makes a conveyor or bounce pad.
- `character_avatar_add` attaches a skinned model with idle, run and fall clips driven from the `Character` state (default model in
  `assets/gameplay_blocks/character/`). `character_spawn` builds a character entity in code; a scene may author `CharacterTuning` instead.
- `character_spawn_height(w, x, z, from_y, max_drop, fallback)` finds the floor under a spawn point.

# Example usage

```c
// init, main lane, after ecs_physics_init
character_controller_init(w);
EcsEntity player = character_spawn(w, "Player", (v3){0, 0.1f, 0}, &(CharacterTuning){.walk_speed = 4, .run_speed = 7});
character_avatar_add(w, player, &(CharacterAvatarDesc){0});

// frame, one ecs_scope: the input system writes CharacterInput (heading from CameraRigState.yaw)
ecs_schedule(w, player_input_system);
ecs_physics_schedule(w);   // the layers the character collides with
ecs_flush(w, dt);          // BEFORE the loop: ecs_flush_ex re-runs every pending system
for (i32 k = 0; k < steps; k++) {
  character_schedule_fixed(w);
  ecs_flush_ex(w, &(EcsFlushDesc){.delta_time = fixed_dt, .steps = 1});
  ecs_rigidbody_step(w, fixed_dt);   // all lanes
}
character_schedule_frame(w);         // pose interpolation, avatar animation, before ecs_transform_schedule
```

`CharacterFrame.alpha` (resource) carries the render interpolation: write `fixed_timestep_alpha` into it on the main lane before the scope.
The starter template (`templates/empty`) is the worked use.

# Gotchas

- `character_schedule_fixed` is per fixed step, `character_schedule_frame` once per frame; both inside the scope. The avatar system
  is `ecs_schedule_single` (it writes other entities' animation layers).
- Kinematic: `Character.position` is authoritative and the root `WorldTransform` is the interpolated render pose. Writing the root
  `WorldTransform` from outside (the `--commands teleport`) teleports the character; `CharacterInput.teleport` does it with a facing.
- The collision layers are a per-frame snapshot from `ecs_physics_schedule`: schedule it before the fixed loop, and a moving platform's
  pose lags the character by one frame.
- `jump` is a request consumed when a step arms the jump buffer: write `ci->jump |= pressed_this_frame` so a frame with zero steps does
  not drop the press.
- `heading` is the view yaw (forward = (-sin, 0, -cos)); `Character.yaw` 0 faces +Z. Feed the heading from `CameraRigState.yaw`.
- The rigidbody mover reads gravity from the world's `EcsRigidBodyWorld` settings and needs `ecs_rigidbody_step` after the fixed systems.
  Its body never rotates: `character_avatar_add` hangs the avatar under a `CharacterVisual` pivot that carries the facing.
- Not covered: pathfinding, vehicles, ragdoll, replication, a camera (see [[camera-rig]]).
