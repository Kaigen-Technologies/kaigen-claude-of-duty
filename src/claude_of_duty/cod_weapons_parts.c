// Claude of Duty weapons: firearm components from published dimensions.
#include "cod_weapons_geo.h"

#define CWG_TAU (2.0f * M_PI)
#define CWG_HALF_PI (0.5f * M_PI)

// ----------------------------------------------------------- small hardware ---

// cross pin with a domed head
void cwg_add_pin(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 r, f32 len) {
  CWG_SCOPE_BEGIN(c);
  cwg_add(c, a, cwg_rod_z(c, r, r, len, 12, 0.0004f), mat, (CwgT){.x = x, .y = y, .z = z, .ry = CWG_HALF_PI});
  PgMesh *dome = cwg_dome(c, r * 1.25f, 10, 0.5f);
  cwg_add(c, a, dome, mat, (CwgT){.x = x + len * 0.5f, .y = y, .z = z, .ry = -CWG_HALF_PI});
  cwg_add(c, a, dome, mat, (CwgT){.x = x - len * 0.5f, .y = y, .z = z, .ry = CWG_HALF_PI});
  CWG_SCOPE_END();
}

// hex-socket screw, head facing +axis
void cwg_add_screw(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 r_head, char axis, f32 len) {
  CWG_SCOPE_BEGIN(c);
  PgMesh *g = cwg_screw(c, r_head, r_head * 0.55f, r_head * 0.5f, len, 10);
  CwgT t = {.x = x, .y = y, .z = z};
  if (axis == 'y')
    t.rx = CWG_HALF_PI;
  else if (axis == 'x')
    t.ry = -CWG_HALF_PI;
  cwg_add(c, a, g, mat, t);
  CWG_SCOPE_END();
}

// QD sling swivel socket: a countersunk cup with a steel insert
void cwg_add_qd_socket(Cwg *c, CwpAsm *a, u32 mat_body, u32 mat_steel, f32 x, f32 y, f32 z, char axis, f32 r) {
  CWG_SCOPE_BEGIN(c);
  PgMesh *cup = cwg_lathe_z(c, 14, {0.0f, r * 0.55f}, {0.0f, r * 1.5f}, {0.0012f, r * 1.62f}, {0.006f, r * 1.62f}, {0.006f, r * 0.9f});
  PgMesh *inner = cwg_lathe_z(c, 12, {0.004f, 0.0f}, {0.004f, r * 0.55f}, {0.0f, r * 0.55f});
  CwgT t = {.x = x, .y = y, .z = z};
  if (axis == 'x')
    t.ry = CWG_HALF_PI;
  else if (axis == 'y')
    t.rx = -CWG_HALF_PI;
  cwg_add(c, a, cup, mat_body, t);
  cwg_add(c, a, inner, mat_steel, t);
  CWG_SCOPE_END();
}

// fixed sling loop: a flat steel eye
void cwg_add_sling_loop(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 radius, CwgT rot) {
  CWG_SCOPE_BEGIN(c);
  rot.x = x;
  rot.y = y;
  rot.z = z;
  cwg_add(c, a, cwg_ring(c, radius, 0.0016f, 14, 6), mat, rot);
  CWG_SCOPE_END();
}

// a live cartridge: brass case, shoulder, neck, copper FMJ tip (the caller's scope owns the meshes)
CwgCartridge cwg_cartridge(Cwg *c, f32 case_len, f32 rim_r, f32 bullet_len) {
  f32 neck_r = rim_r * 0.72f;
  PgMesh *brass = cwg_lathe_z(c, 16, {0.0f, 0.0f}, {0.0f, rim_r}, {0.0012f, rim_r * 0.97f}, {case_len * 0.62f, rim_r * 0.965f},
                              {case_len * 0.78f, neck_r}, {case_len, neck_r});
  PgMesh *bullet = cwg_lathe_z(c, 16, {case_len - 0.004f, neck_r * 0.98f}, {case_len + bullet_len * 0.45f, neck_r * 0.98f},
                               {case_len + bullet_len * 0.8f, neck_r * 0.62f}, {case_len + bullet_len, neck_r * 0.16f},
                               {case_len + bullet_len + 0.0004f, 0.0f});
  return (CwgCartridge){brass, bullet, case_len + bullet_len};
}

// a fired case: the same brass, no bullet, a slightly belled mouth
PgMesh *cwg_empty_case(Cwg *c, f32 case_len, f32 rim_r) {
  f32 neck_r = rim_r * 0.72f;
  return cwg_lathe_z(c, 16, {0.0f, 0.0f}, {0.0f, rim_r}, {0.0012f, rim_r * 0.97f}, {case_len * 0.62f, rim_r * 0.965f},
                     {case_len * 0.78f, neck_r}, {case_len, neck_r * 1.02f}, {case_len, neck_r * 0.86f}, {case_len * 0.8f, neck_r * 0.86f});
}

// ------------------------------------------------------------------- rails ---

// Picatinny along Z with its top face at `y`, plus the dark slot floor
void cwg_add_rail(Cwg *c, CwpAsm *a, u32 mat, f32 z0, f32 z1, f32 y, f32 x, const CwgRail *opts) {
  CWG_SCOPE_BEGIN(c);
  CwgRail none = {0};
  const CwgRail *o = opts ? opts : &none;
  f32 len = m_fabsf(z1 - z0);
  f32 base_h = o->pic.base_h > 0.0f ? o->pic.base_h : 0.0042f;
  f32 top_h = o->pic.top_h > 0.0f ? o->pic.top_h : 0.0032f;
  f32 waist = o->pic.waist > 0.0f ? o->pic.waist : 0.0157f;
  f32 cz = (z0 + z1) * 0.5f;
  f32 yb = y - base_h - top_h;
  cwg_add(c, a, cwg_picatinny(c, len, &o->pic), mat, (CwgT){.x = x, .y = yb, .z = cz});
  if (!o->no_slot_floor) {
    PgMesh *floor = cwg_box(c, waist * 0.99f, 0.0014f, len - 0.0004f, 0.0002f, 1);
    cwg_add(c, a, floor, CWP_MAT_CAVITY, (CwgT){.x = x, .y = yb + base_h - 0.0003f, .z = cz});
  }
  CWG_SCOPE_END();
}

// ------------------------------------------------------ barrel + muzzle devices ---

// stepped barrel with a chamber shoulder, a gas journal and a knurled section
CwgBarrel cwg_add_barrel(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_cavity, const CwgBarrelOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 y = o->y, z_breech = o->z_breech, z_muzzle = o->z_muzzle;
  f32 r_chamber = o->r_chamber > 0.0f ? o->r_chamber : 0.0112f;
  f32 r_bore = o->r_barrel > 0.0f ? o->r_barrel : 0.0072f;
  f32 r_gas = o->r_gas > 0.0f ? o->r_gas : 0.0092f;
  f32 len = z_breech - z_muzzle;
  f32 gas_at = o->gas_at != 0.0f ? o->gas_at : z_muzzle + len * 0.34f;
  PgMesh *g = cwg_lathe_z(c, o->seg ? o->seg : 22u, {0.0f, 0.0f}, {0.0f, r_chamber + 0.0018f}, {0.004f, r_chamber + 0.0022f},
                          {0.02f, r_chamber + 0.0022f}, {0.022f, r_chamber}, {len * 0.24f, r_chamber}, {len * 0.26f, r_bore + 0.0012f},
                          {z_breech - gas_at - 0.012f, r_bore + 0.0012f}, {z_breech - gas_at - 0.01f, r_gas},
                          {z_breech - gas_at + 0.012f, r_gas}, {z_breech - gas_at + 0.014f, r_bore}, {len - 0.014f, r_bore},
                          {len - 0.012f, r_bore + 0.0009f}, {len - 0.001f, r_bore + 0.0009f}, {len, r_bore * 0.72f});
  cwg_add(c, a, g, mat_steel, (CwgT){.y = y, .z = z_breech, .ry = M_PI});
  cwg_add(c, a, cwg_tube_z(c, r_bore * 0.7f, r_bore * 0.42f, len * 0.5f, 14, 0.0002f), mat_cavity,
          (CwgT){.y = y, .z = z_muzzle + len * 0.25f});
  if (!o->no_knurl)
    cwg_add(c, a, cwg_knurl_band(c, r_bore + 0.0006f, 0.012f, 26, 0.00035f, 3), mat_steel, (CwgT){.y = y, .z = z_muzzle + 0.026f});
  CWG_SCOPE_END();
  return (CwgBarrel){gas_at, r_bore};
}

// low-profile gas block with two set screws and the tube back into the receiver
void cwg_add_gas_block(Cwg *c, CwpAsm *a, u32 mat_steel, const CwgGasOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 y = o->y, z = o->z;
  f32 r = o->r_barrel > 0.0f ? o->r_barrel : 0.0072f;
  f32 w = o->w > 0.0f ? o->w : 0.021f;
  f32 h = o->h > 0.0f ? o->h : 0.019f;
  f32 len = o->len > 0.0f ? o->len : 0.026f;
  cwg_add(c, a, cwg_box(c, w, h, len, 0.0008f, 2), mat_steel, (CwgT){.y = y - 0.0015f, .z = z});
  cwg_add_screw(c, a, mat_steel, 0.0f, y - h * 0.5f + 0.0015f, z - 0.007f, 0.0022f, 'y', 0.006f);
  cwg_add_screw(c, a, mat_steel, 0.0f, y - h * 0.5f + 0.0015f, z + 0.007f, 0.0022f, 'y', 0.006f);
  f32 tube_len = o->tube_to - z;
  cwg_add(c, a, cwg_tube_z(c, 0.0026f, 0.0014f, m_fabsf(tube_len), 10, 0.0002f), mat_steel,
          (CwgT){.y = y + r + 0.0052f, .z = z + tube_len * 0.5f});
  CWG_SCOPE_END();
}

hz_internal f32 cwg_muzzle_len(u32 kind) {
  switch (kind) {
  case CWG_MUZZLE_BRAKE: return 0.062f;
  case CWG_MUZZLE_A2: return 0.0483f;
  case CWG_MUZZLE_COMP: return 0.058f;
  default: return 0.042f;
  }
}

