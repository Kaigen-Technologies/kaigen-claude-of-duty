---
title: Scatter
description: how do I place thousands of props (trees, rocks, shrubs) as pooled renderer instances with optional colliders, LOD swap and distance bands, or as an endless field recycled around the player, with density and LOD distances per quality level?
resource: src/gameplay/scatter/scatter.h
tags: [gameplay, scatter, lod, procgen, quality]
updated: 2026-10-03
---

# What it provides

- Two field modes behind one resource (`ScatterState`, up to `SCATTER_FIELD_MAX` fields, up to 32 species each):
  - STATIC: you author the placements (from [[procgen/scatter]] points or game code) and `scatter_static_spawn` creates the rows (one entity per part and LOD) and colliders once; distance bands show or hide them, throttled (0.4 s or 10 m of focus movement).
  - ENDLESS: a `pg_scatter` window of cells around a focus, pooled entities recycled when the focus crosses a cell; a `window` callback runs first so the game authors landmarks and reserves ground.
- Species data: `ScatterSpeciesDesc` has variants `[variant][tint][lod]` of `ScatterPart`s, `lod_distance[lod]` per quality level with `lod_band` between LODs, `density_scale` per level, a collider template, and `tint_param` for a per-placement vec4 on its own material.
- Quality: fields read `renderer_quality(r)` (`scatter_quality_level`, HIGH until the first `renderer_set_quality`) or a `quality_override`; `scatter_field_set_quality` re-reads it.
- `ScatterPlacement` carries species, transform, variant, tint, `instance_tint`, the point's rolls and an optional per-placement collider (capsule, sphere, box).
- `scatter_field_placements`, `_rebuilds`, `_rebuild_cost` expose an endless window to the game's own systems (lights, fx).

# Example usage

```c
scatter_init(w);                                        // main lane, after ecs_renderer_init
u32 field = scatter_field_create(w, r, &(ScatterFieldDesc){
    .mode = SCATTER_STATIC, .species = tables, .species_count = n, .follow_camera = true, .lod_margin = 12.0f});
// later, main lane (a deferred call is fine): the draw data in `species`, same count as the field's
scatter_static_spawn(w, r, field, species, placements, count);
// inside the frame's scope, once, after the camera:
scatter_schedule(w);
// a new world, from a system holding EcsResInOut(ScatterState):
scatter_field_clear(scatter_state, field);              // rows and colliders go this frame; spawn again next frame
```

# Gotchas

- `scatter_schedule` runs camera, tick, cull, pool, then the clear systems: call `scatter_field_clear` from a system that runs before it in the frame.
- A static spawn's rows start hidden and show at the next tick (the spawn forces one).
- `lod_distance` is the xz far edge of each LOD, 0 = unbounded; LOD n+1 starts `lod_band` (plus `lod_margin`) short of LOD n's edge.
- Endless species support one variant and one tint (pooled entities keep their parts); endless colliders are not implemented.
- A static field's `density_scale` is only read by the game when it calls `pg_scatter`; endless fields apply it on rebuild.
- `tint_param` gives each placement its own material instance: use it only where per-instance colour matters.
