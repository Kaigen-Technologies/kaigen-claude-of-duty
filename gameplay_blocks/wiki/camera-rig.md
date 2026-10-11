---
title: Camera Rig
description: does the engine have a third-person follow camera, an orbit camera with damping and fly-to, and a first-person camera with head bob and recoil — the three rig components, the input resource and the state it publishes
resource: src/gameplay/camera_rig/camera_rig.h
tags: [gameplay, camera, rig, controls]
updated: 2026-10-03
---

# What it provides

- `CameraFollow` (third person): damped pivot with look-ahead and shoulder offset, free or target-driven yaw, a boom with sphere-cast
  collision against the static layer, ground clearance, a floor (the sea), speed pull and FOV kick, `snap` to place at once.
- `CameraOrbit`: orbit-around-a-target damping per 60 Hz frame, polar and distance limits, auto-rotate, `goto_request` fly-to
  (easeInOutCubic) or jump-to; `camera_orbit_set_view(co, position, target)` derives azimuth, polar and distance from a camera pose.
- `CameraFirstPerson`: eye at the target's `WorldTransform` + `CharacterTuning.eye_height`, head bob, landing dip and punch springs,
  recoil, ADS zoom, FOV kick while running, the death slump.
- All three apply `camera_shake` trauma (`CameraRigInput.shake`, see [[camera-shake]]) and write the entity's `Camera` component directly.
- `CameraRigInput` (resource) is what the game writes each frame, whole: `look` (radians turned right and tilted up), `zoom` (log factor:
  distance *= exp(zoom)), `shake`, FP `recoil`/`punch`/`fov_kick`/`dead`, orbit `cancel_fly`, and a target for a Follow with no entity.
- `CameraRigState` (resource) is what the last rig left: position, rotation, focus, `yaw` (use it as `CharacterInput.heading`), pitch,
  fov, distance, `ready`.

# Example usage

```c
camera_rig_init(w);   // main lane, once: the two resources
// scene: an entity with Camera + CameraFollow (focus_height, pitch, distance, boom_radius, ...); bind the target at runtime
ecs_sys_get_component_mut(CameraFollow, i)->target = player;

// frame, inside the scope, after the systems that write CameraRigInput and after the characters
ecs_schedule_single(w, my_rig_input_system);    // writes CameraRigInput
character_schedule_frame(w);
camera_rig_schedule(w);
ecs_transform_schedule(w);
ecs_camera_schedule(w, &(EcsCameraScheduleDesc){.transform_sync_off = true});   // the rig wrote Camera
```

The starter template writes `CameraRigInput` on the main lane before the scope (pointer lock stays with the game).

# Gotchas

- The rigs never lock the cursor: the game calls `os_lock_mouse`, skips the lock frame's warp delta and releases it for the editor.
- The rigs run `ecs_schedule_single`, so the systems that write `CameraRigInput` must be scheduled before `camera_rig_schedule`, and
  the Camera must not be synced from the transform afterwards: pass `transform_sync_off` to `ecs_camera_schedule`.
- Boom and ground queries use the static layer of `EcsPhysicsWorld`; `boom_mask` and `ground_mask` pick the `PhysicsFilter` categories
  that count (0 means all), so give terrain and props different categories when only the terrain should hold the camera.
- A `CameraOrbit` with `distance <= 0` and no `goto_request` leaves the Camera alone; set `disabled` (or `CameraFollow.disabled`) to
  hand the Camera to game code.
- View yaw follows forward = (-sin, 0, -cos); the Follow boom sits at `(sin yaw cos pitch, sin pitch, cos yaw cos pitch)` from the pivot,
  pitch positive above. First person: `yaw` positive turns left, `pitch` positive looks up.
- Zero numeric fields mean off or instant unless the header comment names a default; every tuning field is in meters and radians.