// brake, A2 birdcage, compensator or tri-lug, each with a real bore, a crush washer and a crowned exit
CwgMuzzleOut cwg_add_muzzle_device(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_cavity, u32 kind, f32 z_barrel_end, f32 r_barrel, f32 y) {
  CWG_SCOPE_BEGIN(c);
  f32 len = cwg_muzzle_len(kind);
  f32 r_out = r_barrel + 0.0038f;
  f32 z_crown = z_barrel_end - len;
  PgMesh *dev = cwg_mesh(c);
  if (kind == CWG_MUZZLE_BRAKE) {
    pg_mesh_append(dev, cwg_lathe_z(c, 20, {0.0f, r_barrel + 0.0012f}, {0.006f, r_barrel + 0.0022f}, {0.008f, r_out},
                                    {len - 0.01f, r_out}, {len - 0.008f, r_out * 0.96f}, {len - 0.002f, r_out * 0.96f},
                                    {len, r_out * 0.8f}, {len, r_barrel * 0.66f}, {len - 0.006f, r_barrel * 0.62f}));
    PgMesh *port = cwg_box(c, r_out * 2.4f, 0.0055f, 0.0072f, 0.0006f, 1);
    for (u32 i = 0; i < 3; i++) {
      u32 from = dev->pos.len;
      pg_mesh_append(dev, port);
      pg_mesh_translate(dev, (v3){0.0f, 0.0f, 0.016f + (f32)i * 0.013f}, from);
    }
  } else if (kind == CWG_MUZZLE_A2) {
    pg_mesh_append(dev, cwg_lathe_z(c, 20, {0.0f, r_barrel + 0.001f}, {0.005f, r_barrel + 0.002f}, {0.007f, r_out * 0.92f},
                                    {0.012f, r_out}, {len - 0.004f, r_out}, {len, r_out * 0.86f}, {len, r_barrel * 0.6f},
                                    {len - 0.005f, r_barrel * 0.58f}));
    PgMesh *slot = cwg_box(c, 0.0032f, 0.0075f, 0.021f, 0.0005f, 1);
    for (u32 i = 0; i < 5; i++) {
      f32 ang = -M_PI * 0.44f + ((f32)i / 4.0f) * M_PI * 0.88f;
      PgMesh *s = cwg_clone(c, slot);
      cwg_translate(s, 0.0f, r_out * 0.82f, 0.0f);
      cwg_rotate_z(s, ang);
      cwg_translate(s, 0.0f, 0.0f, 0.03f);
      pg_mesh_append(dev, s);
    }
  } else if (kind == CWG_MUZZLE_COMP) {
    pg_mesh_append(dev, cwg_lathe_z(c, 20, {0.0f, r_barrel + 0.0012f}, {0.005f, r_barrel + 0.003f}, {0.008f, r_out + 0.0016f},
                                    {0.03f, r_out + 0.0016f}, {0.031f, r_out + 0.0022f}, {len - 0.003f, r_out + 0.0022f},
                                    {len, r_out + 0.0006f}, {len, r_barrel * 0.7f}, {len - 0.007f, r_barrel * 0.66f}));
    PgMesh *k = cwg_knurl_band(c, r_out + 0.0018f, 0.018f, 30, 0.0003f, 4);
    cwg_translate(k, 0.0f, 0.0f, 0.018f);
    pg_mesh_append(dev, k);
  } else {
    pg_mesh_append(dev, cwg_lathe_z(c, 18, {0.0f, r_barrel + 0.0014f}, {0.004f, r_barrel + 0.0026f}, {0.006f, r_out}, {0.024f, r_out},
                                    {0.026f, r_out - 0.0012f}, {len - 0.002f, r_out - 0.0012f}, {len, r_out - 0.003f},
                                    {len, r_barrel * 0.62f}, {len - 0.005f, r_barrel * 0.6f}));
    PgMesh *lug = cwg_box(c, 0.0042f, 0.0038f, 0.012f, 0.0005f, 1);
    for (u32 i = 0; i < 3; i++) {
      f32 ang = ((f32)i / 3.0f) * CWG_TAU;
      PgMesh *l = cwg_clone(c, lug);
      cwg_translate(l, 0.0f, r_out + 0.0012f, 0.0f);
      cwg_rotate_z(l, ang);
      cwg_translate(l, 0.0f, 0.0f, 0.008f);
      pg_mesh_append(dev, l);
    }
  }
  cwg_add(c, a, dev, mat_steel, (CwgT){.y = y, .z = z_crown + len, .ry = M_PI});
  PgMesh *washer = cwg_lathe_z(c, 16, {0.0f, r_barrel + 0.0012f}, {0.0f, r_barrel + 0.0032f}, {0.0018f, r_barrel + 0.0032f},
                               {0.0018f, r_barrel + 0.0012f});
  cwg_add(c, a, washer, mat_steel, (CwgT){.y = y, .z = z_crown + len});
  cwg_add(c, a, cwg_tube_z(c, r_barrel * 0.66f, r_barrel * 0.4f, len * 0.9f, 14, 0.0002f), mat_cavity,
          (CwgT){.y = y, .z = z_crown + len * 0.5f});
  CWG_SCOPE_END();
  return (CwgMuzzleOut){len, z_crown};
}

// ---------------------------------------------------------------- handguard ---

// free-float handguard of longitudinal slats with real gaps, M-LOK slots, ring braces and an end cap
void cwg_add_handguard(Cwg *c, CwpAsm *a, u32 mat_alu, const CwgHandguardOpts *o) {
  CWG_SCOPE_BEGIN(c);
  u32 mat_panel = o->mat_panel;
  f32 yb = o->y, z0 = o->z0, z1 = o->z1;
  f32 len = z0 - z1;
  f32 r_out = o->r > 0.0f ? o->r : 0.0235f;
  u32 sides = o->sides ? o->sides : 8u;
  f32 slat_w = o->slat_w > 0.0f ? o->slat_w : 0.0135f;
  f32 slat_t = o->slat_t > 0.0f ? o->slat_t : 0.0032f;
  f32 cz = (z0 + z1) * 0.5f;

  PgMesh *collar = cwg_lathe_z(c, 18, {0.0f, r_out * 0.72f}, {0.0f, r_out + 0.0018f}, {0.0025f, r_out + 0.0026f},
                               {0.014f, r_out + 0.0026f}, {0.0165f, r_out + 0.0012f}, {0.0165f, r_out * 0.72f});
  cwg_add(c, a, collar, mat_alu, (CwgT){.y = yb, .z = z0 - 0.0165f});
  cwg_add(c, a, cwg_knurl_band(c, r_out + 0.0028f, 0.011f, 34, 0.00035f, 3), mat_alu, (CwgT){.y = yb, .z = z0 - 0.0085f});

  PgMesh *slat = cwg_box(c, slat_w, slat_t, len - 0.019f, 0.0006f, 1);
  PgMesh *slot_geo = cwg_mlok_slot(c, 0.026f, 0.0072f, 0.0018f);
  cwg_rotate_y(slot_geo, CWG_HALF_PI);
  for (u32 i = 0; i < sides; i++) {
    f32 ang = ((f32)i / (f32)sides) * CWG_TAU + M_PI / (f32)sides;
    b32 is_top = m_fabsf(m_sinf(ang) - 1.0f) < 0.35f;
    f32 y = m_sinf(ang) * (r_out - slat_t * 0.5f);
    f32 x = m_cosf(ang) * (r_out - slat_t * 0.5f);
    if (is_top) {
      if (!o->has_top)
        continue;
      f32 t_len = m_fabsf(o->top_from - o->top_to);
      PgMesh *top = cwg_box(c, slat_w, slat_t, t_len, 0.0006f, 1);
      cwg_add(c, a, top, mat_panel, (CwgT){.x = x, .y = yb + y, .z = (o->top_from + o->top_to) * 0.5f, .rz = ang - CWG_HALF_PI});
      continue;
    }
    cwg_add(c, a, slat, mat_panel, (CwgT){.x = x, .y = yb + y, .z = cz - 0.0095f, .rz = ang - CWG_HALF_PI});
    b32 cardinal = m_fabsf(m_cosf(ang)) > 0.85f || m_sinf(ang) < -0.85f;
    if (cardinal) {
      u32 slots = o->slots ? o->slots : 3u;
      PgMesh *pocket = cwg_box(c, 0.0012f, 0.0052f, 0.0232f, 0.0002f, 1);
      for (u32 s = 0; s < slots; s++) {
        f32 sz = cz + len * 0.5f - 0.045f - (f32)s * 0.038f;
        if (sz < z1 + 0.02f)
          break;
        cwg_add(c, a, slot_geo, mat_panel, (CwgT){.x = x * 1.005f, .y = yb + y * 1.005f, .z = sz, .rz = ang});
        cwg_add(c, a, pocket, CWP_MAT_CAVITY, (CwgT){.x = x * 0.955f, .y = yb + y * 0.955f, .z = sz, .rz = ang});
      }
    }
  }

  u32 brace_count = o->braces ? o->braces : 3u;
  u32 ring_seg = sides * 2 > 10 ? sides * 2 : 10u;
  for (u32 i = 0; i < brace_count; i++) {
    f32 denom = brace_count - 1 > 1 ? (f32)(brace_count - 1) : 1.0f;
    f32 z = z0 - 0.03f - ((f32)i / denom) * (len - 0.07f);
    PgMesh *brace = cwg_lathe_z(c, ring_seg, {0.0f, r_out - slat_t}, {0.0f, r_out + 0.0006f}, {0.0035f, r_out + 0.0006f},
                                {0.0035f, r_out - slat_t});
    cwg_add(c, a, brace, mat_alu, (CwgT){.y = yb, .z = z});
  }
  PgMesh *cap = cwg_lathe_z(c, ring_seg, {0.0f, r_out - slat_t - 0.0008f}, {0.0f, r_out - 0.0002f}, {0.0022f, r_out - 0.0012f},
                            {0.0022f, r_out - slat_t - 0.0008f});
  cwg_add(c, a, cap, mat_alu, (CwgT){.y = yb, .z = z1 + 0.001f});
  CWG_SCOPE_END();
}

// ----------------------------------------------------------------- receiver ---

