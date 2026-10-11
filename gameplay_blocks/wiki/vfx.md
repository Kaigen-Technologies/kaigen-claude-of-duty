---
title: VFX
description: how do I play a composed effect (particles, a flash light, a beam, a trail, a decal) from a system on any lane, run a continuous one (wheel dust, skid marks, a tether), or fill a camera-following ambient volume — the vfx block's request buffer, table material, loops, element kinds and quality budget
resource: src/gameplay/vfx/vfx.h
tags: [gameplay, vfx, particles, lights, trails, decals]
updated: 2026-10-04
---

# What it provides

- An effect is a `VfxDesc` of up to 32 `VfxElemDesc`, registered once (`vfx_register`) and played many times.
  An element is a `kind`: `VFX_ELEM_PARTICLES`, `_LIGHT` (a pooled point light fading over `life`, `(1 - t)^curve`),
  `_TRAIL` (a strip over `TrailSystem` fed by a loop), `_BEAM` (a segment between `at` and `to`: tracer, rail, tether) and
  `_DECAL` (a pooled ring of projected marks). Zero means off for every field, so a literal spells only what it uses.
- Producers that are systems declare `EcsResIn(VfxRequests)` and call `vfx_request(rq, asset, &VfxPlayDesc)` from any
  lane; the main lane drains the buffer in `vfx_frame(w, camera, dt)`, then the game calls `particle_system_update`.
  `vfx_init(w, &VfxWorldDesc)` sets the resource; `vfx_world_create` is the ECS-free form (tests, per-generation worlds).
- Default particle elements share ONE table-driven material per texture (`vfx_default.glsl`): a hundred effects draw in a
  handful of calls. Render modes `VFX_RENDER_SPRITE/GLOW/SPARK/RING/POINT`; `lit` shades by `vfx_set_atmosphere`
  (sun, ambient, height fog); `ramp_tint` multiplies the colour ramp by the play's colour. `material` + `user` keep a custom
  material per element (a GPU-simulated field); `fixed` places an exact count instead of thinning it by the budget.
- Shapes: cone, or `PARTICLE_SHAPE_ARC` (an aimed spray around `yaw`, a ring at `spread_deg` 180, `uniform_angle`, `back`,
  `height`). `VfxPlayDesc` scales speed, size, height and radius apart, `count_scale` thins a crowd, `color` and `user`
  (a `v4` for a custom material) go per play.
- Loops: `vfx_loop_start` / `_move` / `_stop`, `vfx_loop_set_rate` (scales every `rate`, the fractional carry is kept),
  `_set_params` (colour, heading, scales of what it emits now), `_set_target` (a beam's far end), `_trail_point` /
  `_trail_cut` / `_trail_clear` (a strip's path). Loops share the asset's emitters: starting one costs no pool.
- `vfx_ambient_create` / `_set` / `_destroy`: immortal motes whose positions are a stateless function of time, wrapped in a box
  around the camera above the particle floor, one material, one draw.
- Quality: counts and rates are multiplied by `particle_system_budget` (LOW 0.3, MEDIUM 0.6, else 1); the ambient volume
  takes it as its density. `vfx_set_light_unit` is the candela per light intensity unit (a game's exposure convention).

# Example usage

```c
vfx_init(w, &(VfxWorldDesc){.particles = ps, .renderer = r, .trails = trails, .max_lights = 3});   // main lane, once
VfxDesc d = {.name = "impact", .count = 2};
d.elem[0] = (VfxElemDesc){.render = VFX_RENDER_SPARK, .additive = 1, .count = 12, .life = {0.2f, 0.4f}, .size = {0.05f, 0},
                          .speed = {4, 9}, .gravity = {0, -9, 0}, .velocity_stretch = 0.1f, .stops = 1, .color = {{0, {3, 2, 1, 1}}}};
d.elem[1] = (VfxElemDesc){.kind = VFX_ELEM_LIGHT, .light = {.color = {1, 0.6f, 0.3f}, .intensity = 5, .radius = 4, .life = 0.1f}};
HzHandleT(VfxAsset) impact = vfx_register(vfx_world(w), &d);
// in a system: ecs_system(my_system, ..., EcsResIn(VfxRequests))
vfx_request(ecs_sys_get_resource(VfxRequests), impact, &(VfxPlayDesc){.at = hit, .dir = normal});
// main lane, after the flush: vfx_frame(w, cam, dt); particle_system_update(ps, ...); trail_system_update / _prepare
```

# Gotchas

- `vfx_register` is once per effect at start-up: the pools are sized for it and stay reserved until `vfx_unregister`.
  An element's derived `pool` fits ONE play; set `pool` for the crowd.
- A request is played at the next `vfx_frame`; the buffer drops plays past `max_requests` per frame and logs it.
- Trails, lights, decals and bursts are main-lane state, so `vfx_frame` and `vfx_play_ex` are main-lane; a system never
  calls them. `vfx_request` and the loop setters are safe from any lane for their own loop.
- A play's `scale` multiplies `speed` (the speed along the shape), not `vspeed` or `scatter`.
- A decal's textures must be loaded when the effect is registered; a light's `color` is sRGB (`vfx_light_hue` folds a
  linear HDR colour into hue and peak).
- A loop's trail points are queued (8 a frame) and a strip outlives its loop until its points age out.
