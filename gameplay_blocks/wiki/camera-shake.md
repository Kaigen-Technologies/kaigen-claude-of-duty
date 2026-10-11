---
title: Camera Shake
description: trauma-based camera shake — events add trauma, trauma decays linearly, shake = trauma^exponent over smooth 1D gradient noise per channel; tuned defaults
resource: src/gameplay/camera_shake/camera_shake.h
tags: [gameplay, camera, juice]
updated: 2026-09-29
---

# What it provides

- `CameraShake` state (`trauma`, noise `phase`, `seed`), `camera_shake_init`,
  `camera_shake_add(&s, amount)` (clamped to 1), `camera_shake_amount`.
- `camera_shake_update(&s, CameraShakeTuning, dt, distance)` →
  `CameraShakeOffset` (camera-local `translate`, `roll`/`pitch`/`yaw`).
- `camera_shake_apply(&cam, offset)`: moves along camera right/up, turns in
  the camera's own frame. Call `camera_update` afterwards.

The model is Squirrel Eiserloh's "Juicing Your Cameras With Math" (GDC 2016,
[slides](https://media.gdcvault.com/gdc2016/Presentations/Eiserloh_Squirrel_JuicingYourCameras.pdf)):
trauma in [0,1], added by events, decaying linearly; shake = trauma² (or ³),
so small hits barely register and big ones compound; smooth noise per channel
instead of random offsets; rotation reads as shake without pushing the
subject off-centre.

# Defaults

| Field | Default |
|---|---|
| `decay` | 1.6 trauma/s, linear |
| `exponent` | 2: shake = trauma² |
| `frequency` | 15 noise lattice points/s (1D Perlin: smooth, no periodic pattern) |
| `translation` | 0.018 per metre of camera distance at noise peak 1, along camera right/up |
| `roll` | 0.02 rad |

# Example usage

```c
CameraShake shake;                      // app state, not a global
camera_shake_init(&shake, 1);
// on an event
camera_shake_add(&shake, 0.25f * kills);
// per frame, after the camera rig wrote `cam`
Camera render_cam = cam;
CameraShakeOffset o = camera_shake_update(&shake, (CameraShakeTuning){0}, dt,
                                          v3_length(v3_sub(cam.pos, focus)));
camera_shake_apply(&render_cam, o);
camera_update(&render_cam, width, height);
```

# Gotchas

- Apply to a COPY the controller never reads back, or offsets integrate into
  its state and the camera wanders.
- Offsets depend only on trauma and phase, so the result does not change with
  how dt is split (`test_camera_shake_dt_independent`).
- The noise phase wraps at `CAMERA_SHAKE_PERIOD` (4096 lattice points), where
  the gradient hash repeats: seamless, and f32 keeps sub-millesimal
  resolution. A 65536 period would have quantized the phase to 1/128.