// AR-pattern upper: closed flat-top tube, rail on the crest, forward assist, brass deflector, port with a hung-open dust cover
void cwg_add_upper_receiver(Cwg *c, CwpAsm *a, u32 mat, u32 mat_steel, u32 mat_cavity, const CwgUpperOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 z_rear = o->z_rear, z_front = o->z_front, bore = o->bore;
  f32 r = o->r > 0.0f ? o->r : 0.0192f;
  f32 len = z_rear - z_front;
  f32 cz = (z_rear + z_front) * 0.5f;

  PgMesh *body = cwg_lathe_z(c, 22, {0.0f, 0.0f}, {0.0f, r * 0.98f}, {0.0022f, r}, {len * 0.52f, r}, {len * 0.54f, r * 0.985f},
                             {len - 0.004f, r * 0.985f}, {len, r * 0.93f}, {len, 0.0f});
  cwg_add(c, a, body, mat, (CwgT){.y = bore, .z = z_rear, .ry = M_PI});
  cwg_add(c, a, cwg_box(c, 0.0235f, 0.008f, len - 0.002f, 0.0008f, 1), mat, (CwgT){.y = bore + r - 0.0025f, .z = cz});
  cwg_add(c, a, cwg_box(c, 0.0245f, 0.011f, 0.05f, 0.0012f, 2), mat, (CwgT){.y = bore + r - 0.0075f, .z = z_rear - 0.024f});

  PgMesh *fa = cwg_lathe_z(c, 14, {0.0f, 0.0f}, {0.0f, 0.0055f}, {0.0015f, 0.0062f}, {0.006f, 0.0062f}, {0.007f, 0.0048f},
                           {0.019f, 0.0048f}, {0.019f, 0.0f});
  cwg_add(c, a, fa, mat, (CwgT){.x = 0.0115f, .y = bore - 0.004f, .z = z_rear - 0.006f, .rx = 0.35f});
  cwg_add(c, a, cwg_box(c, 0.0085f, 0.0085f, 0.0035f, 0.0008f, 2), mat_steel, (CwgT){.x = 0.0132f, .y = bore - 0.0025f, .z = z_rear + 0.0025f, .rx = 0.35f});

  PgMesh *defl = cwg_ext(c, 0.016f, 0.0009f, {0.0f, 0.0f}, {0.013f, 0.004f}, {0.013f, 0.019f}, {0.0f, 0.017f});
  cwg_add(c, a, defl, mat, (CwgT){.x = r - 0.001f, .y = bore - 0.006f, .z = z_rear - 0.045f, .ry = CWG_HALF_PI});

  f32 port_w = 0.032f, port_h = 0.019f;
  cwg_add(c, a, cwg_box(c, port_h, 0.012f, port_w, 0.0008f, 1), mat_cavity,
          (CwgT){.x = r - 0.0075f, .y = bore + 0.001f, .z = o->port_z, .ry = CWG_HALF_PI});
  PgMesh *lip = cwg_ext_rr(c, port_w + 0.005f, port_h + 0.005f, 0.0022f, 3, 0.0022f, 0.0006f);
  PgMesh *lip_inner = cwg_ext_rr(c, port_w, port_h, 0.0018f, 3, 0.003f, 0.0005f);
  cwg_add(c, a, lip, mat, (CwgT){.x = r - 0.0022f, .y = bore + 0.001f, .z = o->port_z, .ry = CWG_HALF_PI});
  cwg_add(c, a, lip_inner, mat_cavity, (CwgT){.x = r - 0.0042f, .y = bore + 0.001f, .z = o->port_z, .ry = CWG_HALF_PI});

  f32 hinge_y = bore - 0.0092f, hinge_x = r - 0.0035f;
  cwg_add(c, a, cwg_rod_z(c, 0.0016f, 0.0016f, port_w + 0.014f, 10, 0.0003f), mat_steel, (CwgT){.x = hinge_x, .y = hinge_y, .z = o->port_z});
  f32 cover_open = 1.35f;
  PgMesh *cover = cwg_mesh(c);
  pg_mesh_append(cover, cwg_box(c, port_h + 0.004f, 0.0014f, port_w + 0.006f, 0.0005f, 1));
  PgMesh *flange = cwg_box(c, port_h + 0.004f, 0.0032f, 0.0016f, 0.0004f, 1);
  for (u32 s = 0; s < 2; s++) {
    f32 sz = s == 0 ? -1.0f : 1.0f;
    u32 from = cover->pos.len;
    pg_mesh_append(cover, flange);
    pg_mesh_translate(cover, (v3){0.0f, 0.0009f, sz * (port_w * 0.5f + 0.0022f)}, from);
  }
  u32 from = cover->pos.len;
  pg_mesh_append(cover, cwg_box(c, 0.0018f, 0.0034f, port_w + 0.006f, 0.0004f, 1));
  pg_mesh_translate(cover, (v3){(port_h + 0.004f) * 0.5f - 0.0009f, 0.001f, 0.0f}, from);
  cwg_translate(cover, (port_h + 0.004f) * 0.5f, 0.0f, 0.0f);
  cwg_rotate_z(cover, -cover_open);
  cwg_add(c, a, cover, mat, (CwgT){.x = hinge_x, .y = hinge_y, .z = o->port_z});

  cwg_add_rail(c, a, mat, z_front + 0.002f, z_rear - 0.002f, o->rail_top, 0.0f, NULL);
  cwg_add_pin(c, a, mat_steel, 0.0f, bore - r + 0.004f, z_front + 0.014f, 0.0024f, r * 2.0f - 0.004f);
  CWG_SCOPE_END();
}

// bolt carrier group seen through the ejection port (its own assembly because it cycles)
void cwg_add_bolt_carrier(Cwg *c, CwpAsm *a, u32 mat_steel, f32 y, f32 r, f32 len, f32 z) {
  CWG_SCOPE_BEGIN(c);
  PgMesh *body = cwg_lathe_z(c, 18, {0.0f, r * 0.6f}, {0.0f, r}, {0.002f, r + 0.0004f}, {len * 0.45f, r + 0.0004f}, {len * 0.47f, r},
                             {len, r}, {len, r * 0.5f});
  cwg_add(c, a, body, mat_steel, (CwgT){.y = y, .z = z, .ry = M_PI});
  cwg_add(c, a, cwg_box(c, 0.011f, 0.0075f, 0.016f, 0.0006f, 1), mat_steel, (CwgT){.y = y + r + 0.0026f, .z = z + len * 0.25f});
  cwg_add(c, a, cwg_box(c, 0.006f, 0.005f, 0.03f, 0.0005f, 1), mat_steel, (CwgT){.x = r * 0.78f, .y = y + r * 0.42f, .z = z + len * 0.1f, .rz = 0.5f});
  CWG_SCOPE_END();
}

// AR lower: body, hollow tilted magwell with liner and flared mouth, buffer tower, trigger guard, grip boss
void cwg_add_lower_receiver(Cwg *c, CwpAsm *a, u32 mat, u32 mat_steel, const CwgLowerOpts *o) {
  (void)mat_steel;
  CWG_SCOPE_BEGIN(c);
  f32 bore = o->bore, z_rear = o->z_rear, z_front = o->z_front;
  f32 w = o->w > 0.0f ? o->w : 0.0245f;
  f32 mag_w = o->mag_w > 0.0f ? o->mag_w : 0.0295f;
  f32 mag_d = o->mag_d > 0.0f ? o->mag_d : 0.0685f;
  f32 mag_top = o->mag_top != 0.0f ? o->mag_top : bore - 0.014f;
  f32 mag_bottom = o->mag_bottom != 0.0f ? o->mag_bottom : bore - 0.062f;
  f32 mag_z = o->mag_z, mag_tilt = o->mag_tilt != 0.0f ? o->mag_tilt : 0.09f;

  cwg_add(c, a, cwg_box(c, w, 0.026f, z_rear - z_front, 0.0016f, 2), mat, (CwgT){.y = bore - 0.014f, .z = (z_rear + z_front) * 0.5f});

  f32 well_h = mag_top - mag_bottom;
  PgPoly well_hole = cwg_rr(c, mag_w - 0.005f, mag_d - 0.005f, 0.006f, 5);
  PgPoly well_outer = cwg_rr(c, mag_w, mag_d, 0.0075f, 5);
  PgMesh *well = cwg_extrude(c, well_outer.pts, well_outer.count, well_h, 0.0012f, 1, CWG_HOLES(well_hole));
  cwg_add(c, a, well, mat, (CwgT){.y = (mag_top + mag_bottom) * 0.5f, .z = mag_z, .rx = CWG_HALF_PI + mag_tilt});
  PgPoly liner_hole = cwg_rr(c, mag_w - 0.0082f, mag_d - 0.0082f, 0.005f, 5);
  PgPoly liner_outer = cwg_rr(c, mag_w - 0.0052f, mag_d - 0.0052f, 0.006f, 5);
  PgMesh *liner = cwg_extrude(c, liner_outer.pts, liner_outer.count, well_h - 0.004f, 0.0006f, 1, CWG_HOLES(liner_hole));
  cwg_add(c, a, liner, CWP_MAT_CAVITY, (CwgT){.y = (mag_top + mag_bottom) * 0.5f, .z = mag_z, .rx = CWG_HALF_PI + mag_tilt});
  PgPoly mouth_hole = cwg_rr(c, mag_w - 0.003f, mag_d - 0.003f, 0.006f, 5);
  PgPoly mouth_outer = cwg_rr(c, mag_w + 0.004f, mag_d + 0.005f, 0.008f, 5);
  PgMesh *mouth = cwg_extrude(c, mouth_outer.pts, mouth_outer.count, 0.006f, 0.0012f, 1, CWG_HOLES(mouth_hole));
  cwg_add(c, a, mouth, mat, (CwgT){.y = mag_bottom + 0.002f, .z = mag_z + m_sinf(mag_tilt) * well_h * 0.5f, .rx = CWG_HALF_PI + mag_tilt});

  cwg_add(c, a, cwg_box(c, w - 0.001f, 0.03f, 0.026f, 0.0014f, 2), mat, (CwgT){.y = bore - 0.0155f, .z = z_rear - 0.012f});

  PgMesh *guard = cwg_extrude(c, CWG_PTS({-0.028f, 0.0f}, {0.03f, 0.0f}, {0.032f, -0.006f}, {0.028f, -0.0225f}, {0.018f, -0.0275f},
                                         {-0.02f, -0.0275f}, {-0.028f, -0.021f}),
                              0.0172f, 0.0011f, 2,
                              CWG_HOLES(CWG_POLY({-0.0225f, -0.003f}, {0.0245f, -0.003f}, {0.0255f, -0.008f}, {0.022f, -0.0205f},
                                                 {0.015f, -0.0235f}, {-0.0165f, -0.0235f}, {-0.0225f, -0.019f})));
  cwg_rotate_y(guard, CWG_HALF_PI);
  cwg_add(c, a, guard, mat, (CwgT){.y = bore - 0.026f, .z = o->trigger_z});

  cwg_add(c, a, cwg_box(c, 0.028f, 0.012f, 0.03f, 0.0012f, 2), mat, (CwgT){.y = bore - 0.0255f, .z = z_rear - 0.028f, .rx = -o->grip_angle * 0.5f});
  CWG_SCOPE_END();
}

