// Claude of Duty world assembler: the palette, prop prototypes and the recording API every kit module writes into.

// ---------------------------------------------------------------- palette ---

#define CW_F_DOUBLE 1u
#define CW_F_TRANSPARENT 2u
#define CW_F_NO_MASKS 4u // the palette entry has no vertex masks

// key, library surface, physics surface, tint 0xRRGGBB (0 = the library's), metres per tile (0 = the library's), flags
#define CW_KEY_LIST(X)                                                                                                     \
  X(PLASTER_CREAM, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0xcfc0a4, 2.35f, 0)                                              \
  X(PLASTER_SAND, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0xb9a582, 2.1f, 0)                                                \
  X(PLASTER_BLUE, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0x8f9aa0, 2.2f, 0)                                                \
  X(PLASTER_PINK, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0xc09a86, 2.5f, 0)                                                \
  X(PLASTER_WHITE, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0xd8d2c4, 1.9f, 0)                                               \
  X(BRICK, COD_MAT_BRICK, COD_SURFACE_CONCRETE, 0xa8846c, 1.3f, 0)                                                        \
  X(BRICK_FINE, COD_MAT_BRICK, COD_SURFACE_CONCRETE, 0x9c8068, 0.62f, 0)                                                  \
  X(CONCRETE, COD_MAT_CONCRETE, COD_SURFACE_CONCRETE, 0xa9a49a, 2.5f, 0)                                                  \
  X(CONCRETE_PROP, COD_MAT_CONCRETE, COD_SURFACE_CONCRETE, 0xa5a096, 0.9f, 0)                                             \
  X(CONCRETE_DARK, COD_MAT_CONCRETE, COD_SURFACE_CONCRETE, 0x7d7a73, 2.2f, 0)                                             \
  X(ROOF_SCREED, COD_MAT_CONCRETE, COD_SURFACE_CONCRETE, 0xb5a992, 2.8f, 0)                                               \
  X(FLOOR_CONCRETE, COD_MAT_CONCRETE_FLOOR, COD_SURFACE_CONCRETE, 0x9e9a91, 3.0f, 0)                                      \
  X(TILE_FLOOR, COD_MAT_TILE, COD_SURFACE_CONCRETE, 0xa9a08d, 1.4f, 0)                                                    \
  X(ROAD_DUST, COD_MAT_GRAVEL, COD_SURFACE_DIRT, 0xc9b896, 2.2f, 0)                                                       \
  X(ASPHALT, COD_MAT_ASPHALT, COD_SURFACE_CONCRETE, 0x9d968a, 3.2f, 0)                                                    \
  X(ROAD_RUT, COD_MAT_ASPHALT, COD_SURFACE_CONCRETE, 0x6f6a62, 1.5f, 0)                                                   \
  X(SAND, COD_MAT_SAND, COD_SURFACE_SAND, 0, 2.6f, 0)                                                                     \
  X(DIRT, COD_MAT_DIRT, COD_SURFACE_DIRT, 0, 2.4f, 0)                                                                     \
  X(GRAVEL, COD_MAT_GRAVEL, COD_SURFACE_DIRT, 0, 1.8f, 0)                                                                 \
  X(DUST_SKIRT, COD_MAT_GRAVEL, COD_SURFACE_DIRT, 0xa89d86, 1.1f, 0)                                                      \
  X(METAL_RUST, COD_MAT_METAL_RUST, COD_SURFACE_METAL, 0, 1.1f, 0)                                                        \
  X(METAL_RUST_PROP, COD_MAT_METAL_RUST, COD_SURFACE_METAL, 0x9d7c66, 0.4f, 0)                                            \
  X(METAL_BLUE, COD_MAT_METAL_PAINTED, COD_SURFACE_METAL, 0x6d8390, 1.3f, 0)                                              \
  X(METAL_GREEN, COD_MAT_METAL_PAINTED, COD_SURFACE_METAL, 0x76806a, 1.3f, 0)                                             \
  X(METAL_DARK, COD_MAT_METAL_PAINTED, COD_SURFACE_METAL, 0x4a4a48, 1.0f, 0)                                              \
  X(STEEL, COD_MAT_METAL_BRUSHED, COD_SURFACE_METAL, 0, 0.9f, 0)                                                          \
  X(CORRUGATED, COD_MAT_CORRUGATED, COD_SURFACE_METAL, 0, 2.2f, 0)                                                        \
  X(WOOD, COD_MAT_WOOD, COD_SURFACE_WOOD, 0, 1.8f, 0)                                                                     \
  X(WOOD_PROP, COD_MAT_WOOD, COD_SURFACE_WOOD, 0xb08a5e, 0.55f, 0)                                                        \
  X(WOOD_PROP_DARK, COD_MAT_WOOD, COD_SURFACE_WOOD, 0x7d6244, 0.5f, 0)                                                    \
  X(WOOD_DARK, COD_MAT_WOOD, COD_SURFACE_WOOD, 0x8a6a4a, 1.5f, 0)                                                         \
  X(WOOD_PALE, COD_MAT_WOOD, COD_SURFACE_WOOD, 0xc0a482, 1.2f, 0)                                                         \
  X(FABRIC_RED, COD_MAT_FABRIC, COD_SURFACE_FABRIC, 0xa2564a, 0.26f, CW_F_DOUBLE)                                         \
  X(FABRIC_TEAL, COD_MAT_FABRIC, COD_SURFACE_FABRIC, 0x5f8a8c, 0.26f, CW_F_DOUBLE)                                        \
  X(FABRIC_CREAM, COD_MAT_FABRIC, COD_SURFACE_FABRIC, 0xbcb298, 0.26f, CW_F_DOUBLE)                                       \
  X(BURLAP, COD_MAT_BURLAP, COD_SURFACE_FABRIC, 0xa2957a, 0.16f, 0)                                                       \
  X(RUBBER, COD_MAT_RUBBER, COD_SURFACE_RUBBER, 0, 0.45f, 0)                                                              \
  X(GLASS, COD_MAT_GLASS, COD_SURFACE_GLASS, 0, 2.0f, CW_F_TRANSPARENT | CW_F_NO_MASKS)                                                   \
  X(FOLIAGE, COD_MAT_FOLIAGE, COD_SURFACE_FOLIAGE, 0, 0.0f, CW_F_DOUBLE)                                   \
  X(WINDOW_VOID, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0x474441, 1.1f, 0)                                                 \
  X(INTERIOR_SHELL, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0x5f5b56, 1.6f, 0)                                              \
  X(WINDOW_GLASS, COD_MAT_GLASS, COD_SURFACE_GLASS, 0, 2.0f, CW_F_TRANSPARENT | CW_F_NO_MASKS)                                            \
  X(PLYWOOD, COD_MAT_WOOD, COD_SURFACE_WOOD, 0x7a6549, 0.62f, 0)                                                          \
  X(EMISSIVE_WARM, COD_MAT_PLASTER, COD_SURFACE_GLASS, 0xfff0d8, 0.4f, CW_F_NO_MASKS)                                                 \
  X(WINDOW_GLOW, COD_MAT_PLASTER, COD_SURFACE_PLASTER, 0x6a5a45, 1.2f, 0)                                                 \
  X(LAMP_LENS, COD_MAT_GLASS, COD_SURFACE_GLASS, 0, 1.0f, CW_F_TRANSPARENT | CW_F_NO_MASKS)

