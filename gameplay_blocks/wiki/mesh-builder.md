---
title: Mesh Builder
description: does the engine have a styled primitive batcher - box, sphere, capsule, cone, cylinder, lathe, torus, sheet by transform or between two points, merged per material slot, uploaded in one call, with static colliders created from the same calls?
resource: src/gameplay/mesh_builder/mesh_builder.h
tags: [gameplay, procgen, mesh, collider, authoring]
updated: 2026-10-03
---

# What it provides

- `MeshBuilder` over `pg_geo`: one `PgMesh` per SLOT (a slot is a material or draw batch you pick, not a `PgOut` part, so it is not capped at 96 parts). Slots begin lazily on first use.
- `MeshBuilderStyle` per call: `slot`, linear `color` (`mesh_builder_hex(0xRRGGBB)`), `paint` + `paint_weight` fn, `shade_lo/hi`, `aux` word or `aux_fn`, `deform` fn, `flat`, and an optional `collider` kind.
- Primitives by transform: `mesh_builder_box`, `_round_box`, `_sphere`, `_polyhedron`, `_capsule`, `_cylinder`, `_lathe`, `_torus`, `_sheet`; between two points: `_capsule_between`, `_cylinder_between`, `_cone_between` (`mesh_builder_segment` is the placement).
- `mesh_builder_add_pg(b, style, xform, &pgmesh)`: the escape hatch for any `PgMesh` you built yourself (a boulder, a car body) under the style.
- Collision without geometry: `mesh_builder_collider_box/_cylinder/_sphere/_capsule`.
- `mesh_builder_finish` uploads every slot and spawns the static colliders (box, sphere, capsule direct; cylinders as shared trimesh blobs) on `MeshBuilderFinishDesc.collider_filter`.

# Example usage

```c
MeshBuilder mb;
mesh_builder_begin(&mb, arena, &(MeshBuilderDesc){.slot_count = MATERIAL_COUNT});
MeshBuilderStyle wall = {.slot = MAT_WALL, .collider = MESH_BUILDER_COLLIDER_BOX};
mesh_builder_box(&mb, &wall, m4_trs(center, quat_identity(), V3_ONE), size);
mesh_builder_capsule_between(&mb, &(MeshBuilderStyle){.slot = MAT_PIPE}, from, to, 0.05f, 2, 6);
MeshBuilderResult built = mesh_builder_finish(
    &mb, &(MeshBuilderFinishDesc){.renderer = r, .world = w, .alloc = &alloc, .uv_per_metre = 1.0f});
// built.slots[MAT_WALL].parts[0 .. part_count) are RendererSubMesh handles; bounds and vertex_count alongside
```

# Gotchas

- Main lane only: `begin`, the primitives and `finish` are one-caller; `finish` asserts `is_main_thread()` (renderer upload, the ECS world outside a scope).
- A slot past the u16 vertex limit yields several `parts`; assert `part_count == 1` where you hold one handle per slot.
- A primitive's collider is that primitive placed by `xform` (scale becomes radii and extents); lathe, torus and sheet add none.
- `world` may be NULL only when no collider was added; `alloc` must outlive the result (trimesh blobs live in it).
- Sphere is unit radius, so the transform's scale gives the size; the style colour is linear, `mesh_builder_hex` converts from sRGB.
- Box UVs come from `pg_box_grid`, not `pg_box`; a material that samples box UVs differs from a hand-built `pg_box`.