// ambidextrous safety selector: a paddle with a detent boss that turns about X
PgMesh *cwg_selector_part(Cwg *c, f32 r) {
  PgMesh *parts = cwg_mesh(c);
  PgMesh *shaft = cwg_rod_z(c, r * 0.62f, r * 0.62f, 0.03f, 12, 0.0004f);
  cwg_rotate_y(shaft, CWG_HALF_PI);
  pg_mesh_append(parts, shaft);
  PgMesh *boss = cwg_lathe_z(c, 12, {0.0f, 0.0f}, {0.0f, r}, {0.0012f, r * 1.1f}, {0.005f, r * 1.1f}, {0.005f, 0.0f});
  cwg_rotate_y(boss, -CWG_HALF_PI);
  cwg_translate(boss, 0.0135f, 0.0f, 0.0f);
  pg_mesh_append(parts, boss);
  PgMesh *paddle = cwg_ext(c, 0.0042f, 0.0008f, {0.0f, -0.0035f}, {0.021f, -0.006f}, {0.024f, 0.0f}, {0.02f, 0.005f}, {0.0f, 0.0045f});
  cwg_rotate_y(paddle, CWG_HALF_PI);
  cwg_translate(paddle, 0.0185f, 0.0f, 0.0f);
  pg_mesh_append(parts, paddle);
  return parts;
}

// trigger blade with the finger-pad serrations, pivot at the origin
PgMesh *cwg_trigger_part(Cwg *c) {
  PgMesh *blade = cwg_extrude(c, CWG_PTS({-0.0045f, 0.0045f}, {0.0048f, 0.0045f}, {0.0056f, -0.008f}, {0.0044f, -0.0158f},
                                         {0.0016f, -0.0202f}, {-0.0032f, -0.0192f}, {-0.0055f, -0.011f}, {-0.006f, -0.002f}),
                              0.0072f, 0.0007f, 2, NULL, 0);
  PgMesh *geo = cwg_mesh(c);
  pg_mesh_append(geo, blade);
  PgMesh *ser = cwg_box(c, 0.0015f, 0.0011f, 0.0066f, 0.0003f, 1);
  for (u32 i = 0; i < 6; i++) {
    u32 from = geo->pos.len;
    pg_mesh_append(geo, ser);
    pg_mesh_transform(geo, m4_trs((v3){0.0049f - (f32)i * 0.0004f, -0.0045f - (f32)i * 0.0026f, 0.0f},
                                  quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, -0.2f - (f32)i * 0.05f), (v3){1.0f, 1.0f, 1.0f}),
                      from);
  }
  cwg_rotate_y(geo, -CWG_HALF_PI);
  return geo;
}

// --------------------------------------------------------------- grip / stock ---

// pistol grip: one extruded side profile, palm swells, beavertail, rubber over-mould, finger ridges and a capped screw
void cwg_add_pistol_grip(Cwg *c, CwpAsm *a, u32 mat_poly, u32 mat_rubber, const CwgGripOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 len = o->len > 0.0f ? o->len : 0.108f;
  f32 w = o->w > 0.0f ? o->w : 0.031f;
  f32 angle = o->angle != 0.0f ? o->angle : 0.38f;
  f32 oy = o->y, oz = o->z;
  f32 zf = -0.0155f, zb = 0.0155f;
  PgMesh *core = cwg_extrude(c, CWG_PTS({zb + 0.004f, 0.008f}, {zf - 0.002f, 0.007f}, {zf - 0.0035f, -0.006f}, {zf - 0.0015f, -0.02f},
                                        {zf - 0.003f, -0.034f}, {zf - 0.0005f, -0.05f}, {zf - 0.002f, -0.064f}, {zf + 0.001f, -0.08f},
                                        {zf + 0.0035f, -len + 0.004f}, {zf + 0.008f, -len}, {zb - 0.006f, -len},
                                        {zb - 0.001f, -len + 0.006f}, {zb + 0.001f, -0.06f}, {zb + 0.0025f, -0.03f},
                                        {zb + 0.006f, -0.012f}),
                             w, 0.0035f, 3, NULL, 0);
  cwg_rotate_y(core, CWG_HALF_PI);
  cwg_add(c, a, core, mat_poly, (CwgT){.y = oy, .z = oz, .rx = -angle});

  PgMesh *swell = cwg_blob(c, 0.008f, len * 0.62f, 0.03f, 0.006f, 3);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    cwg_add(c, a, swell, mat_poly, (CwgT){.x = sx * (w * 0.5f - 0.0015f), .y = oy - len * 0.42f, .z = oz + 0.0035f, .rx = -angle});
  }
  cwg_add(c, a, cwg_blob(c, w * 0.96f, 0.02f, 0.024f, 0.006f, 3), mat_poly, (CwgT){.y = oy + 0.005f, .z = oz + 0.012f, .rx = -angle * 0.6f});
  cwg_add(c, a, cwg_blob(c, w * 1.03f, len * 0.58f, 0.019f, 0.005f, 3), mat_rubber, (CwgT){.y = oy - len * 0.44f, .z = oz + 0.0025f, .rx = -angle});
  PgMesh *ridge = cwg_blob(c, w * 0.9f, 0.011f, 0.007f, 0.003f, 3);
  for (u32 i = 0; i < 4; i++) {
    f32 t = 0.15f + (f32)i * 0.2f;
    f32 yy = oy - t * len;
    f32 zz = oz + zf + 0.001f + m_sinf(t * M_PI) * 0.001f;
    f32 cs = m_cosf(-angle), sn = m_sinf(-angle);
    cwg_add(c, a, ridge, mat_rubber,
            (CwgT){.y = oy + (yy - oy) * cs - (zz - oz) * sn, .z = oz + (yy - oy) * sn + (zz - oz) * cs, .rx = -angle});
  }
  f32 cap_y = oy - m_cosf(angle) * len, cap_z = oz + m_sinf(angle) * len;
  cwg_add(c, a, cwg_blob(c, w * 0.92f, 0.007f, 0.031f, 0.0025f, 2), mat_poly, (CwgT){.y = cap_y + 0.001f, .z = cap_z, .rx = -angle});
  cwg_add_screw(c, a, mat_rubber, 0.0f, cap_y - 0.0015f, cap_z, 0.0026f, 'y', 0.006f);
  CWG_SCOPE_END();
}

// collapsible carbine stock: receiver extension, castle nut, detent notches, extruded shell, lever, grooved pad
void cwg_add_carbine_stock(Cwg *c, CwpAsm *a, u32 mat_alu, u32 mat_poly, u32 mat_rubber, const CwgStockOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 bore = o->bore, z_rear = o->z_rear, z_front = o->z_front;
  f32 y_axis = o->y != 0.0f ? o->y : bore - 0.012f;
  f32 tube_r = 0.0146f;
  f32 len = z_rear - z_front;

  cwg_add(c, a, cwg_tube_z(c, tube_r, tube_r - 0.0022f, len - 0.004f, 18, 0.0004f), mat_alu, (CwgT){.y = y_axis, .z = (z_rear + z_front) * 0.5f});
  PgMesh *nut = cwg_lathe_z(c, 18, {0.0f, tube_r}, {0.0f, tube_r + 0.0034f}, {0.0016f, tube_r + 0.0038f}, {0.0085f, tube_r + 0.0038f},
                            {0.01f, tube_r + 0.003f}, {0.01f, tube_r});
  cwg_add(c, a, nut, mat_alu, (CwgT){.y = y_axis, .z = z_front});
  PgMesh *notch_src = cwg_box(c, 0.0022f, 0.0034f, 0.006f, 0.0004f, 1);
  for (u32 i = 0; i < 6; i++) {
    PgMesh *notch = cwg_clone(c, notch_src);
    cwg_translate(notch, 0.0f, tube_r + 0.0032f, 0.0f);
    cwg_rotate_z(notch, ((f32)i / 6.0f) * CWG_TAU);
    cwg_translate(notch, 0.0f, y_axis, z_front + 0.005f);
    cwg_add(c, a, notch, mat_alu, (CwgT){0});
  }
  PgMesh *detent = cwg_box(c, 0.0075f, 0.0032f, 0.0075f, 0.0006f, 1);
  for (u32 i = 0; i < 6; i++) {
    f32 z = z_front + 0.026f + (f32)i * 0.018f;
    if (z > z_rear - 0.02f)
      break;
    cwg_add(c, a, detent, mat_alu, (CwgT){.y = y_axis - tube_r + 0.0008f, .z = z});
  }

  f32 body_len = 0.104f;
  f32 bz = z_rear - body_len * 0.5f;
  f32 comb_y = y_axis + 0.026f, toe_y = y_axis - 0.042f;
  PgMesh *shell = cwg_extrude(c, CWG_PTS({-body_len * 0.5f, y_axis + 0.004f}, {-body_len * 0.5f + 0.012f, y_axis + 0.017f},
                                         {-body_len * 0.5f + 0.03f, comb_y - 0.002f}, {body_len * 0.5f - 0.012f, comb_y},
                                         {body_len * 0.5f, comb_y - 0.006f}, {body_len * 0.5f, toe_y + 0.008f},
                                         {body_len * 0.5f - 0.008f, toe_y}, {-body_len * 0.5f + 0.028f, toe_y + 0.006f},
                                         {-body_len * 0.5f + 0.008f, y_axis - 0.02f}, {-body_len * 0.5f, y_axis - 0.009f}),
                              0.043f, 0.0035f, 2, NULL, 0);
  cwg_rotate_y(shell, CWG_HALF_PI);
  PgMesh *shell_parts = cwg_mesh(c);
  pg_mesh_append(shell_parts, shell);
  PgMesh *cheek = cwg_blob(c, 0.047f, 0.012f, body_len * 0.66f, 0.005f, 3);
  cwg_translate(cheek, 0.0f, comb_y - 0.002f, -0.006f);
  pg_mesh_append(shell_parts, cheek);
  PgMesh *scallop = cwg_blob(c, 0.005f, 0.024f, 0.052f, 0.005f, 3);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    u32 from = shell_parts->pos.len;
    pg_mesh_append(shell_parts, scallop);
    pg_mesh_translate(shell_parts, (v3){sx * 0.0205f, y_axis - 0.012f, 0.004f}, from);
  }
  cwg_add(c, a, shell_parts, mat_poly, (CwgT){.z = bz});

  PgMesh *lever = cwg_ext(c, 0.014f, 0.0008f, {-0.014f, 0.0f}, {0.016f, 0.0f}, {0.018f, -0.007f}, {0.012f, -0.011f}, {-0.012f, -0.011f},
                          {-0.016f, -0.005f});
  cwg_add(c, a, lever, mat_poly, (CwgT){.y = y_axis - 0.036f, .z = bz + 0.012f});

  cwg_add(c, a, cwg_blob(c, 0.045f, 0.072f, 0.013f, 0.0045f, 3), mat_rubber, (CwgT){.y = y_axis - 0.008f, .z = z_rear - 0.004f, .rx = 0.06f});
  PgMesh *groove = cwg_box(c, 0.043f, 0.0035f, 0.005f, 0.0012f, 2);
  for (u32 i = 0; i < 5; i++)
    cwg_add(c, a, groove, mat_rubber, (CwgT){.y = y_axis + 0.02f - (f32)i * 0.0125f, .z = z_rear + 0.0026f, .rx = 0.06f});

  cwg_add_sling_loop(c, a, mat_alu, 0.0225f, y_axis - 0.026f, bz - 0.03f, 0.0075f, (CwgT){.ry = CWG_HALF_PI});
  cwg_add_qd_socket(c, a, mat_poly, mat_alu, -0.0215f, y_axis - 0.014f, bz - 0.026f, 'x', 0.005f);
  CWG_SCOPE_END();
}