typedef enum CwKey {
#define X(n, ...) CW_##n,
  CW_KEY_LIST(X)
#undef X
      CW_KEY_COUNT,
} CwKey;

typedef struct CwPaletteEntry {
  const char *name;
  u32 material;
  u32 surface;
  u32 tint;
  f32 scale;
  u32 flags;
} CwPaletteEntry;

hz_internal const CwPaletteEntry cw_palette_table[CW_KEY_COUNT] = {
#define X(n, mat, surf, tint, scale, flags) {#n, mat, surf, tint, scale, flags},
    CW_KEY_LIST(X)
#undef X
};

hz_internal const CwPaletteEntry *cw_palette(u32 key) { return &cw_palette_table[key]; }

// the palette's remaining opts: weather, wear and roughness replace the library's; zero fields keep it
typedef struct CwPaletteOpts {
  f32 normal_strength;
  f32 detile;
  v4 weather;
  v4 wear;
  v2 roughness;
  u32 emissive;
  f32 emissive_intensity;
  f32 opacity;
  f32 env_intensity;
  b32 has_weather, has_wear, has_roughness;
} CwPaletteOpts;

#define CW_WEATHER(a, b, c, d) .weather = {a, b, c, d}, .has_weather = true
#define CW_WEAR(a, b, c, d) .wear = {a, b, c, d}, .has_wear = true
#define CW_ROUGH(a, b) .roughness = {a, b}, .has_roughness = true

