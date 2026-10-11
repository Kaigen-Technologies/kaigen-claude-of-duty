// Claude of Duty materials: the surface library and the opts merge.

// the bake programs, one per distinct surface generator plus the two shared maps
typedef enum CodBakeProgram {
  COD_PROG_CONCRETE = 0,
  COD_PROG_BRICK,
  COD_PROG_PLASTER,
  COD_PROG_TILE,
  COD_PROG_ASPHALT,
  COD_PROG_SAND,
  COD_PROG_DIRT,
  COD_PROG_GRAVEL,
  COD_PROG_METAL_RUST,
  COD_PROG_METAL_PAINTED,
  COD_PROG_METAL_BRUSHED,
  COD_PROG_CORRUGATED,
  COD_PROG_WOOD,
  COD_PROG_FABRIC,
  COD_PROG_BURLAP,
  COD_PROG_FOLIAGE,
  COD_PROG_RUBBER,
  COD_PROG_GLASS,
  COD_PROG_DETAIL,
  COD_PROG_MACRO,
  COD_PROG_COUNT,
} CodBakeProgram;

// how a texture set is generated (library `bake`); tints are sRGB hex, 0 = none
typedef struct CodBakeDef {
  u32 program;
  u32 size;
  f32 world_size;
  f32 relief;
  f32 seed;
  v4 param;
  u32 tint_a;
  u32 tint_b;
} CodBakeDef;

// the resolved material parameters (library `mat` over the defaults, then the request); only 4-byte members, compared bytewise
typedef struct CodMatParams {
  u32 uv_mode;
  u32 local_space;
  u32 alpha_mask;
  u32 vertex_masks;
  u32 double_sided;
  u32 transparent;
  u32 viewmodel;
  f32 scale;
  f32 parallax;
  f32 parallax_fade_start;
  f32 parallax_fade_end;
  f32 parallax_layers;
  f32 detile;
  f32 detail_world;
  f32 macro_relief;
  f32 normal_strength;
  f32 ao_strength;
  f32 opacity;
  f32 env_intensity;
  v4 emissive; // linear radiance, cd/m2
  v4 detail;
  v4 macro;
  v4 macro_big;
  v4 patch;
  v4 cloth;
  v4 weather;
  v4 wear;
  v4 wear_material;
  v4 roughness; // x scale, y offset, z minimum
  v4 wear_color;
  v4 dust_color;
  v4 grime_color;
  v4 rust_color;
  v4 tint;
} CodMatParams;

// a library entry
typedef struct CodSurfaceDef {
  const char *name;
  CodBakeDef bake;
  CodMatParams mat;
} CodSurfaceDef;

// sRGB 0xRRGGBB to a linear colour
hz_internal v4 cod_hex(u32 hex) {
  v3 s = {(f32)((hex >> 16) & 255u) / 255.0f, (f32)((hex >> 8) & 255u) / 255.0f, (f32)(hex & 255u) / 255.0f};
  v3 l = vec3_srgb_to_linear(s);
  return (v4){l.x, l.y, l.z, 1.0f};
}

// the default material parameters
hz_internal void cod_params_default(CodMatParams *p) {
  memzero_struct(p);
  p->uv_mode = COD_UV_PLANAR;
  p->scale = 2.0f;
  p->parallax = 0.0f;
  p->parallax_fade_start = 6.0f;
  p->parallax_fade_end = 14.0f;
  p->parallax_layers = 22.0f;
  p->detail = (v4){11.0f, 0.55f, 0.35f, 16.0f};
  p->detail_world = 0.26f;
  p->macro = (v4){0.045f, 0.35f, 0.1f, 0.35f};
  p->macro_big = (v4){1.0f, 0.0f, 0.03f, 0.0f};
  p->patch = (v4){0.0f, 2.6f, 0.12f, -0.08f};
  p->cloth = (v4){0.0f, 1.0f, 0.0f, 0.0f};
  p->weather = (v4){0.35f, 0.3f, 0.55f, 0.4f};
  p->wear = (v4){0.5f, 0.7f, 0.5f, 0.0f};
  p->wear_material = (v4){0.42f, 0.0f, 0.0f, 0.5f};
  p->wear_color = cod_hex(0x8d8b86);
  p->dust_color = cod_hex(0x6b6154);
  p->grime_color = cod_hex(0x2a2620);
  p->rust_color = cod_hex(0x6d3a1c);
  p->tint = cod_hex(0xffffff);
  p->normal_strength = 1.0f;
  p->roughness = (v4){1.0f, 0.0f, 0.06f, 0.0f};
  p->ao_strength = 1.0f;
  p->opacity = 1.0f;
  p->env_intensity = 1.0f;
}