// -------------------------------------------------------------------- magazine ---

// curved magazine as a stack of tilted slices with ribs, feed lips, floor plate, witness holes and the top round
CwgMag cwg_build_magazine(Cwg *c, CwpAsm *a, const CwgMagOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 w = o->w > 0.0f ? o->w : 0.0255f;
  f32 d = o->d > 0.0f ? o->d : 0.0655f;
  f32 len = o->len > 0.0f ? o->len : 0.215f;
  f32 curve = o->curve > 0.0f ? o->curve : 0.028f;
  u32 segs = o->segs ? o->segs : 8u;

  PgMesh *body_parts = cwg_mesh(c);
  PgMesh *rib_parts = cwg_mesh(c);
  f32 step = len / (f32)segs;
  for (u32 i = 0; i < segs; i++) {
    f32 t = ((f32)i + 0.5f) / (f32)segs;
    f32 py = -t * len, pz = -curve * t * t, tilt = m_atan2f(2.0f * curve * t, len);
    f32 taper = 1.0f - t * 0.04f;
    PgMesh *seg = cwg_ext_rr(c, w * taper, d * taper, 0.0055f, 5, step * 1.06f, 0.0008f);
    cwg_rotate_x(seg, CWG_HALF_PI + tilt);
    cwg_translate(seg, 0.0f, py, pz);
    pg_mesh_append(body_parts, seg);
    if (i > 0 && i < segs - 1) {
      PgMesh *rib_src = cwg_box(c, 0.0018f, step * 0.62f, d * 0.66f, 0.0005f, 1);
      for (u32 s = 0; s < 2; s++) {
        f32 sx = s == 0 ? -1.0f : 1.0f;
        PgMesh *rib = cwg_clone(c, rib_src);
        cwg_rotate_x(rib, tilt);
        cwg_translate(rib, sx * (w * taper * 0.5f), py, pz);
        pg_mesh_append(rib_parts, rib);
      }
    }
  }
  PgMesh *lip = cwg_ext(c, d * 0.9f, 0.0005f, {-0.0032f, 0.0f}, {0.0032f, 0.0f}, {0.0026f, 0.009f}, {-0.0026f, 0.009f});
  cwg_rotate_y(lip, CWG_HALF_PI);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    u32 from = body_parts->pos.len;
    pg_mesh_append(body_parts, lip);
    pg_mesh_translate(body_parts, (v3){sx * (w * 0.5f - 0.0032f), -0.0015f, 0.0f}, from);
  }
  PgMesh *notch = cwg_box(c, 0.008f, 0.0075f, 0.0055f, 0.0009f, 1);
  cwg_translate(notch, 0.0f, -0.03f, d * 0.5f + 0.0015f);
  pg_mesh_append(body_parts, notch);

  f32 end_y = -len, end_z = -curve, end_tilt = m_atan2f(2.0f * curve, len);
  PgMesh *plate = cwg_ext_rr(c, w + 0.0026f, d * 0.97f, 0.004f, 4, 0.01f, 0.001f);
  cwg_rotate_x(plate, CWG_HALF_PI + end_tilt);
  cwg_translate(plate, 0.0f, end_y - 0.0035f, end_z);
  pg_mesh_append(body_parts, plate);
  PgMesh *ledge = cwg_box(c, w + 0.0034f, 0.007f, 0.013f, 0.0016f, 2);
  cwg_rotate_x(ledge, end_tilt);
  cwg_translate(ledge, 0.0f, end_y - 0.007f, end_z - d * 0.4f);
  pg_mesh_append(body_parts, ledge);
  PgMesh *pad = cwg_ext_rr(c, w + 0.003f, d * 0.9f, 0.004f, 4, 0.005f, 0.0009f);
  cwg_rotate_x(pad, CWG_HALF_PI + end_tilt);
  cwg_translate(pad, 0.0f, end_y - 0.0105f, end_z);

  u32 poly = o->poly;
  cwg_add(c, a, body_parts, poly, (CwgT){0});
  if (rib_parts->idx.len)
    cwg_add(c, a, rib_parts, poly, (CwgT){0});
  cwg_add(c, a, pad, CWP_MAT_RUBBER, (CwgT){0});

  u32 holes = o->witness ? o->witness : 4u;
  PgMesh *hole_src = cwg_ext_rr(c, 0.0085f, 0.0044f, 0.0018f, 3, 0.004f, 0.0004f);
  cwg_rotate_y(hole_src, CWG_HALF_PI);
  for (u32 i = 0; i < holes; i++) {
    f32 denom = holes - 1 > 1 ? (f32)(holes - 1) : 1.0f;
    f32 t = 0.26f + ((f32)i / denom) * 0.56f;
    f32 py = -t * len, pz = -curve * t * t, tilt = m_atan2f(2.0f * curve * t, len);
    for (u32 s = 0; s < 2; s++) {
      f32 sx = s == 0 ? -1.0f : 1.0f;
      PgMesh *h = cwg_clone(c, hole_src);
      cwg_rotate_x(h, tilt);
      cwg_translate(h, sx * (w * 0.5f - 0.0006f), py, pz);
      cwg_add(c, a, h, CWP_MAT_CAVITY, (CwgT){0});
    }
  }

  f32 case_len = o->case_len > 0.0f ? o->case_len : 0.0446f;
  f32 bullet_len = o->bullet_len > 0.0f ? o->bullet_len : 0.019f;
  CwgCartridge ct = cwg_cartridge(c, case_len, o->rim_r > 0.0f ? o->rim_r : 0.00495f, bullet_len);
  f32 cz = m_minf(d * 0.5f - 0.0025f, case_len + bullet_len - d * 0.5f + 0.0015f);
  cwg_add(c, a, ct.brass, CWP_MAT_BRASS, (CwgT){.y = -0.0085f, .z = cz, .ry = M_PI});
  cwg_add(c, a, ct.bullet, CWP_MAT_COPPER, (CwgT){.y = -0.0085f, .z = cz, .ry = M_PI});
  CWG_SCOPE_END();
  return (CwgMag){len, w, d};
}

// ------------------------------------------------------------ optics + sights ---

// a flat annulus (inner, outer, seg) facing +Z
hz_internal PgMesh *cwg_flat_ring(Cwg *c, f32 inner, f32 outer, u32 seg) {
  PgMesh *m = cwg_mesh(c);
  for (u32 j = 0; j < 2; j++) {
    f32 radius = j == 0 ? inner : outer;
    for (u32 i = 0; i <= seg; i++) {
      f32 s = ((f32)i / (f32)seg) * CWG_TAU;
      v3 p = {radius * m_cosf(s), radius * m_sinf(s), 0.0f};
      pg_mesh_push_vertex(m, p, (v3){0.0f, 0.0f, 1.0f}, (v2){(p.x / outer + 1.0f) * 0.5f, (p.y / outer + 1.0f) * 0.5f});
    }
  }
  for (u32 i = 0; i < seg; i++) {
    u32 s = i;
    u32 va = s, vb = s + seg + 1, vc = s + seg + 2, vd = s + 1;
    pg_mesh_push_tri(m, va, vb, vd);
    pg_mesh_push_tri(m, vb, vc, vd);
  }
  return m;
}