hz_internal const CwPaletteOpts cw_palette_opts_table[CW_KEY_COUNT] = {
    [CW_PLASTER_CREAM] = {CW_WEATHER(0.4f, 0.5f, 1.4f, 0.55f)},
    [CW_PLASTER_SAND] = {CW_WEATHER(0.45f, 0.5f, 1.5f, 0.6f)},
    [CW_PLASTER_BLUE] = {CW_WEATHER(0.4f, 0.55f, 1.5f, 0.6f)},
    [CW_PLASTER_PINK] = {CW_WEATHER(0.45f, 0.5f, 1.3f, 0.55f)},
    [CW_PLASTER_WHITE] = {CW_WEATHER(0.3f, 0.35f, 0.9f, 0.5f)},
    [CW_BRICK_FINE] = {CW_WEATHER(0.45f, 0.5f, 0.8f, 0.6f)},
    [CW_CONCRETE_PROP] = {.normal_strength = 1.3f, CW_WEATHER(0.45f, 0.5f, 0.35f, 0.55f)},
    [CW_CONCRETE_DARK] = {CW_WEATHER(0.4f, 0.6f, 1.2f, 0.6f)},
    [CW_ROOF_SCREED] = {CW_WEATHER(0.6f, 0.2f, 0.3f, 0.45f)},
    [CW_ROAD_DUST] = {.detile = 0.9f, CW_WEATHER(0.4f, 0.04f, 0.08f, 0.14f), CW_WEAR(0.0f, 0.5f, 0.45f, 0.0f)},
    [CW_ASPHALT] = {.detile = 0.6f, CW_WEAR(0.0f, 0.55f, 0.45f, 0.0f)},
    [CW_ROAD_RUT] = {.detile = 0.7f, CW_WEATHER(0.3f, 0.5f, 0.15f, 0.28f), CW_WEAR(0.0f, 0.55f, 0.45f, 0.0f)},
    [CW_SAND] = {.detile = 0.7f, CW_WEAR(0.0f, 0.45f, 0.45f, 0.0f)},
    [CW_DIRT] = {.detile = 0.8f, CW_WEAR(0.0f, 0.5f, 0.45f, 0.0f)},
    [CW_GRAVEL] = {CW_WEAR(0.0f, 0.5f, 0.45f, 0.0f)},
    [CW_DUST_SKIRT] = {CW_WEATHER(0.3f, 0.0f, 0.0f, 0.16f), CW_WEAR(0.0f, 0.9f, 0.7f, 0.0f)},
    [CW_METAL_RUST_PROP] = {.normal_strength = 1.35f, CW_WEATHER(0.5f, 0.35f, 0.3f, 0.5f)},
    [CW_WOOD_PROP] = {.normal_strength = 1.45f, CW_WEATHER(0.35f, 0.3f, 0.35f, 0.5f)},
    [CW_WOOD_PROP_DARK] = {.normal_strength = 1.45f, CW_WEATHER(0.35f, 0.35f, 0.4f, 0.55f)},
    [CW_BURLAP] = {CW_WEATHER(0.5f, 0.3f, 0.4f, 0.5f)},
    [CW_WINDOW_VOID] = {CW_ROUGH(1.0f, 0.15f), CW_WEATHER(0.2f, 0.7f, 0.2f, 0.7f)},
    [CW_INTERIOR_SHELL] = {CW_ROUGH(1.0f, 0.1f), CW_WEATHER(0.25f, 0.8f, 0.3f, 0.65f)},
    [CW_WINDOW_GLASS] = {CW_ROUGH(0.3f, 0.06f), .opacity = 0.16f, .env_intensity = 2.1f},
    [CW_PLYWOOD] = {.normal_strength = 1.2f, CW_WEATHER(0.5f, 0.45f, 0.5f, 0.6f)},
    [CW_EMISSIVE_WARM] = {.emissive = 0xffd39a, .emissive_intensity = 12.0f},
    [CW_WINDOW_GLOW] = {.emissive = 0xffb066, .emissive_intensity = 1.1f},
    [CW_LAMP_LENS] = {.emissive = 0xffc47a, .opacity = 0.5f},
};

#undef CW_WEATHER
#undef CW_WEAR
#undef CW_ROUGH

hz_internal u32 cw_surface_of(u32 key) { return cw_palette(key)->surface; }

// ------------------------------------------------------------------ props ---

