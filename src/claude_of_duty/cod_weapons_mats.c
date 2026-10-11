// Claude of Duty weapons: the weapon surface set over the COD material library.

// one weapon surface: the library entry plus the viewmodel overrides; the fields past `normal_strength` go to the material when CodMaterialDesc carries them
typedef struct CwpMatDef {
  u32 lib; // CodMaterialId
  f32 scale;
  v3 tint; // linear multiplier
  u32 bake_size;
  u32 bake_seed;
  f32 bake_relief;
  u32 tint_a; // sRGB bake tints of the fabrics
  u32 tint_b;
  v4 roughness;
  f32 normal_strength;
  v4 detail;
  v4 weather;
  v4 macro;
  v4 wear;
  v4 wear_material;
  u32 wear_color;  // sRGB
  u32 grime_color; // sRGB, 0 = the library's
  v4 cloth;
  f32 specular; // specular intensity
} CwpMatDef;

hz_internal const CwpMatDef cwp_mat_defs[CWP_MAT_SLEEVE + 1] = {
    [CWP_MAT_ALU] = {.lib = COD_MAT_RUBBER, .scale = 0.095f, .tint = {0.285f, 0.302f, 0.349f}, .bake_size = 1024, .bake_seed = 601, .bake_relief = 0.005f,
        .roughness = {0.66f, 0.09f, 0.24f, 0.0f}, .normal_strength = 1.5f, .detail = {22.0f, 1.2f, 0.72f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.2f, 0.6f, 0.5f, 0.0f}, .wear_material = {0.54f, 0.8f, 0.0f, 0.8f}, .wear_color = 0x34383d, .grime_color = 0x0b0a08, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.11f},
    [CWP_MAT_ALU_FINE] = {.lib = COD_MAT_RUBBER, .scale = 0.038f, .tint = {0.135f, 0.144f, 0.165f}, .bake_size = 1024, .bake_seed = 733, .bake_relief = 0.0025f,
        .roughness = {0.56f, 0.07f, 0.26f, 0.0f}, .normal_strength = 1.15f, .detail = {30.0f, 0.85f, 0.6f, 4.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.18f, 0.5f, 0.5f, 0.0f}, .wear_material = {0.5f, 0.8f, 0.0f, 0.75f}, .wear_color = 0x40444a, .grime_color = 0x0b0a08, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.08f},
    [CWP_MAT_STEEL] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.12f, .tint = {0.17f, 0.162f, 0.152f}, .bake_size = 512, .bake_seed = 617, .bake_relief = 0.006f,
        .roughness = {0.66f, 0.24f, 0.42f, 0.0f}, .normal_strength = 1.2f, .detail = {13.0f, 0.95f, 0.42f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.16f, 0.55f, 0.5f, 0.0f}, .wear_material = {0.26f, 1.0f, 0.0f, 0.7f}, .wear_color = 0x62666b, .grime_color = 0x0c0a07, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.5f},
    [CWP_MAT_STEEL_SOOT] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.1f, .tint = {0.022f, 0.02f, 0.018f}, .bake_size = 512, .bake_seed = 617, .bake_relief = 0.006f,
        .roughness = {0.42f, 0.5f, 0.8f, 0.0f}, .normal_strength = 1.3f, .detail = {15.0f, 1.0f, 0.5f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.75f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.06f, 0.7f, 0.55f, 0.0f}, .wear_material = {0.55f, 1.0f, 0.0f, 0.6f}, .wear_color = 0x3a3c3e, .grime_color = 0x070604, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.1f},
    [CWP_MAT_STEEL_BRIGHT] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.05f, .tint = {0.155f, 0.155f, 0.164f},
        .roughness = {0.5f, 0.44f, 0.58f, 0.0f}, .normal_strength = 1.0f, .detail = {12.0f, 0.8f, 0.3f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.16f, 0.45f, 0.4f, 0.0f}, .wear_material = {0.18f, 1.0f, 0.0f, 0.6f}, .wear_color = 0x5c6066, .grime_color = 0x0a0806, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.5f},
    [CWP_MAT_STEEL_BLACK] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.07f, .tint = {0.155f, 0.158f, 0.165f}, .bake_size = 512, .bake_seed = 829, .bake_relief = 0.004f,
        .roughness = {0.56f, 0.14f, 0.36f, 0.0f}, .normal_strength = 0.95f, .detail = {18.0f, 0.7f, 0.3f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.24f, 0.5f, 0.5f, 0.0f}, .wear_material = {0.22f, 1.0f, 0.0f, 0.75f}, .wear_color = 0x6a6f75, .grime_color = 0x0a0806, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.5f},
    [CWP_MAT_POLYMER] = {.lib = COD_MAT_RUBBER, .scale = 0.055f, .tint = {0.224f, 0.211f, 0.192f}, .bake_size = 1024, .bake_seed = 149, .bake_relief = 0.009f,
        .roughness = {0.63f, 0.15f, 0.3f, 0.0f}, .normal_strength = 1.5f, .detail = {26.0f, 1.15f, 0.55f, 6.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.26f, 0.6f, 0.5f, 0.0f}, .wear_material = {0.46f, 0.0f, 0.0f, 0.5f}, .wear_color = 0x3e4145, .grime_color = 0x0b0a08, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.13f},
    [CWP_MAT_POLYMER_TAN] = {.lib = COD_MAT_RUBBER, .scale = 0.08f, .tint = {0.62f, 0.498f, 0.358f}, .bake_seed = 131,
        .roughness = {0.63f, 0.16f, 0.3f, 0.0f}, .normal_strength = 1.2f, .detail = {24.0f, 1.0f, 0.5f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.24f, 0.7f, 0.5f, 0.0f}, .wear_material = {0.44f, 0.0f, 0.0f, 0.5f}, .wear_color = 0x5c5340, .grime_color = 0x0f0c08, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.14f},
    [CWP_MAT_RUBBER] = {.lib = COD_MAT_RUBBER, .scale = 0.055f, .tint = {0.147f, 0.137f, 0.127f}, .bake_seed = 211,
        .roughness = {0.86f, 0.04f, 0.55f, 0.0f}, .normal_strength = 1.35f, .detail = {14.0f, 1.0f, 0.55f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.55f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.22f, 0.8f, 0.6f, 0.0f}, .wear_material = {0.72f, 0.0f, 0.0f, 0.35f}, .wear_color = 0x24262a, .grime_color = 0x0a0908, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.12f},
    [CWP_MAT_BRASS] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.05f, .tint = {2.3f, 1.58f, 0.74f},
        .roughness = {0.55f, 0.16f, 0.36f, 0.0f}, .normal_strength = 0.75f, .detail = {10.0f, 0.55f, 0.28f, 4.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.8f, 0.3f, 0.3f, 0.0f}, .wear_material = {0.12f, 1.0f, 0.0f, 0.8f}, .wear_color = 0xe8c98a, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.5f},
    [CWP_MAT_COPPER] = {.lib = COD_MAT_METAL_BRUSHED, .scale = 0.04f, .tint = {2.25f, 1.4f, 1.09f},
        .roughness = {0.6f, 0.18f, 0.34f, 0.0f}, .normal_strength = 0.75f, .detail = {10.0f, 0.55f, 0.28f, 4.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.5f, 0.3f, 0.3f, 0.0f}, .wear_material = {0.2f, 1.0f, 0.0f, 0.8f}, .wear_color = 0xd9a271, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.5f},
    [CWP_MAT_GLOVE] = {.lib = COD_MAT_FABRIC, .scale = 0.032f, .tint = {0.19f, 0.155f, 0.127f}, .bake_size = 512, .bake_seed = 401, .tint_a = 0x453a30, .tint_b = 0x2a2320,
        .roughness = {0.92f, 0.06f, 0.78f, 0.0f}, .normal_strength = 1.35f, .detail = {12.0f, 0.85f, 0.62f, 6.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.34f, 0.85f, 0.75f, 0.0f}, .wear_material = {0.72f, 0.0f, 0.0f, 0.4f}, .wear_color = 0x2a2118, .grime_color = 0x080604, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.16f},
    [CWP_MAT_GLOVE_PAD] = {.lib = COD_MAT_RUBBER, .scale = 0.024f, .tint = {0.118f, 0.095f, 0.08f}, .bake_seed = 307,
        .roughness = {1.0f, 0.0f, 0.78f, 0.0f}, .normal_strength = 0.7f, .detail = {9.0f, 0.95f, 0.6f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.4f, 0.75f, 0.65f, 0.0f}, .wear_material = {0.78f, 0.0f, 0.0f, 0.35f}, .wear_color = 0x241c14, .grime_color = 0x070504, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.15f},
    [CWP_MAT_GLOVE_SEAM] = {.lib = COD_MAT_FABRIC, .scale = 0.02f, .tint = {0.35f, 0.286f, 0.234f}, .bake_size = 512, .bake_seed = 401, .tint_a = 0x453a30, .tint_b = 0x2a2320,
        .roughness = {0.9f, 0.06f, 0.74f, 0.0f}, .normal_strength = 1.0f, .detail = {24.0f, 0.6f, 0.45f, 5.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.5f, 0.8f, 0.7f, 0.0f}, .wear_material = {0.7f, 0.0f, 0.0f, 0.4f}, .wear_color = 0x3a2d20, .grime_color = 0x0a0806, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.16f},
    [CWP_MAT_SLEEVE] = {.lib = COD_MAT_FABRIC, .scale = 0.05f, .tint = {0.16f, 0.152f, 0.138f}, .bake_size = 512, .bake_seed = 503, .tint_a = 0x6e6047, .tint_b = 0x4c4231,
        .roughness = {0.95f, 0.05f, 0.8f, 0.0f}, .normal_strength = 1.45f, .detail = {9.0f, 0.95f, 0.7f, 6.0f}, .weather = {0.0f, 0.0f, 0.0f, 0.62f}, .macro = {0.55f, 0.05f, 0.07f, 0.06f},
        .wear = {0.5f, 0.9f, 0.75f, 0.0f}, .wear_material = {0.9f, 0.0f, 0.0f, 0.45f}, .wear_color = 0x4a4034, .grime_color = 0x0c0a06, .cloth = {0.0f, 1.0f, 0.0f, 0.0f}, .specular = 0.14f},
};