// tube red-dot sight: belled objective, flared bore, glass, turrets, cantilever mount and scope rings
CwpOptic cwg_build_optic(Cwg *c, CwpAsm *a, const CwgOpticOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 r_tube = o->r_tube > 0.0f ? o->r_tube : 0.0155f;
  f32 len = o->len > 0.0f ? o->len : 0.068f;
  u32 mat_body = o->mat_body, mat_steel = o->mat_steel;
  f32 y = o->y, z = o->z, rail_top = o->rail_top;
  u32 seg = 72, seg_in = 80;

  f32 r_bore_oc = r_tube * 0.787f, r_bore_ob = r_tube * 1.065f, r_bell_ob = r_tube * 1.226f;
  f32 z_oc = len * 0.5f, z_ob = -len * 0.5f;

  PgMesh *tube = cwg_lathe_z(c, seg, {z_ob, r_bore_ob * 0.995f}, {z_ob + 0.0004f, r_bell_ob * 0.99f}, {z_ob, r_bell_ob * 1.008f},
                             {z_ob + 0.0022f, r_bell_ob}, {z_ob + 0.008f, r_bell_ob * 0.995f}, {z_ob + 0.014f, r_tube * 1.1f},
                             {z_ob + 0.022f, r_tube * 1.01f}, {z_ob + 0.03f, r_tube}, {z_oc - 0.012f, r_tube},
                             {z_oc - 0.01f, r_tube * 1.05f}, {z_oc - 0.002f, r_tube * 1.05f}, {z_oc - 0.0003f, r_tube * 1.02f},
                             {z_oc, r_tube * 0.995f}, {z_oc, r_bore_oc * 1.02f});
  cwg_add(c, a, tube, mat_body, (CwgT){.y = y, .z = z});

  PgMesh *baffle = cwg_lathe_z(c, seg_in, {z_ob + 0.001f, r_bore_ob}, {z_ob + 0.001f, r_bore_ob * 0.985f},
                               {z_oc - 0.009f, r_bore_oc * 0.985f}, {z_oc - 0.009f, r_bore_oc});
  cwg_add(c, a, baffle, CWP_MAT_OPTIC_TUBE, (CwgT){.y = y, .z = z});

  f32 lens_r = r_bore_oc * 0.99f;
  PgMesh *relief = cwg_lathe_z(c, seg_in, {0.0f, lens_r * 0.998f}, {0.0012f, lens_r * 1.012f}, {0.0034f, r_bore_oc * 1.01f},
                               {0.0038f, r_tube * 1.0f}, {0.0038f, r_bore_oc}, {0.0f, r_bore_oc});
  cwg_add(c, a, relief, CWP_MAT_OPTIC_TUBE, (CwgT){.y = y, .z = z + z_oc - 0.0045f});

  PgMesh *lens_oc = cwg_lathe_z(c, seg_in, {0.0f, 0.0f}, {-0.0009f, lens_r * 0.6f}, {-0.0014f, lens_r});
  PgMesh *lens_ob = cwg_lathe_z(c, seg_in, {0.0f, 0.0f}, {-0.0012f, r_bore_ob * 0.58f}, {-0.0019f, r_bore_ob * 0.985f});
  cwg_add(c, a, lens_ob, CWP_MAT_GLASS, (CwgT){.y = y, .z = z + z_ob + 0.0055f});
  cwg_add(c, a, lens_oc, CWP_MAT_GLASS, (CwgT){.y = y, .z = z + z_oc - 0.007f, .ry = M_PI});

  cwg_add(c, a, cwg_flat_ring(c, lens_r * 0.965f, lens_r * 0.99f, seg_in), CWP_MAT_LENS_RING, (CwgT){.y = y, .z = z + z_oc - 0.0066f});
  PgMesh *vig = cwg_mesh(c);
  pg_circle(vig, &(PgCircleDesc){.radius = lens_r * 0.995f, .segments = seg_in, .theta_start = 0.0f, .theta_length = CWG_TAU});
  cwg_add(c, a, vig, CWP_MAT_LENS_VIG, (CwgT){.y = y, .z = z + z_oc - 0.0085f});

  PgMesh *turret = cwg_mesh(c);
  pg_mesh_append(turret, cwg_lathe_z(c, 32, {0.0f, 0.0062f}, {0.004f, 0.0075f}, {0.0075f, 0.0075f}, {0.0085f, 0.0068f},
                                     {0.0125f, 0.0068f}, {0.0128f, 0.006f}, {0.0128f, 0.0f}));
  PgMesh *turret_knurl = cwg_knurl_band(c, 0.0072f, 0.0052f, 26, 0.00032f, 3);
  cwg_translate(turret_knurl, 0.0f, 0.0f, 0.0102f);
  pg_mesh_append(turret, turret_knurl);
  PgMesh *marks = cwg_mesh(c);
  for (u32 i = 0; i < 12; i++) {
    f32 ang = ((f32)i / 12.0f) * CWG_TAU;
    f32 h = i == 0 ? 0.0026f : 0.0014f;
    PgMesh *t = cwg_box(c, 0.00035f, h, 0.0006f, 0.00008f, 1);
    cwg_rotate_z(t, ang);
    cwg_translate(t, m_cosf(ang) * (0.0075f - h * 0.42f), m_sinf(ang) * (0.0075f - h * 0.42f), 0.0f);
    pg_mesh_append(marks, t);
  }
  CwgT elev = {.y = y + r_tube * 0.9f, .z = z + 0.004f, .rx = -CWG_HALF_PI};
  CwgT wind = {.x = r_tube * 0.9f, .y = y, .z = z + 0.004f, .ry = CWG_HALF_PI};
  cwg_add(c, a, turret, mat_body, elev);
  cwg_add(c, a, turret, mat_body, wind);
  CwgT elev_m = elev, wind_m = wind;
  elev_m.y = elev.y + 0.0055f;
  wind_m.x = wind.x + 0.0055f;
  cwg_add(c, a, marks, CWP_MAT_CAVITY, elev_m);
  cwg_add(c, a, marks, CWP_MAT_CAVITY, wind_m);

  PgMesh *dial = cwg_lathe_z(c, 32, {0.0f, 0.008f}, {0.005f, 0.0092f}, {0.0125f, 0.0092f}, {0.0128f, 0.008f}, {0.0128f, 0.0f});
  cwg_add(c, a, dial, mat_body, (CwgT){.x = -r_tube * 0.9f, .y = y, .z = z - 0.006f, .ry = -CWG_HALF_PI});
  cwg_add(c, a, cwg_knurl_band(c, 0.0094f, 0.006f, 26, 0.00028f, 3), mat_body,
          (CwgT){.x = -r_tube * 0.9f - 0.008f, .y = y, .z = z - 0.006f, .ry = -CWG_HALF_PI});

  f32 mount_top = y - r_tube;
  f32 mount_h = mount_top - rail_top;
  PgMesh *base = cwg_ext(c, 0.03f, 0.0008f, {-0.0092f, 0.0f}, {0.0092f, 0.0f}, {0.0105f, -0.0025f}, {0.0072f, -mount_h * 0.45f},
                         {0.0072f, -mount_h + 0.005f}, {0.013f, -mount_h + 0.0018f}, {0.013f, -mount_h}, {-0.013f, -mount_h},
                         {-0.013f, -mount_h + 0.0018f}, {-0.0072f, -mount_h + 0.005f}, {-0.0072f, -mount_h * 0.45f},
                         {-0.0105f, -0.0025f});
  cwg_add(c, a, base, mat_body, (CwgT){.y = mount_top, .z = z + 0.002f});
  PgMesh *clamp = cwg_lathe_z(c, seg, {0.0f, r_tube}, {0.0f, r_tube + 0.0035f}, {0.0055f, r_tube + 0.0035f}, {0.0055f, r_tube});
  cwg_add(c, a, clamp, mat_body, (CwgT){.y = y, .z = z - 0.014f});
  cwg_add(c, a, clamp, mat_body, (CwgT){.y = y, .z = z + 0.012f});
  cwg_add_screw(c, a, mat_steel, 0.0135f, mount_top - 0.004f, z - 0.0115f, 0.0028f, 'x', 0.01f);
  cwg_add_screw(c, a, mat_steel, 0.0135f, mount_top - 0.004f, z + 0.0145f, 0.0028f, 'x', 0.01f);
  cwg_add(c, a, cwg_box(c, 0.032f, 0.006f, 0.03f, 0.0008f, 1), mat_body, (CwgT){.y = rail_top + 0.001f, .z = z + 0.002f});
  cwg_add_screw(c, a, mat_steel, 0.0165f, rail_top + 0.001f, z - 0.008f, 0.003f, 'x', 0.012f);
  cwg_add_screw(c, a, mat_steel, 0.0165f, rail_top + 0.001f, z + 0.012f, 0.003f, 'x', 0.012f);

  PgMesh *cup = cwg_lathe_z(c, seg, {0.0f, r_bore_oc * 0.995f}, {0.0004f, r_bore_oc * 1.03f}, {0.0009f, r_tube * 1.02f},
                            {0.0018f, r_tube * 1.075f}, {0.0055f, r_tube * 1.1f}, {0.0072f, r_tube * 1.09f}, {-0.0042f, r_tube * 1.085f},
                            {-0.0048f, r_tube * 1.03f});
  cwg_add(c, a, cup, CWP_MAT_RUBBER, (CwgT){.y = y, .z = z + z_oc - 0.0012f});
  f32 hood_len = o->hood > 0.0f ? o->hood : 0.009f;
  PgMesh *hood = cwg_lathe_z(c, seg, {0.0f, r_bell_ob * 1.0f}, {0.0f, r_bell_ob * 1.05f}, {hood_len - 0.0003f, r_bell_ob * 1.05f},
                             {hood_len, r_bell_ob * 1.035f}, {hood_len, r_bell_ob * 0.99f});
  cwg_add(c, a, hood, mat_body, (CwgT){.y = y, .z = z + z_ob - hood_len + 0.0015f});
  PgMesh *hood_liner = cwg_tube_z(c, r_bell_ob * 1.035f, r_bell_ob * 0.998f, hood_len - 0.0008f, seg, 0.0002f);
  cwg_add(c, a, hood_liner, CWP_MAT_OPTIC_TUBE, (CwgT){.y = y, .z = z + z_ob - hood_len * 0.5f + 0.0015f});
  PgMesh *bumper = cwg_lathe_z(c, seg, {0.0f, r_bell_ob * 1.01f}, {0.0006f, r_bell_ob * 1.075f}, {0.0038f, r_bell_ob * 1.08f},
                               {0.005f, r_bell_ob * 1.03f});
  cwg_add(c, a, bumper, CWP_MAT_RUBBER, (CwgT){.y = y, .z = z + z_ob - hood_len - 0.0035f});
  CWG_SCOPE_END();
  return (CwpOptic){.valid = true, .center = {0.0f, y, z}, .lens_z = z + z_oc - 0.007f, .aperture_r = lens_r * 0.94f, .tube_r = r_tube, .len = len};
}