// prototype ids in registration order: the order is the RNG order
#define CW_PROTO_LIST(X)                                                                                                   \
  X(CRATE_A) X(CRATE_B) X(CRATE_C) X(CRATE_FLAT) X(BOX_CARD_A) X(BOX_CARD_B) X(BARREL_RUST) X(BARREL_BLUE) X(BARREL_WOOD)  \
  X(GAS_BOTTLE) X(BUCKET) X(JERRY_CAN) X(SANDBAG_A) X(SANDBAG_B) X(SANDBAG_C) X(JERSEY) X(BLOCK_BIG) X(BLOCK_SMALL)       \
  X(TYRE) X(TYRE_SMALL) X(PALLET) X(TABLE) X(TABLE_SMALL) X(STALL) X(SHELF) X(MATTRESS) X(CHAIR) X(CABINET) X(AC_UNIT)    \
  X(SAT_DISH) X(WATER_TANK) X(ROOF_VENT) X(LAMP_POST) X(LAMP_GLASS) X(BRICK_A) X(BRICK_B) X(ROCK_A) X(ROCK_B)            \
  X(SLAB_SHARD) X(REBAR) X(PLANK_A) X(PLANK_B) X(LITTER) X(DUST_SKIRT) X(BOTTLE) X(CAN) X(PALM_TRUNK) X(PALM_FROND)      \
  X(SHRUB) X(WEEDS) X(PLANTER) X(SIGN_BOARD) X(SIGN_HANG) X(POCK) X(WRECK) X(WHEEL_FLAT) X(GLASS_SHARDS) X(CINDER)       \
  X(TRAY) X(PRODUCE) X(CONDUIT_BOX) X(STOOL)

typedef enum CwProtoId {
#define X(n) CWP_##n,
  CW_PROTO_LIST(X)
#undef X
      CWP_COUNT,
} CwProtoId;

#define CW_PROTO_MAX_LODS 3

typedef struct CwProto {
  b32 defined;
  CwGeo geo;
  u32 key;
  f32 tilt, sink, skirt, max_dist;
  b32 cast_shadow, receive_shadow;
  u32 count; // instances placed
  f32 simplify;          // decimation error limit in metres (0 = the geometry as built)
  CwSimplifyStats simp;  // what decimation did
  u32 lod_count;         // levels including the source mesh
  CwGeo lods[CW_PROTO_MAX_LODS - 1];                 // the coarser levels, each decimated from the previous
  f32 lod_err[CW_PROTO_MAX_LODS];                    // metres of error against the source mesh, per level
  CwSimplifyStats lod_simp[CW_PROTO_MAX_LODS - 1];   // what each coarser level's decimation did
  HzHandleT(RendererSubMesh) lod_sub[CW_PROTO_MAX_LODS - 1];
} CwProto;

typedef struct CwProtoOpts {
  f32 tilt, sink, skirt, max_dist;
  b32 no_shadow;
  b32 no_receive;
  f32 simplify; // decimation error limit in metres, under what the closest view resolves
  f32 lod1, lod2; // decimation error limits in metres for the coarser levels (0 = none)
} CwProtoOpts;

hz_internal const char *const CW_PROTO_NAMES[] = {
#define X(n) #n,
    CW_PROTO_LIST(X)
#undef X
};

// -------------------------------------------------------------- recording ---

typedef enum CwPaint { CW_PAINT_NONE = 0, CW_PAINT_FACADE, CW_PAINT_PARTITION } CwPaint;

typedef struct CwOpts {
  b32 has_masks;
  v3 masks;
  u32 paint;
  f32 paint_p;
} CwOpts;

// the masks option: a literal [wear, grime, ao]
#define CW_M(a, b, c) (&(CwOpts){.has_masks = true, .masks = {(a), (b), (c)}})

typedef struct CwCmd {
  m4 xf;
  u32 geo;
  u16 key;
  u16 paint;
  f32 paint_p;
  v3 masks;
  b32 has_masks;
} CwCmd;

typedef enum CwSpecKind {
  CWS_CLOTH = 1,
  CWS_RUNOFF,
  CWS_PATCH,
  CWS_DRIFT,
  CWS_ROCK,
  CWS_SPALL,
  CWS_PANEL,
  CWS_CATENARY,
  CWS_TERRAIN,
  CWS_ROAD,
  CWS_CUSHION,
} CwSpecKind;