hz_internal void cod_bake_default(CodBakeDef *b, u32 program, f32 world_size, f32 relief, f32 seed, u32 size) {
  memzero_struct(b);
  b->program = program;
  b->size = size;
  b->world_size = world_size;
  b->relief = relief;
  b->seed = seed;
}

// the entry for a CodMaterialId
hz_internal void cod_library(u32 id, CodSurfaceDef *d) {
  memzero_struct(d);
  CodMatParams *m = &d->mat;
  cod_params_default(m);
  CodBakeDef *b = &d->bake;
  switch (id) {
  case COD_MAT_CONCRETE:
    d->name = "concrete";
    cod_bake_default(b, COD_PROG_CONCRETE, 2.5f, 0.09f, 11, 1024);
    b->param = (v4){1, 0, 0, 0};
    m->scale = 2.5f; m->parallax = 0.016f; m->detile = 0.4f;
    m->detail = (v4){9, 0.95f, 0.58f, 26};
    m->macro = (v4){0.085f, 0.62f, 0.24f, 0.45f};
    m->macro_big = (v4){2.05f, 0.130f, 0.028f, 0};
    m->patch = (v4){0.28f, 2.0f, 0.145f, -0.08f};
    m->weather = (v4){0.42f, 0.4f, 0.55f, 0.5f};
    m->wear_color = cod_hex(0x9a978f); m->dust_color = cod_hex(0x8b7f6a); m->grime_color = cod_hex(0x2b2823);
    m->roughness = (v4){0.98f, -0.01f, 0.24f, 0};
    break;
  case COD_MAT_CONCRETE_FLOOR:
    d->name = "concrete_floor";
    cod_bake_default(b, COD_PROG_CONCRETE, 2.5f, 0.075f, 47, 1024);
    b->param = (v4){0, 1, 0, 0};
    m->scale = 3.2f; m->parallax = 0.01f; m->detile = 0.0f;
    m->detail = (v4){9, 0.90f, 0.52f, 26};
    m->macro = (v4){0.075f, 0.48f, 0.18f, 0.3f};
    m->macro_relief = 0.3f;
    m->weather = (v4){0.55f, 0.1f, 0.15f, 0.5f};
    m->roughness = (v4){1.0f, 0.0f, 0.22f, 0};
    break;
  case COD_MAT_BRICK:
    d->name = "brick";
    cod_bake_default(b, COD_PROG_BRICK, 1.35f, 0.055f, 23, 1024);
    m->scale = 1.35f; m->parallax = 0.024f; m->parallax_layers = 24.0f;
    m->detail = (v4){7, 0.88f, 0.48f, 22};
    m->macro = (v4){0.09f, 0.58f, 0.22f, 0.55f};
    m->macro_big = (v4){1.95f, 0.115f, 0.03f, 0};
    m->weather = (v4){0.4f, 0.5f, 0.6f, 0.55f};
    m->wear_color = cod_hex(0xa08678); m->grime_color = cod_hex(0x241f19);
    m->roughness = (v4){0.98f, -0.01f, 0.26f, 0};
    break;
  case COD_MAT_PLASTER:
    d->name = "plaster";
    cod_bake_default(b, COD_PROG_PLASTER, 2.2f, 0.06f, 5, 1024);
    m->scale = 2.2f; m->parallax = 0.014f; m->detile = 0.8f;
    m->detail = (v4){10, 0.95f, 0.54f, 24};
    m->macro = (v4){0.085f, 0.72f, 0.26f, 0.5f};
    m->macro_big = (v4){2.15f, 0.150f, 0.026f, 0};
    m->patch = (v4){0.34f, 2.2f, 0.175f, -0.10f};
    m->weather = (v4){0.34f, 0.5f, 0.6f, 0.5f};
    m->wear_color = cod_hex(0xb0a692); m->dust_color = cod_hex(0x9c8a6c); m->grime_color = cod_hex(0x2a251d);
    m->roughness = (v4){0.97f, -0.02f, 0.26f, 0};
    break;
  case COD_MAT_TILE:
    d->name = "tile";
    cod_bake_default(b, COD_PROG_TILE, 1.5f, 0.03f, 31, 1024);
    m->scale = 1.5f; m->parallax = 0.03f; m->parallax_layers = 20.0f;
    m->detail = (v4){8, 0.6f, 0.36f, 18};
    m->macro = (v4){0.09f, 0.40f, 0.16f, 0.3f};
    m->macro_big = (v4){1.7f, 0.075f, 0.032f, 0};
    m->patch = (v4){0.14f, 1.7f, 0.10f, -0.05f};
    m->weather = (v4){0.3f, 0.2f, 0.3f, 0.5f};
    m->roughness = (v4){0.9f, -0.04f, 0.16f, 0};
    break;
  case COD_MAT_ASPHALT:
    d->name = "asphalt";
    cod_bake_default(b, COD_PROG_ASPHALT, 3.0f, 0.075f, 71, 1024);
    m->scale = 3.0f; m->parallax = 0.014f; m->detile = 1.0f;
    m->detail = (v4){8, 0.8f, 0.42f, 18};
    m->macro = (v4){0.062f, 0.52f, 0.22f, 0.25f};
    m->macro_relief = 0.55f;
    m->weather = (v4){0.45f, 0.05f, 0.1f, 0.26f};
    m->dust_color = cod_hex(0x8b8071); m->grime_color = cod_hex(0x232120);
    m->roughness = (v4){0.98f, -0.02f, 0.3f, 0};
    break;
  case COD_MAT_SAND:
    d->name = "sand";
    cod_bake_default(b, COD_PROG_SAND, 2.5f, 0.10f, 91, 1024);
    m->uv_mode = COD_UV_TRIPLANAR; m->scale = 2.5f; m->detile = 0.0f;
    m->detail = (v4){8, 0.7f, 0.30f, 18};
    m->macro = (v4){0.050f, 0.44f, 0.14f, 0.35f};
    m->macro_relief = 0.45f;
    m->weather = (v4){0.15f, 0.0f, 0.0f, 0.18f};
    m->dust_color = cod_hex(0xa89066); m->grime_color = cod_hex(0x4c4132);
    m->roughness = (v4){1.0f, 0.0f, 0.3f, 0};
    break;
  case COD_MAT_DIRT:
    d->name = "dirt";
    cod_bake_default(b, COD_PROG_DIRT, 2.5f, 0.12f, 13, 1024);
    m->uv_mode = COD_UV_TRIPLANAR; m->scale = 2.5f;
    m->detail = (v4){7, 0.85f, 0.36f, 18};
    m->macro = (v4){0.055f, 0.48f, 0.18f, 0.4f};
    m->macro_relief = 0.6f;
    m->weather = (v4){0.2f, 0.0f, 0.0f, 0.22f};
    m->dust_color = cod_hex(0x94805c); m->grime_color = cod_hex(0x37301f);
    m->roughness = (v4){0.98f, -0.02f, 0.3f, 0};
    break;
  case COD_MAT_GRAVEL:
    d->name = "gravel";
    cod_bake_default(b, COD_PROG_GRAVEL, 1.6f, 0.055f, 57, 1024);
    m->uv_mode = COD_UV_TRIPLANAR; m->scale = 1.6f;
    m->detail = (v4){6, 0.8f, 0.34f, 20};
    m->macro = (v4){0.070f, 0.44f, 0.2f, 0.3f};
    m->macro_relief = 0.7f;
    m->weather = (v4){0.2f, 0.0f, 0.0f, 0.16f};
    m->dust_color = cod_hex(0xa2947a); m->grime_color = cod_hex(0x4a4238);
    m->roughness = (v4){0.96f, -0.03f, 0.28f, 0};
    break;
  case COD_MAT_METAL_RUST:
    d->name = "metal_rust";
    cod_bake_default(b, COD_PROG_METAL_RUST, 1.2f, 0.035f, 37, 1024);
    m->scale = 1.2f; m->parallax = 0.004f;
    m->detail = (v4){9, 0.7f, 0.36f, 16};
    m->macro = (v4){0.10f, 0.30f, 0.14f, 0.4f};
    m->weather = (v4){0.25f, 0.4f, 0.5f, 0.35f};
    m->wear_color = cod_hex(0x8c8f93);
    m->wear_material = (v4){0.28f, 1.0f, 0, 0.85f};
    break;
  case COD_MAT_METAL_PAINTED:
    d->name = "metal_painted";
    cod_bake_default(b, COD_PROG_METAL_PAINTED, 1.5f, 0.018f, 61, 1024);
    b->tint_a = 0x4a5340; b->tint_b = 0x2a2f26;
    m->scale = 1.5f; m->parallax = 0.003f;
    m->detail = (v4){10, 0.6f, 0.32f, 16};
    m->macro = (v4){0.10f, 0.28f, 0.14f, 0.35f};
    m->weather = (v4){0.3f, 0.45f, 0.35f, 0.35f};
    m->wear_color = cod_hex(0x8f9296);
    m->wear_material = (v4){0.3f, 1.0f, 0, 0.9f};
    m->roughness = (v4){0.92f, -0.03f, 0.22f, 0};
    break;
  case COD_MAT_METAL_BRUSHED:
    d->name = "metal_brushed";
    cod_bake_default(b, COD_PROG_METAL_BRUSHED, 0.8f, 0.004f, 83, 512);
    m->scale = 0.8f;
    m->detail = (v4){8, 0.25f, 0.15f, 8};
    m->macro = (v4){0.09f, 0.14f, 0.1f, 0.2f};
    m->weather = (v4){0.15f, 0.15f, 0.2f, 0.2f};
    m->wear_color = cod_hex(0xb9bcc0);
    m->wear_material = (v4){0.16f, 1.0f, 0, 0.9f};
    break;
  case COD_MAT_CORRUGATED:
    d->name = "corrugated";
    cod_bake_default(b, COD_PROG_CORRUGATED, 2.4f, 0.075f, 29, 1024);
    m->scale = 2.4f; m->parallax = 0.03f; m->parallax_layers = 24.0f;
    m->detail = (v4){10, 0.6f, 0.32f, 18};
    m->macro = (v4){0.09f, 0.26f, 0.12f, 0.3f};
    m->weather = (v4){0.3f, 0.5f, 0.5f, 0.4f};
    m->wear_color = cod_hex(0x9aa0a4);
    m->wear_material = (v4){0.32f, 1.0f, 0, 0.85f};
    break;
  case COD_MAT_WOOD:
    d->name = "wood";
    cod_bake_default(b, COD_PROG_WOOD, 2.0f, 0.038f, 19, 1024);
    m->scale = 2.0f; m->parallax = 0.008f;
    m->detail = (v4){10, 0.8f, 0.42f, 18};
    m->macro = (v4){0.085f, 0.34f, 0.14f, 0.5f};
    m->weather = (v4){0.3f, 0.35f, 0.5f, 0.45f};
    m->wear_color = cod_hex(0xa88b62);
    m->wear_material = (v4){0.5f, 0.0f, 0, 0.7f};
    break;
  case COD_MAT_FABRIC:
    d->name = "fabric";
    cod_bake_default(b, COD_PROG_FABRIC, 0.7f, 0.008f, 43, 512);
    b->tint_a = 0x5a5445; b->tint_b = 0x3a3830;
    m->scale = 0.7f;
    m->detail = (v4){6, 0.42f, 0.28f, 10};
    m->macro = (v4){0.12f, 0.34f, 0.12f, 0.3f};
    m->macro_big = (v4){1.8f, 0.07f, 0.09f, 0};
    m->weather = (v4){0.25f, 0.2f, 0.3f, 0.35f};
    m->normal_strength = 1.15f;
    m->cloth = (v4){0.20f, 0.72f, 0.26f, 0};
    break;
  case COD_MAT_BURLAP:
    d->name = "burlap";
    cod_bake_default(b, COD_PROG_BURLAP, 0.5f, 0.018f, 67, 512);
    m->scale = 0.5f; m->parallax = 0.003f;
    m->detail = (v4){6, 0.4f, 0.28f, 9};
    m->macro = (v4){0.14f, 0.32f, 0.12f, 0.35f};
    m->macro_big = (v4){1.7f, 0.06f, 0.11f, 0};
    m->weather = (v4){0.4f, 0.15f, 0.35f, 0.4f};
    m->dust_color = cod_hex(0x9c8760);
    m->normal_strength = 1.15f;
    m->cloth = (v4){0.06f, 0.86f, 0.10f, 0};
    break;
  case COD_MAT_FOLIAGE:
    d->name = "foliage";
    cod_bake_default(b, COD_PROG_FOLIAGE, 0.6f, 0.02f, 79, 512);
    m->uv_mode = COD_UV_MESH; m->scale = 1.0f; m->alpha_mask = 1; m->double_sided = 1;
    m->detail = (v4){4, 0.25f, 0.15f, 8};
    m->macro = (v4){0.16f, 0.3f, 0.08f, 0.6f};
    m->weather = (v4){0.15f, 0.0f, 0.0f, 0.2f};
    break;
  case COD_MAT_RUBBER:
    d->name = "rubber";
    cod_bake_default(b, COD_PROG_RUBBER, 0.5f, 0.013f, 97, 512);
    m->scale = 0.45f;
    m->detail = (v4){7, 0.62f, 0.42f, 13};
    m->macro = (v4){0.16f, 0.36f, 0.20f, 0.18f};
    m->macro_big = (v4){1.8f, 0.10f, 0.11f, 0};
    m->weather = (v4){0.40f, 0.18f, 0.22f, 0.45f};
    m->dust_color = cod_hex(0x8d8478); m->grime_color = cod_hex(0x181715);
    m->tint = cod_hex(0xfffaf2);
    m->normal_strength = 1.25f;
    m->roughness = (v4){0.94f, -0.03f, 0.34f, 0};
    break;
  default:
    d->name = "glass";
    cod_bake_default(b, COD_PROG_GLASS, 2.0f, 0.0008f, 3, 512);
    m->scale = 2.0f; m->double_sided = 1; m->transparent = 1; m->opacity = 0.22f; m->env_intensity = 1.6f;
    m->detail = (v4){3, 0.06f, 0.05f, 6};
    m->macro = (v4){0.05f, 0.1f, 0.06f, 0.1f};
    m->weather = (v4){0.1f, 0.3f, 0.4f, 0.15f};
    m->normal_strength = 0.35f;
    m->roughness = (v4){0.9f, -0.01f, 0.03f, 0};
    break;
  }
}