// engraved rollmark: recessed strokes in the part's own space so nothing swims
void cwg_add_rollmark(Cwg *c, CwpAsm *a, u32 mat, const CwgRollmark *o) {
  CWG_SCOPE_BEGIN(c);
  f32 h = o->h > 0.0f ? o->h : 0.0036f;
  f32 stroke = o->stroke > 0.0f ? o->stroke : 0.0006f;
  f32 depth = o->depth > 0.0f ? o->depth : 0.0008f;
  f32 pitch = o->pitch > 0.0f ? o->pitch : 0.0017f;
  const u32 *pat = o->pattern ? o->pattern : (const u32[]){3, 2, 3, 3, 1, 0, 2, 3, 2, 3, 0, 3, 1, 2, 3, 2, 0, 3, 3, 2};
  u32 pat_len = o->pattern ? o->pattern_len : 20u;
  u32 n = o->count ? o->count : pat_len;
  PgMesh *g = cwg_mesh(c);
  for (u32 i = 0; i < n; i++) {
    u32 p = pat[i % pat_len];
    if (p == 0)
      continue;
    f32 bh = h * (0.52f + (f32)p * 0.16f);
    PgMesh *b = cwg_box(c, depth, bh, stroke, 0.00008f, 1);
    cwg_translate(b, 0.0f, (h - bh) * 0.5f, -(f32)i * pitch);
    pg_mesh_append(g, b);
    if (p == 3) {
      PgMesh *cb = cwg_box(c, depth, stroke * 0.85f, pitch * 0.72f, 0.00008f, 1);
      cwg_translate(cb, 0.0f, (h - bh) * 0.5f + bh * 0.16f, -(f32)i * pitch - pitch * 0.3f);
      pg_mesh_append(g, cb);
    }
  }
  PgMesh *line = cwg_box(c, depth, stroke * 0.9f, (f32)(n - 1) * pitch, 0.00008f, 1);
  cwg_translate(line, 0.0f, -h * 0.55f, -(f32)(n - 1) * pitch * 0.5f);
  pg_mesh_append(g, line);
  cwg_add(c, a, g, mat, (CwgT){.x = o->x, .y = o->y, .z = o->z});
  CWG_SCOPE_END();
}

// folding front sight: base, hinge, two ears, post and cross-bar
void cwg_add_front_sight(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_alu, f32 x, f32 rail_top, f32 z, b32 up) {
  CWG_SCOPE_BEGIN(c);
  cwg_add(c, a, cwg_box(c, 0.024f, 0.008f, 0.019f, 0.0008f, 1), mat_alu, (CwgT){.x = x, .y = rail_top + 0.004f, .z = z});
  cwg_add(c, a, cwg_rod_z(c, 0.0026f, 0.0026f, 0.026f, 10, 0.0003f), mat_steel, (CwgT){.x = x, .y = rail_top + 0.008f, .z = z + 0.006f, .ry = CWG_HALF_PI});
  f32 h = up ? 0.03f : 0.006f;
  f32 tilt = up ? 0.0f : -1.35f;
  PgMesh *ear = cwg_ext(c, 0.0075f, 0.0005f, {-0.0022f, 0.0f}, {0.0022f, 0.0f}, {0.0022f, h}, {0.0f, h + 0.002f}, {-0.0022f, h});
  PgMesh *ears = cwg_mesh(c);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    u32 from = ears->pos.len;
    pg_mesh_append(ears, ear);
    pg_mesh_translate(ears, (v3){sx * 0.0088f, 0.0f, 0.0f}, from);
  }
  PgMesh *post = cwg_rod_z(c, 0.0011f, 0.0009f, h * 0.72f, 8, 0.0002f);
  cwg_rotate_x(post, CWG_HALF_PI);
  cwg_translate(post, 0.0f, h * 0.36f + 0.002f, 0.0f);
  pg_mesh_append(ears, post);
  PgMesh *cross = cwg_box(c, 0.019f, 0.0022f, 0.0055f, 0.0004f, 1);
  cwg_translate(cross, 0.0f, h - 0.0012f, 0.0f);
  pg_mesh_append(ears, cross);
  cwg_add(c, a, ears, mat_steel, (CwgT){.x = x, .y = rail_top + 0.008f, .z = z, .rx = tilt});
  CWG_SCOPE_END();
}

// folding rear sight: base, leaf, aperture ring and the windage drum
void cwg_add_rear_sight(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_alu, f32 x, f32 rail_top, f32 z, b32 up) {
  CWG_SCOPE_BEGIN(c);
  cwg_add(c, a, cwg_box(c, 0.024f, 0.0085f, 0.022f, 0.0008f, 1), mat_alu, (CwgT){.x = x, .y = rail_top + 0.0042f, .z = z});
  f32 h = up ? 0.027f : 0.005f;
  f32 tilt = up ? 0.0f : 1.35f;
  PgMesh *parts = cwg_mesh(c);
  pg_mesh_append(parts, cwg_ext(c, 0.006f, 0.0006f, {-0.011f, 0.0f}, {0.011f, 0.0f}, {0.011f, h * 0.55f}, {0.006f, h}, {-0.006f, h},
                                {-0.011f, h * 0.55f}));
  PgMesh *ap = cwg_ring(c, 0.0032f, 0.0011f, 14, 6);
  cwg_translate(ap, 0.0f, h * 0.66f, 0.0f);
  pg_mesh_append(parts, ap);
  PgMesh *drum_g = cwg_mesh(c);
  pg_mesh_append(drum_g, cwg_lathe_z(c, 20, {0.0f, 0.0f}, {0.0f, 0.0048f}, {0.0035f, 0.0052f}, {0.008f, 0.0052f}, {0.008f, 0.0f}));
  PgMesh *drum_knurl = cwg_knurl_band(c, 0.0053f, 0.0042f, 22, 0.00028f, 3);
  cwg_translate(drum_knurl, 0.0f, 0.0f, 0.0055f);
  pg_mesh_append(drum_g, drum_knurl);
  cwg_rotate_y(drum_g, CWG_HALF_PI);
  cwg_translate(drum_g, 0.012f, h * 0.3f, 0.0f);
  pg_mesh_append(parts, drum_g);
  cwg_add(c, a, parts, mat_steel, (CwgT){.x = x, .y = rail_top + 0.0085f, .z = z, .rx = tilt});
  CWG_SCOPE_END();
}

// AR charging handle: bar, shaft, T-handle wings with ridges, latch lever, hook and pivot pin
PgMesh *cwg_charging_handle(Cwg *c) {
  PgMesh *parts = cwg_mesh(c);
  PgMesh *bar = cwg_box(c, 0.028f, 0.0055f, 0.052f, 0.0008f, 1);
  cwg_translate(bar, 0.0f, 0.0f, 0.012f);
  pg_mesh_append(parts, bar);
  PgMesh *shaft = cwg_rod_z(c, 0.0055f, 0.0055f, 0.07f, 12, 0.0005f);
  cwg_translate(shaft, 0.0f, -0.0022f, -0.02f);
  pg_mesh_append(parts, shaft);
  PgMesh *wing = cwg_ext(c, 0.0055f, 0.0007f, {0.0f, -0.005f}, {0.02f, -0.0075f}, {0.024f, -0.002f}, {0.024f, 0.004f}, {0.0f, 0.004f});
  PgMesh *wr = cwg_clone(c, wing);
  cwg_rotate_y(wr, CWG_HALF_PI);
  cwg_translate(wr, 0.012f, 0.0f, 0.034f);
  pg_mesh_append(parts, wr);
  PgMesh *wl = cwg_clone(c, wing);
  cwg_rotate_y(wl, -CWG_HALF_PI);
  cwg_translate(wl, -0.012f, 0.0f, 0.034f);
  pg_mesh_append(parts, wl);
  PgMesh *ridge = cwg_box(c, 0.0022f, 0.0075f, 0.0016f, 0.0003f, 1);
  for (u32 i = 0; i < 3; i++) {
    for (u32 s = 0; s < 2; s++) {
      f32 sx = s == 0 ? -1.0f : 1.0f;
      u32 from = parts->pos.len;
      pg_mesh_append(parts, ridge);
      pg_mesh_translate(parts, (v3){sx * (0.017f + (f32)i * 0.003f), 0.0f, 0.031f + (f32)i * 0.0022f}, from);
    }
  }
  PgMesh *latch = cwg_ext(c, 0.0042f, 0.0006f, {0.0f, -0.0032f}, {0.0165f, -0.0042f}, {0.0205f, -0.0018f}, {0.0205f, 0.0026f},
                          {0.0155f, 0.0042f}, {0.0f, 0.0034f});
  cwg_rotate_y(latch, -CWG_HALF_PI);
  cwg_translate(latch, -0.0125f, 0.0012f, 0.0335f);
  pg_mesh_append(parts, latch);
  PgMesh *hook = cwg_box(c, 0.0038f, 0.0052f, 0.0032f, 0.0005f, 1);
  cwg_translate(hook, -0.0295f, 0.0006f, 0.0292f);
  pg_mesh_append(parts, hook);
  PgMesh *pin = cwg_rod_z(c, 0.0011f, 0.0011f, 0.0072f, 8, 0.0002f);
  cwg_rotate_y(pin, CWG_HALF_PI);
  cwg_translate(pin, -0.0135f, 0.0012f, 0.0356f);
  pg_mesh_append(parts, pin);
  PgMesh *pad = cwg_box(c, 0.0028f, 0.0062f, 0.0075f, 0.0004f, 1);
  cwg_translate(pad, -0.0316f, 0.0014f, 0.0345f);
  pg_mesh_append(parts, pad);
  return parts;
}