typedef struct CwSpec {
  u32 kind;
  union {
    CwClothSpec cloth;
    struct {
      f32 width, len, amount;
      u32 cols, rows;
      f32 wander, seed;
    } runoff;
    struct {
      const f32 *radii;
      u32 lobes;
      f32 sag;
    } patch;
    struct {
      f32 len, w, h;
      u32 nz;
      f32 seed;
    } drift;
    struct {
      f32 size;
      u32 detail;
      f32 squash, seed;
    } rock;
    struct {
      f32 w, h, depth;
    } spall;
    CwPanelSpec panel;
    struct {
      v3 from, to;
      f32 sag, radius;
      u32 seg, radial;
      f32 jitter;
    } cat;
    struct {
      f32 sx, sy, sz, bevel;
      v3 fill;
    } cushion;
  };
} CwSpec;

// a geometry slot: built at record time (ready) or expanded in parallel from its spec
typedef struct CwSlot {
  CwGeo g;
  u32 spec; // index into specs, or ~0u when ready
} CwSlot;

typedef struct CwInst {
  u16 proto;
  b32 has_masks;
  v3 masks;
  v3 pos; // level space
  f32 ry, rx, rz;
  v3 scale;
} CwInst;

typedef struct CwBoxProxy {
  u32 surface;
  m4 xf; // level space; a unit box scaled and placed
} CwBoxProxy;

typedef struct CwCacheEntry {
  u32 kind;
  i32 qa, qb;
  u32 geo;
} CwCacheEntry;

typedef struct CwJitter {
  CodRng rng;
  f32 yaw, scale;
} CwJitter;

DYN_ARRAY_TYPEDEF(CwCmd)
DYN_ARRAY_TYPEDEF(CwSlot)
DYN_ARRAY_TYPEDEF(CwSpec)
DYN_ARRAY_TYPEDEF(CwInst)
DYN_ARRAY_TYPEDEF(CwBoxProxy)
DYN_ARRAY_TYPEDEF(CwCacheEntry)

typedef u32 CwGeoId;

typedef struct CwA {
  Allocator al;
  DynArray(CwCmd) cmds;
  DynArray(CwCmd) ccmds; // real triangles as collision, keyed by the physics surface
  DynArray(CwSlot) geos;
  DynArray(CwSpec) specs;
  DynArray(CwInst) insts;
  DynArray(CwBoxProxy) boxes;
  DynArray(CwCacheEntry) cache;
  DynArray(v3) interior_lights;
  DynArray(v3) lamp_anchors;
  CwProto protos[CWP_COUNT];
  CwGeoId g_box, g_box_fine, g_box_soft, g_box_thin, g_pane;
  m4 xform;
  b32 skirts;
  b32 jitter_on;
  CwJitter jitter;
} CwA;

hz_internal CwGeoId cwa_ready(CwA *A, CwGeo g) {
  CwGeoId id = A->geos.len;
  dyn_arr_append(A->geos, ((CwSlot){.g = g, .spec = ~0u}));
  return id;
}

hz_internal CwGeoId cwa_spec(CwA *A, CwSpec s) {
  u32 si = A->specs.len;
  dyn_arr_append(A->specs, s);
  CwGeoId id = A->geos.len;
  dyn_arr_append(A->geos, ((CwSlot){.spec = si}));
  return id;
}

// A.add / A.addOnce: a transformed geometry merged into the static batch for `key`
hz_internal void cwa_add(CwA *A, u32 key, CwGeoId geo, m4 m, const CwOpts *o) {
  CwCmd c = {.xf = m, .geo = geo, .key = (u16)key};
  if (o) {
    c.has_masks = o->has_masks;
    c.masks = o->masks;
    c.paint = (u16)o->paint;
    c.paint_p = o->paint_p;
  }
  dyn_arr_append(A->cmds, c);
}

// A.collideGeo: a geometry's triangles become collision of `surface`
hz_internal void cwa_collide_geo(CwA *A, u32 surface, CwGeoId geo, m4 m) {
  dyn_arr_append(A->ccmds, ((CwCmd){.xf = m, .geo = geo, .key = (u16)surface}));
}

// A.cache: kit pieces that repeat share one geometry, keyed by kind and two 2-decimal quantities
hz_internal i32 cw_q2(f32 v) { return (i32)m_roundf(v * 100.0f); }

hz_internal b32 cwa_cache_find(CwA *A, u32 kind, i32 qa, i32 qb, CwGeoId *out) {
  for (u32 i = 0; i < A->cache.len; i++) {
    const CwCacheEntry *e = &A->cache.items[i];
    if (e->kind == kind && e->qa == qa && e->qb == qb) {
      *out = e->geo;
      return true;
    }
  }
  return false;
}