// the COD material request of a weapon surface; the fields CodMaterialDesc cannot carry yet are listed in the report
hz_internal CodMaterialDesc cwp_material_desc(u32 key) {
  CodMaterialDesc d = cod_material_desc(COD_MAT_RUBBER);
  if (key == CWP_MAT_CAVITY || key == CWP_MAT_OPTIC_TUBE) {
    // truly black and matte (cavity 0x0a0c0e, optic tube 0x1d2023)
    d.tint = key == CWP_MAT_CAVITY ? (ColorF32){0.003f, 0.0037f, 0.0044f, 1.0f} : (ColorF32){0.0123f, 0.0144f, 0.0168f, 1.0f};
    d.uv_mode = COD_UV_TRIPLANAR;
    d.local_space = true;
    d.scale = 0.1f;
    d.double_sided = true;
    d.parallax = 0.0f;
    return d;
  }
  const CwpMatDef *m = &cwp_mat_defs[key];
  d = cod_material_desc(m->lib);
  d.uv_mode = COD_UV_TRIPLANAR;
  d.local_space = true;
  d.vertex_masks = true;
  d.parallax = 0.0f;
  d.scale = m->scale;
  d.viewmodel = true;
  d.tint = (ColorF32){m->tint.x, m->tint.y, m->tint.z, 1.0f};
  if (m->tint_a) {
    d.bake_variant = 1;
    d.bake_color = m->tint_a;
  }
  d.bake_size = m->bake_size;
  d.bake_seed = m->bake_seed;
  d.bake_relief = m->bake_relief;
  d.roughness = (v4){m->roughness.x, m->roughness.y, m->roughness.z, 0.0f};
  d.normal_strength = m->normal_strength;
  d.ao_strength = 1.0f;
  d.detail = m->detail;
  d.weather_v = m->weather;
  d.macro = m->macro;
  d.wear = m->wear;
  d.wear_material = m->wear_material;
  d.override_mask = COD_OVR_DETAIL | COD_OVR_MACRO | COD_OVR_WEATHER | COD_OVR_WEAR | COD_OVR_WEAR_MATERIAL | COD_OVR_ROUGHNESS | COD_OVR_AO |
                    COD_OVR_NORMAL;
  return d;
}