// angled foregrip: five tapering blobs with rubber ridges on the front
void cwg_add_fore_grip(Cwg *c, CwpAsm *a, u32 mat_poly, u32 mat_rubber, f32 y, f32 z, f32 angle, f32 len) {
  CWG_SCOPE_BEGIN(c);
  if (len <= 0.0f)
    len = 0.062f;
  if (angle == 0.0f)
    angle = 0.25f;
  PgMesh *core = cwg_mesh(c);
  for (u32 i = 0; i < 5; i++) {
    f32 t = (f32)i / 4.0f;
    PgMesh *g = cwg_blob(c, 0.026f - t * 0.003f, len / 5.0f + 0.003f, 0.03f - t * 0.004f, 0.005f, 3);
    cwg_translate(g, 0.0f, -t * len, t * 0.008f);
    pg_mesh_append(core, g);
  }
  cwg_add(c, a, core, mat_poly, (CwgT){.y = y, .z = z, .rx = angle});
  PgMesh *grips = cwg_mesh(c);
  for (u32 i = 0; i < 4; i++) {
    f32 t = 0.15f + (f32)i * 0.23f;
    PgMesh *gr = cwg_box(c, 0.024f, 0.006f, 0.0055f, 0.002f, 2);
    cwg_translate(gr, 0.0f, -t * len, -0.013f);
    pg_mesh_append(grips, gr);
  }
  cwg_add(c, a, grips, mat_rubber, (CwgT){.y = y, .z = z, .rx = angle});
  CWG_SCOPE_END();
}

// pistol mini reflex: base, tapered walls, hood, emitter, screws, canted window in a frame
CwpOptic cwg_build_mini_reflex(Cwg *c, CwpAsm *a, const CwgReflexOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 w = o->w > 0.0f ? o->w : 0.0246f;
  f32 h = o->h > 0.0f ? o->h : 0.021f;
  f32 len = o->len > 0.0f ? o->len : 0.0455f;
  f32 y = o->y, z = o->z;
  u32 mat_body = o->mat_body;
  f32 glass_tilt = o->tilt != 0.0f ? o->tilt : 0.16f;

  cwg_add(c, a, cwg_ext_rr(c, w, len, 0.003f, 3, 0.0042f, 0.0007f), mat_body, (CwgT){.y = y + 0.002f, .z = z, .rx = CWG_HALF_PI});
  PgMesh *wall = cwg_ext(c, 0.0036f, 0.0007f, {-len * 0.5f, 0.0f}, {len * 0.42f, 0.0f}, {len * 0.46f, h * 0.52f}, {len * 0.3f, h * 0.86f},
                         {-len * 0.42f, h}, {-len * 0.5f, h * 0.92f});
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    cwg_add(c, a, wall, mat_body, (CwgT){.x = sx * (w * 0.5f - 0.0018f), .y = y + 0.004f, .z = z, .ry = CWG_HALF_PI});
  }
  cwg_add(c, a, cwg_box(c, w, 0.0035f, 0.011f, 0.0008f, 1), mat_body, (CwgT){.y = y + h * 0.98f, .z = z - len * 0.36f});
  cwg_add(c, a, cwg_blob(c, w - 0.007f, 0.0075f, 0.012f, 0.0016f, 2), mat_body, (CwgT){.y = y + 0.0075f, .z = z - len * 0.3f});
  PgMesh *led = cwg_lathe_z(c, 10, {0.0f, 0.0f}, {0.0f, 0.0016f}, {0.0012f, 0.0018f}, {0.0012f, 0.0f});
  cwg_add(c, a, led, CWP_MAT_STEEL_BRIGHT, (CwgT){.y = y + 0.0105f, .z = z - len * 0.28f, .rx = -0.5f});

  cwg_add_screw(c, a, CWP_MAT_STEEL, 0.0f, y + 0.004f, z + len * 0.4f, 0.0026f, 'y', 0.008f);
  cwg_add_screw(c, a, CWP_MAT_STEEL, w * 0.5f - 0.002f, y + h * 0.5f, z + len * 0.28f, 0.0022f, 'x', 0.006f);
  cwg_add_screw(c, a, CWP_MAT_STEEL, 0.0f, y + h * 0.86f, z + len * 0.1f, 0.0022f, 'y', 0.006f);

  f32 glass_w = w - 0.007f, glass_h = h * 0.72f;
  cwg_add(c, a, cwg_ext_rr(c, glass_w, glass_h, 0.0015f, 3, 0.0012f, 0.0003f), CWP_MAT_GLASS,
          (CwgT){.y = y + h * 0.56f, .z = z + len * 0.14f, .rx = glass_tilt});
  PgPoly frame_hole = cwg_rr(c, glass_w - 0.0002f, glass_h - 0.0002f, 0.0014f, 3);
  PgPoly frame_outer = cwg_rr(c, glass_w + 0.0028f, glass_h + 0.0028f, 0.0018f, 3);
  PgMesh *frame = cwg_extrude(c, frame_outer.pts, frame_outer.count, 0.0022f, 0.0005f, 1, CWG_HOLES(frame_hole));
  cwg_add(c, a, frame, mat_body, (CwgT){.y = y + h * 0.56f, .z = z + len * 0.14f, .rx = glass_tilt});
  CWG_SCOPE_END();
  return (CwpOptic){.valid = true, .center = {0.0f, y + h * 0.56f, z + len * 0.14f}, .lens_z = z + len * 0.14f,
                    .aperture_r = m_minf(glass_w, glass_h) * 0.46f};
}

// pistol slide: body, rib, nose bevel, serrations, flank cuts, ejection port, breech face and iron sights
CwgSlideOut cwg_build_slide(Cwg *c, CwpAsm *a, const CwgSlideOpts *o) {
  CWG_SCOPE_BEGIN(c);
  f32 w = o->w > 0.0f ? o->w : 0.0262f;
  f32 h = o->h > 0.0f ? o->h : 0.0248f;
  f32 len = o->len > 0.0f ? o->len : 0.183f;
  u32 mat = o->mat;
  f32 z_rear = o->z_rear != 0.0f ? o->z_rear : 0.052f;
  f32 z_front = z_rear - len;
  f32 cz = (z_rear + z_front) * 0.5f;
  f32 bore = 0.0f;

  cwg_add(c, a, cwg_box(c, w, h, len, 0.0016f, 2), mat, (CwgT){.y = bore + 0.0015f, .z = cz});
  cwg_add(c, a, cwg_box(c, w - 0.008f, 0.004f, len - 0.02f, 0.0012f, 2), mat, (CwgT){.y = bore + h * 0.5f + 0.0025f, .z = cz - 0.004f});
  PgMesh *nose = cwg_ext(c, 0.016f, 0.0012f, {-w * 0.5f, -h * 0.5f}, {w * 0.5f, -h * 0.5f}, {w * 0.5f, h * 0.34f}, {w * 0.36f, h * 0.5f},
                         {-w * 0.36f, h * 0.5f}, {-w * 0.5f, h * 0.34f});
  cwg_add(c, a, nose, mat, (CwgT){.y = bore + 0.0015f, .z = z_front + 0.008f});

  PgMesh *serration = cwg_box(c, w + 0.0006f, h * 0.62f, 0.0026f, 0.0006f, 1);
  for (u32 k = 0; k < 2; k++) {
    f32 z0 = k == 0 ? z_rear - 0.006f : z_front + 0.03f;
    u32 count = k == 0 ? 7u : 5u;
    for (u32 i = 0; i < count; i++)
      cwg_add(c, a, serration, mat, (CwgT){.y = bore + 0.0015f, .z = z0 - (f32)i * 0.0052f});
  }
  PgMesh *cut = cwg_ext_rr(c, 0.042f, h * 0.4f, 0.004f, 3, 0.0016f, 0.0005f);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    cwg_add(c, a, cut, mat, (CwgT){.x = sx * (w * 0.5f - 0.0004f), .y = bore + 0.001f, .z = cz - 0.012f, .ry = CWG_HALF_PI});
  }

  f32 port_w = 0.036f, port_h = 0.0135f;
  cwg_add(c, a, cwg_box(c, 0.01f, port_h, port_w, 0.0008f, 1), CWP_MAT_CAVITY,
          (CwgT){.x = w * 0.5f - 0.006f, .y = bore + 0.004f, .z = z_rear - 0.05f, .ry = CWG_HALF_PI});
  PgPoly lip_hole = cwg_rr(c, port_w, port_h, 0.0016f, 3);
  PgPoly lip_outer = cwg_rr(c, port_w + 0.004f, port_h + 0.004f, 0.002f, 3);
  PgMesh *lip = cwg_extrude(c, lip_outer.pts, lip_outer.count, 0.002f, 0.0005f, 1, CWG_HOLES(lip_hole));
  cwg_add(c, a, lip, mat, (CwgT){.x = w * 0.5f - 0.0009f, .y = bore + 0.004f, .z = z_rear - 0.05f, .ry = CWG_HALF_PI});

  cwg_add(c, a, cwg_box(c, w - 0.006f, h - 0.008f, 0.004f, 0.0008f, 1), CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore + 0.001f, .z = z_rear - 0.032f});

  PgMesh *rear = cwg_ext(c, 0.0055f, 0.0004f, {-0.009f, 0.0f}, {0.009f, 0.0f}, {0.009f, 0.0055f}, {0.0022f, 0.0055f}, {0.0022f, 0.0022f},
                         {-0.0022f, 0.0022f}, {-0.0022f, 0.0055f}, {-0.009f, 0.0055f});
  cwg_add(c, a, rear, CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore + h * 0.5f + 0.0045f, .z = z_rear - 0.012f});
  PgMesh *dot = cwg_dome(c, 0.0011f, 8, 0.5f);
  for (u32 s = 0; s < 2; s++) {
    f32 sx = s == 0 ? -1.0f : 1.0f;
    cwg_add(c, a, dot, CWP_MAT_STEEL_BRIGHT, (CwgT){.x = sx * 0.0055f, .y = bore + h * 0.5f + 0.0075f, .z = z_rear - 0.0148f, .ry = M_PI});
  }
  cwg_add(c, a, cwg_box(c, 0.0035f, 0.0062f, 0.0042f, 0.0004f, 1), CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore + h * 0.5f + 0.0055f, .z = z_front + 0.014f});
  cwg_add(c, a, cwg_dome(c, 0.0013f, 8, 0.5f), CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore + h * 0.5f + 0.0058f, .z = z_front + 0.0118f, .ry = M_PI});
  CWG_SCOPE_END();
  return (CwgSlideOut){z_rear, z_front, w, h, len, bore + h * 0.5f + 0.0065f};
}