hz_internal CwGeoId cwa_cache_store(CwA *A, u32 kind, i32 qa, i32 qb, CwGeoId geo) {
  dyn_arr_append(A->cache, ((CwCacheEntry){.kind = kind, .qa = qa, .qb = qb, .geo = geo}));
  return geo;
}

// ---- collision proxies ----

// A.box: an axis-aligned or Y-rotated box proxy
hz_internal void cwa_box(CwA *A, u32 surface, f32 cx, f32 cy, f32 cz, f32 sx, f32 sy, f32 sz, f32 ry) {
  dyn_arr_append(A->boxes, ((CwBoxProxy){.surface = surface, .xf = cw_trs(cx, cy, cz, ry, sx, sy, sz, 0.0f, 0.0f)}));
}

// A.slabBox: a wall slab given in panel space, placed by the panel's matrix
hz_internal void cwa_slab_box(CwA *A, u32 surface, m4 pm, f32 x, f32 y, f32 w, f32 h, f32 t) {
  dyn_arr_append(A->boxes, ((CwBoxProxy){.surface = surface, .xf = m4_mul(pm, cw_trs(x, y, t * 0.5f, 0.0f, w, h, t, 0.0f, 0.0f))}));
}

// a panel-space point to level space
hz_internal v3 cw_world_of(m4 pm, f32 x, f32 y, f32 z) { return m4_mulv3(pm, (v3){x, y, z}, 1.0f); }

// the Y rotation baked into a panel matrix
hz_internal f32 cw_ry_of(m4 pm) { return m_atan2f(pm.m[2][0], pm.m[2][2]); }

// ---- prop prototypes ----

// appends a level decimated from the last one within max_error metres; kept only when it drops a quarter of the triangles
hz_internal void cwa_proto_add_lod(CwA *A, CwProto *p, f32 max_error) {
  if (max_error <= 0.0f || p->lod_count >= CW_PROTO_MAX_LODS)
    return;
  const CwGeo *prev = p->lod_count == 1 ? &p->geo : &p->lods[p->lod_count - 2];
  CwGeo g = cw_geo_clone(&A->al, prev);
  CwSimplifyStats st = cw_geo_simplify(&g, max_error);
  if (st.tris_after * 4u > st.tris_before * 3u)
    return;
  p->lods[p->lod_count - 1] = g;
  p->lod_simp[p->lod_count - 1] = st;
  p->lod_err[p->lod_count] = p->lod_err[p->lod_count - 1] + st.error;
  p->lod_count++;
}

hz_internal void cwa_proto(CwA *A, u32 id, CwGeo geo, u32 key, CwProtoOpts o) {
  CwProto *p = &A->protos[id];
  assert_msg(!p->defined, "cw proto registered twice");
  *p = (CwProto){.defined = true, .geo = geo, .key = key, .tilt = o.tilt, .sink = o.sink, .skirt = o.skirt, .max_dist = o.max_dist,
                 .cast_shadow = !o.no_shadow, .receive_shadow = !o.no_receive};
  if (o.simplify > 0.0f) {
    p->simplify = o.simplify;
    p->simp = cw_geo_simplify(&p->geo, o.simplify);
  }
  p->lod_count = 1;
  p->lod_err[0] = p->simp.error;
  cwa_proto_add_lod(A, p, o.lod1);
  cwa_proto_add_lod(A, p, o.lod2);
}

hz_internal void cwa_place(CwA *A, u32 id, v3 pos, f32 ry, f32 rx, f32 rz, v3 scale, const v3 *masks) {
  CwInst in = {.proto = (u16)id, .pos = pos, .ry = ry, .rx = rx, .rz = rz, .scale = scale};
  if (masks) {
    in.has_masks = true;
    in.masks = *masks;
  }
  dyn_arr_append(A->insts, in);
  A->protos[id].count++;
}

// A.put: loose placement with jitter and the contact skirt
hz_internal void cwa_put(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 s, const v3 *masks, f32 rx, f32 rz) {
  CwProto *p = &A->protos[id];
  if (A->jitter_on && p->defined && p->tilt > 0.0f) {
    CodRng *r = &A->jitter.rng;
    ry += cod_rng_range(r, -A->jitter.yaw, A->jitter.yaw);
    rx += cod_rng_range(r, -p->tilt, p->tilt);
    rz += cod_rng_range(r, -p->tilt, p->tilt);
    s *= 1.0f + cod_rng_range(r, -A->jitter.scale, A->jitter.scale);
    y -= p->sink;
  }
  cwa_place(A, id, (v3){x, y, z}, ry, rx, rz, (v3){s, s, s}, masks);
  if (A->skirts && p->defined && p->skirt > 0.0f && A->protos[CWP_DUST_SKIRT].defined) {
    f32 rr = p->skirt * s;
    cwa_place(A, CWP_DUST_SKIRT, (v3){x, y + 0.004f, z}, m_fmodf(x * 2.7f + z * 1.9f, 6.283f), 0.0f, 0.0f, (v3){rr, 1.0f, rr}, NULL);
  }
}