// the texture size at a quality: a power of two scaled by the preset
hz_internal u32 cod_bake_size_for(u32 base, u32 quality) {
  f32 q = quality == COD_QUALITY_LOW ? 0.5f : quality == COD_QUALITY_MEDIUM ? 0.75f : 1.0f;
  f32 s = m_floorf(base * q / 128.0f + 0.5f) * 128.0f;
  if (s < 128.0f)
    s = 128.0f;
  u32 size = 128;
  while ((f32)size * 1.41421356f < s)
    size <<= 1;
  return size;
}

// the request merged over the library entry: the bake identity and the final material parameters
typedef struct CodResolved {
  u32 id;
  CodBakeDef bake;
  CodMatParams mat;
} CodResolved;

// emissive x emissive intensity in framebuffer units, as linear radiance
hz_internal v4 cod_emissive_radiance(u32 emissive, f32 intensity) {
  if (!emissive || intensity <= 0.0f)
    return (v4){0};
  v4 c = cod_hex(emissive);
  f32 k = intensity * COD_SCENE_UNIT;
  return (v4){c.x * k, c.y * k, c.z * k, 0.0f};
}

hz_internal void cod_resolve(const CodMaterialDesc *desc, u32 quality, CodResolved *out) {
  memzero_struct(out);
  u32 id = desc->id < COD_MAT_COUNT ? desc->id : COD_MAT_CONCRETE;
  CodSurfaceDef lib;
  cod_library(id, &lib);
  out->id = id;
  out->bake = lib.bake;
  CodMatParams *m = &out->mat;
  *m = lib.mat;

  u32 ovr = desc->override_mask;
  if (desc->uv_mode != COD_UV_LIBRARY)
    m->uv_mode = desc->uv_mode;
  m->local_space = desc->local_space;
  m->viewmodel = desc->viewmodel;
  m->vertex_masks = desc->vertex_masks;
  if (desc->scale > 0.0f)
    m->scale = desc->scale;
  if (desc->tint.a > 0.0f)
    m->tint = (v4){desc->tint.r, desc->tint.g, desc->tint.b, 1.0f};
  if (desc->parallax >= 0.0f)
    m->parallax = desc->parallax;
  if (desc->weather > 0.0f) {
    m->weather.x *= desc->weather;
    m->weather.y *= desc->weather;
  }
  if (desc->double_sided)
    m->double_sided = 1;
  if (desc->transparent)
    m->transparent = 1;
  if (ovr & COD_OVR_DETAIL) m->detail = desc->detail;
  if (ovr & COD_OVR_MACRO) m->macro = desc->macro;
  if (ovr & COD_OVR_WEATHER) m->weather = desc->weather_v;
  if (ovr & COD_OVR_WEAR) m->wear = desc->wear;
  if (ovr & COD_OVR_WEAR_MATERIAL) m->wear_material = desc->wear_material;
  if (ovr & COD_OVR_DETILE) m->detile = desc->detile;
  if (ovr & COD_OVR_ROUGHNESS) m->roughness = desc->roughness;
  if (ovr & COD_OVR_AO) m->ao_strength = desc->ao_strength;
  if (ovr & COD_OVR_NORMAL) m->normal_strength = desc->normal_strength;
  if (desc->opacity > 0.0f) m->opacity = desc->opacity;
  if (desc->env_intensity > 0.0f) m->env_intensity = desc->env_intensity;
  m->emissive = cod_emissive_radiance(desc->emissive, desc->emissive_intensity);
  // projection modes that cannot parallax or de-tile
  if (m->uv_mode == COD_UV_TRIPLANAR) {
    m->parallax = 0.0f;
    m->detile = 0.0f;
  }

  CodBakeDef *b = &out->bake;
  if (desc->bake_variant) {
    // the second paint tone is the first scaled to the library's 0.56 ratio
    b->tint_a = desc->bake_color;
    u32 r = (u32)((f32)((desc->bake_color >> 16) & 255u) * 0.56f);
    u32 g = (u32)((f32)((desc->bake_color >> 8) & 255u) * 0.56f);
    u32 bl = (u32)((f32)(desc->bake_color & 255u) * 0.56f);
    b->tint_b = (r << 16) | (g << 8) | bl;
  }
  if (desc->bake_size) b->size = desc->bake_size;
  if (desc->bake_seed) b->seed = (f32)desc->bake_seed;
  if (desc->bake_relief > 0.0f) b->relief = desc->bake_relief;
  b->size = cod_bake_size_for(b->size, quality);
}