hz_internal void cwa_put_s(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 sx, f32 sy, f32 sz, const v3 *masks, f32 rx, f32 rz) {
  cwa_place(A, id, (v3){x, y, z}, ry, rx, rz, (v3){sx, sy, sz}, masks);
}

// a literal mask triple for put
#define CW_V3(a, b, c) (&(v3){(a), (b), (c)})

// ---- assembler lifetime ----

// new assembler on `al`; the shared unit pieces (BOX, BOX_FINE, BOX_SOFT, BOX_THIN, PANE) are built up front
hz_internal CwA *cwa_begin(Allocator al) {
  CwA *A = ALLOC(&al, CwA);
  A->al = al;
  A->cmds = dyn_arr_new_alloc(&A->al, CwCmd, 1u << 17);
  A->ccmds = dyn_arr_new_alloc(&A->al, CwCmd, 16);
  A->geos = dyn_arr_new_alloc(&A->al, CwSlot, 1u << 15);
  A->specs = dyn_arr_new_alloc(&A->al, CwSpec, 1u << 13);
  A->insts = dyn_arr_new_alloc(&A->al, CwInst, 1u << 14);
  A->boxes = dyn_arr_new_alloc(&A->al, CwBoxProxy, 1u << 13);
  A->cache = dyn_arr_new_alloc(&A->al, CwCacheEntry, 512);
  A->interior_lights = dyn_arr_new_alloc(&A->al, v3, 64);
  A->lamp_anchors = dyn_arr_new_alloc(&A->al, v3, 16);
  A->xform = cw_level_xform();
  A->skirts = true;
  A->g_box = cwa_ready(A, cw_chamfer_box(&A->al, 1, 1, 1, 0.012f));
  A->g_box_fine = cwa_ready(A, cw_chamfer_box(&A->al, 1, 1, 1, 0.004f));
  A->g_box_soft = cwa_ready(A, cw_chamfer_box(&A->al, 1, 1, 1, 0.03f));
  A->g_box_thin = cwa_ready(A, cw_plain_box(&A->al));
  A->g_pane = cwa_ready(A, cw_quad(&A->al, 1, 1));
  return A;
}

#define BOX(A) ((A)->g_box)
#define BOX_FINE(A) ((A)->g_box_fine)
#define BOX_SOFT(A) ((A)->g_box_soft)
#define BOX_THIN(A) ((A)->g_box_thin)
#define PANE(A) ((A)->g_pane)

// the contact shape of every deferred geometry kind, recorded with the rng draws the generator makes

hz_internal CwGeoId cwa_cloth(CwA *A, CwClothSpec s) {
  CwSpec sp = {.kind = CWS_CLOTH};
  sp.cloth = s;
  return cwa_spec(A, sp);
}

// the cloth geometry's seed: the caller's rng draws it when no seed is given
hz_internal f32 cw_cloth_seed(CodRng *rng) { return rng ? (f32)(cod_rng_f64(rng) * 30.0) : 0.0f; }

hz_internal CwGeoId cwa_runoff(CwA *A, CodRng *rng, f32 width, f32 len, f32 amount, u32 cols) {
  CwSpec sp = {.kind = CWS_RUNOFF};
  sp.runoff.width = width;
  sp.runoff.len = len;
  sp.runoff.amount = amount;
  sp.runoff.cols = cols;
  sp.runoff.rows = 7;
  sp.runoff.wander = 0.35f;
  sp.runoff.seed = rng ? (f32)(cod_rng_f64(rng) * 40.0) : 0.0f;
  return cwa_spec(A, sp);
}

// the lobe radii are the rng draws
hz_internal CwGeoId cwa_patch(CwA *A, CodRng *rng, f32 radius, u32 lobes, f32 wobble) {
  f32 *radii = ALLOC_ARRAY_NO_ZERO(&A->al, f32, lobes);
  for (u32 i = 0; i < lobes; i++)
    radii[i] = radius * (1.0f - wobble + cod_rng_float(rng) * wobble * 2.0f);
  CwSpec sp = {.kind = CWS_PATCH};
  sp.patch.radii = radii;
  sp.patch.lobes = lobes;
  return cwa_spec(A, sp);
}

hz_internal CwGeoId cwa_drift(CwA *A, CodRng *rng, f32 len, f32 w, f32 h, u32 nz) {
  CwSpec sp = {.kind = CWS_DRIFT};
  sp.drift.len = len;
  sp.drift.w = w;
  sp.drift.h = h;
  sp.drift.nz = nz;
  sp.drift.seed = (f32)(cod_rng_f64(rng) * 30.0);
  return cwa_spec(A, sp);
}

hz_internal CwGeoId cwa_rock(CwA *A, CodRng *rng, f32 size, u32 detail, f32 squash) {
  CwSpec sp = {.kind = CWS_ROCK};
  sp.rock.size = size;
  sp.rock.detail = detail;
  sp.rock.squash = squash;
  sp.rock.seed = (f32)(cod_rng_f64(rng) * 40.0);
  return cwa_spec(A, sp);
}

hz_internal CwGeoId cwa_spall(CwA *A, f32 w, f32 h, f32 depth) {
  CwSpec sp = {.kind = CWS_SPALL};
  sp.spall.w = w;
  sp.spall.h = h;
  sp.spall.depth = depth;
  return cwa_spec(A, sp);
}

// jitter is a pure function of the segment index, no rng
hz_internal CwGeoId cwa_catenary(CwA *A, v3 from, v3 to, f32 sag, f32 radius, u32 seg, u32 radial, f32 jitter) {
  CwSpec sp = {.kind = CWS_CATENARY};
  sp.cat.from = from;
  sp.cat.to = to;
  sp.cat.sag = sag;
  sp.cat.radius = radius;
  sp.cat.seg = seg;
  sp.cat.radial = radial;
  sp.cat.jitter = jitter;
  return cwa_spec(A, sp);
}

hz_internal CwGeoId cwa_cushion(CwA *A, f32 sx, f32 sy, f32 sz, f32 bevel, v3 fill) {
  CwSpec sp = {.kind = CWS_CUSHION};
  sp.cushion.sx = sx;
  sp.cushion.sy = sy;
  sp.cushion.sz = sz;
  sp.cushion.bevel = bevel;
  sp.cushion.fill = fill;
  return cwa_spec(A, sp);
}

// expands one deferred geometry on `al`; pure of its spec, so any lane can run it
hz_internal CwGeo cw_expand_spec(const CwSpec *s, Allocator *al);

// a pick over a file-scope id list
#define CW_PICK(rng, list) ((list)[cod_rng_pick((rng), (u32)(sizeof(list) / sizeof((list)[0])))])
#define CW_TAU32 6.28f

// put with the common [1, g, 1] mask scale; every argument is already drawn
hz_internal void cwa_put_g(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 s, f32 g) {
  v3 m = {1.0f, g, 1.0f};
  cwa_put(A, id, x, y, z, ry, s, &m, 0.0f, 0.0f);
}

hz_internal void cwa_put_gr(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 s, f32 g, f32 rx, f32 rz) {
  v3 m = {1.0f, g, 1.0f};
  cwa_put(A, id, x, y, z, ry, s, &m, rx, rz);
}

hz_internal void cwa_put_m(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 s, f32 m0, f32 m1, f32 m2, f32 rx, f32 rz) {
  v3 m = {m0, m1, m2};
  cwa_put(A, id, x, y, z, ry, s, &m, rx, rz);
}

hz_internal void cwa_put_s_m(CwA *A, u32 id, f32 x, f32 y, f32 z, f32 ry, f32 sx, f32 sy, f32 sz, f32 m0, f32 m1, f32 m2, f32 rx, f32 rz) {
  v3 m = {m0, m1, m2};
  cwa_put_s(A, id, x, y, z, ry, sx, sy, sz, &m, rx, rz);
}

// the loose-placement jitter stream armed for the set-dressing passes
hz_internal void cwa_jitter_on(CwA *A) {
  A->jitter_on = true;
  A->jitter.rng = cod_rng_make(0x9e3779b1u);
  A->jitter.yaw = 0.209f;
  A->jitter.scale = 0.08f;
}

hz_internal void cwa_jitter_off(CwA *A) { A->jitter_on = false; }

hz_internal b32 cwa_has(const CwA *A, u32 id) { return A->protos[id].defined; }
