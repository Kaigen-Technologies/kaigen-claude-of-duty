// Claude of Duty weapons: the rifle, SMG and pistol assembled from the parts and their build.
#include "cod_weapons_geo.h"

#define CWG_HALF_PI_M (0.5f * M_PI)

hz_internal v3 cwg_v(f32 x, f32 y, f32 z) { return (v3){x, y, z}; }

hz_internal CwpNode cwg_node(v3 pos, v3 rot) { return (CwpNode){pos, rot}; }

// ------------------------------------------------------------------ rifle ---

hz_internal void cwg_build_rifle(Cwg *c, CwpModel *m) {
  f32 bore = 0.075f, r_upper = 0.0192f, rail_top = bore + 0.0286f;
  f32 z_upper_rear = 0.055f, z_upper_front = -0.143f, port_z = -0.052f;
  f32 mag_z = -0.058f, mag_tilt = 0.08f;
  f32 hg_z0 = -0.145f, hg_z1 = -0.385f, hg_r = 0.0235f;
  f32 z_breech = -0.1f, z_barrel_end = -0.44f;
  f32 optic_y = bore + 0.067f, optic_z = -0.022f;
  CwpAsm *body = &m->body;

  cwg_add_upper_receiver(c, body, CWP_MAT_ALU, CWP_MAT_STEEL, CWP_MAT_CAVITY,
                         &(CwgUpperOpts){.z_rear = z_upper_rear, .z_front = z_upper_front, .bore = bore, .r = r_upper, .port_z = port_z,
                                         .rail_top = rail_top});
  cwg_add_lower_receiver(c, body, CWP_MAT_ALU, CWP_MAT_STEEL,
                         &(CwgLowerOpts){.bore = bore, .z_rear = z_upper_rear + 0.004f, .z_front = -0.088f, .w = 0.0245f, .mag_w = 0.0292f,
                                         .mag_d = 0.0672f, .mag_top = 0.049f, .mag_bottom = 0.008f, .mag_z = mag_z, .mag_tilt = mag_tilt,
                                         .trigger_z = -0.012f, .grip_angle = 0.38f});

  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *paddle = cwg_ext(c, 0.0042f, 0.0007f, {-0.012f, -0.0035f}, {0.012f, -0.0045f}, {0.014f, 0.0035f}, {-0.012f, 0.0045f});
    cwg_add(c, body, paddle, CWP_MAT_STEEL, (CwgT){.x = -0.0135f, .y = 0.0545f, .z = -0.018f, .ry = CWG_HALF_PI_M});
    cwg_add(c, body, cwg_blob(c, 0.006f, 0.011f, 0.014f, 0.0018f, 2), CWP_MAT_ALU, (CwgT){.x = -0.0128f, .y = 0.0555f, .z = -0.0085f});
    cwg_add(c, body, cwg_blob(c, 0.0075f, 0.016f, 0.019f, 0.0022f, 2), CWP_MAT_ALU, (CwgT){.x = 0.0132f, .y = 0.0505f, .z = -0.0295f});
    PgMesh *button = cwg_lathe_z(c, 14, {0.0f, 0.0f}, {0.0f, 0.0048f}, {0.0016f, 0.0052f}, {0.0042f, 0.0052f}, {0.0042f, 0.0f});
    cwg_add(c, body, button, CWP_MAT_STEEL, (CwgT){.x = 0.0158f, .y = 0.0505f, .z = -0.0295f, .ry = CWG_HALF_PI_M});
    CWG_SCOPE_END();
  }
  cwg_add_pin(c, body, CWP_MAT_STEEL, 0.0f, 0.0555f, -0.083f, 0.0028f, 0.0252f);
  cwg_add_pin(c, body, CWP_MAT_STEEL, 0.0f, 0.0555f, 0.0455f, 0.0028f, 0.0252f);
  cwg_add_rollmark(c, body, CWP_MAT_CAVITY, &(CwgRollmark){.x = -0.0149f, .y = 0.0355f, .z = -0.031f, .h = 0.0036f});
  cwg_add_rollmark(c, body, CWP_MAT_CAVITY,
                   &(CwgRollmark){.x = -0.0149f, .y = 0.0272f, .z = -0.033f, .h = 0.0024f, .pitch = 0.0014f,
                                  .pattern = (const u32[]){2, 3, 1, 0, 2, 2, 3, 0, 3, 2}, .pattern_len = 10});

  cwg_add_barrel(c, body, CWP_MAT_STEEL, CWP_MAT_CAVITY,
                 &(CwgBarrelOpts){.y = bore, .z_breech = z_breech, .z_muzzle = z_barrel_end, .r_chamber = 0.0112f, .r_barrel = 0.0077f,
                                  .r_gas = 0.0098f, .gas_at = -0.3f});
  cwg_add_gas_block(c, body, CWP_MAT_STEEL_SOOT,
                    &(CwgGasOpts){.y = bore, .z = -0.3f, .r_barrel = 0.0077f, .tube_to = -0.15f, .w = 0.021f, .h = 0.0195f});
  CwgMuzzleOut muzzle = cwg_add_muzzle_device(c, body, CWP_MAT_STEEL_SOOT, CWP_MAT_CAVITY, CWG_MUZZLE_BRAKE, z_barrel_end, 0.0077f, bore);

  f32 hand_z = -0.235f;
  cwg_add_handguard(c, body, CWP_MAT_ALU,
                    &(CwgHandguardOpts){.mat_panel = CWP_MAT_POLYMER, .y = bore, .z0 = hg_z0, .z1 = hg_z1, .r = hg_r, .sides = 8,
                                        .slat_w = 0.0166f, .slat_t = 0.0036f, .slots = 4, .braces = 3, .has_top = true,
                                        .top_from = hand_z + 0.048f, .top_to = hg_z1 + 0.056f});
  cwg_add_rail(c, body, CWP_MAT_ALU, hg_z1 + 0.004f, hg_z0 - 0.002f, rail_top, 0.0f, NULL);
  cwg_add_qd_socket(c, body, CWP_MAT_ALU, CWP_MAT_STEEL, -hg_r + 0.001f, bore - 0.008f, hg_z0 - 0.035f, 'x', 0.005f);
  cwg_add_sling_loop(c, body, CWP_MAT_STEEL, 0.0f, bore - hg_r - 0.0015f, hg_z1 + 0.03f, 0.0075f,
                     (CwgT){.rx = CWG_HALF_PI_M, .ry = CWG_HALF_PI_M});

  cwg_add_pistol_grip(c, body, CWP_MAT_POLYMER, CWP_MAT_RUBBER, &(CwgGripOpts){.y = 0.035f, .z = 0.015f, .angle = 0.38f, .len = 0.108f, .w = 0.031f});
  cwg_add_carbine_stock(c, body, CWP_MAT_ALU, CWP_MAT_POLYMER, CWP_MAT_RUBBER,
                        &(CwgStockOpts){.bore = bore, .z_front = z_upper_rear + 0.003f, .z_rear = 0.245f, .y = bore - 0.012f});

  CwpOptic optic = cwg_build_optic(c, body,
                                   &(CwgOpticOpts){.r_tube = 0.0155f, .len = 0.052f, .hood = 0.007f, .y = optic_y, .z = optic_z,
                                                   .rail_top = rail_top, .mat_body = CWP_MAT_ALU_FINE, .mat_steel = CWP_MAT_STEEL});
  cwg_add_front_sight(c, body, CWP_MAT_POLYMER, CWP_MAT_ALU, 0.0f, rail_top, -0.358f, false);
  cwg_add_rear_sight(c, body, CWP_MAT_POLYMER, CWP_MAT_ALU, 0.0f, rail_top, -0.112f, false);

  CwgMag mag = cwg_build_magazine(c, &m->part[CWP_PART_MAGAZINE],
                                  &(CwgMagOpts){.w = 0.0255f, .d = 0.0655f, .len = 0.212f, .curve = 0.03f, .segs = 8, .witness = 4,
                                                .poly = CWP_MAT_POLYMER});
  m->has_part[CWP_PART_MAGAZINE] = true;

  {
    CWG_SCOPE_BEGIN(c);
    cwg_add(c, &m->part[CWP_PART_CHARGING], cwg_charging_handle(c), CWP_MAT_ALU, (CwgT){0});
    m->has_part[CWP_PART_CHARGING] = true;
    CwpAsm *bolt = &m->part[CWP_PART_BOLT];
    cwg_add_bolt_carrier(c, bolt, CWP_MAT_STEEL_BRIGHT, 0.0f, 0.0152f, 0.092f, 0.0f);
    CwgCartridge round = cwg_cartridge(c, 0.0446f, 0.00495f, 0.019f);
    cwg_add(c, bolt, round.brass, CWP_MAT_BRASS, (CwgT){.z = -0.09f, .ry = M_PI});
    m->has_part[CWP_PART_BOLT] = true;
    cwg_add(c, &m->part[CWP_PART_TRIGGER], cwg_trigger_part(c), CWP_MAT_STEEL_BRIGHT, (CwgT){0});
    m->has_part[CWP_PART_TRIGGER] = true;
    PgMesh *sel = cwg_selector_part(c, 0.006f);
    CwpAsm *selector = &m->part[CWP_PART_SELECTOR];
    cwg_add(c, selector, sel, CWP_MAT_ALU, (CwgT){0});
    cwg_add(c, selector, sel, CWP_MAT_ALU, (CwgT){.sx = -1.0f});
    m->has_part[CWP_PART_SELECTOR] = true;
    CWG_SCOPE_END();
  }

  m->muzzle = cwg_v(0.0f, bore, muzzle.crown_z);
  m->chamber = cwg_v(0.0f, bore, port_z);
  m->eject = cwg_v(r_upper + 0.008f, bore + 0.003f, port_z);
  m->eject_dir = v3_normalize(cwg_v(0.86f, 0.44f, 0.26f));
  m->sight = cwg_v(0.0f, optic_y, optic.lens_z);
  m->sight_axis = cwg_v(0.0f, 0.0f, -1.0f);
  m->iron_sight = cwg_v(0.0f, rail_top + 0.026f, 0.038f);
  m->grip_r = (CwpGrip){cwg_v(0.0251f, 0.06f, 0.1223f), cwg_v(0.05f, -0.55f, -0.833f), cwg_v(1.0f, 0.03f, 0.04f)};
  m->grip_l = (CwpGrip){cwg_v(-0.1f, 0.0734f, hand_z + 0.0252f), cwg_v(0.8977f, -0.3267f, -0.2955f), cwg_v(-0.2784f, -0.7648f, 0.581f)};
  m->handguard = (CwpHandguard){.valid = true, .axis = cwg_v(0.0f, bore, 0.0f), .dir = cwg_v(0.0f, 0.0f, 1.0f), .r = hg_r + 0.0036f, .z0 = hg_z0, .z1 = hg_z1};
  m->mag_seat = cwg_node(cwg_v(0.0f, 0.061f, mag_z), cwg_v(mag_tilt, 0.0f, 0.0f));
  m->mag_drop = cwg_v(0.0f, -0.4f, 0.02f);
  m->charge_rest = cwg_node(cwg_v(0.0f, bore + r_upper - 0.0075f, z_upper_rear - 0.024f), V3_ZERO);
  m->charge_pull = cwg_v(0.0f, 0.0f, 0.082f);
  m->bolt_rest = cwg_node(cwg_v(0.0f, bore, 0.021f), V3_ZERO);
  m->bolt_travel = cwg_v(0.0f, 0.0f, 0.062f);
  m->trigger_pivot = cwg_node(cwg_v(0.0f, 0.0455f, -0.0055f), V3_ZERO);
  m->trigger_pull = -0.34f;
  m->selector_pivot = cwg_node(cwg_v(0.0f, 0.0525f, 0.0205f), V3_ZERO);
  m->optic = optic;
  m->case_len = 0.0446f;
  m->rim_r = 0.00495f;
  m->mag_len = mag.len;
  m->mag_w = mag.w;
  m->mag_d = mag.d;
}

// -------------------------------------------------------------------- smg ---

hz_internal void cwg_build_smg(Cwg *c, CwpModel *m) {
  f32 bore = 0.068f, r_rec = 0.0158f, rail_top = bore + 0.0245f;
  f32 z_rec_rear = 0.062f, z_rec_front = -0.112f, port_z = -0.042f;
  f32 mag_z = -0.052f, mag_tilt = 0.05f;
  f32 hg_z0 = -0.114f, hg_z1 = -0.268f, hg_r = 0.019f;
  f32 z_barrel_end = -0.3f;
  f32 optic_y = bore + 0.055f, optic_z = -0.008f;
  CwpAsm *body = &m->body;
  f32 rec_len = z_rec_rear - z_rec_front;

  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *rec = cwg_lathe_z(c, 22, {0.0f, r_rec * 0.55f}, {0.0f, r_rec * 0.99f}, {0.002f, r_rec}, {rec_len - 0.004f, r_rec},
                              {rec_len - 0.002f, r_rec * 0.96f}, {rec_len, r_rec * 0.6f});
    cwg_add(c, body, rec, CWP_MAT_ALU, (CwgT){.y = bore, .z = z_rec_rear, .ry = M_PI});
    cwg_add(c, body, cwg_box(c, 0.0225f, 0.009f, rec_len - 0.004f, 0.0009f, 1), CWP_MAT_ALU,
            (CwgT){.y = bore + r_rec - 0.003f, .z = (z_rec_rear + z_rec_front) * 0.5f});
    CWG_SCOPE_END();
  }
  cwg_add_rail(c, body, CWP_MAT_ALU, z_rec_front + 0.004f, z_rec_rear - 0.004f, rail_top, 0.0f, NULL);

  f32 mag_w = 0.0242f, mag_d = 0.0345f, well_h = 0.036f;
  {
    CWG_SCOPE_BEGIN(c);
    cwg_add(c, body, cwg_tube_z(c, 0.0072f, 0.0052f, 0.14f, 14, 0.0004f), CWP_MAT_ALU, (CwgT){.x = -r_rec + 0.0028f, .y = bore + r_rec - 0.007f, .z = -0.06f});
    f32 port_w = 0.03f, port_h = 0.017f;
    cwg_add(c, body, cwg_box(c, 0.01f, port_h, port_w, 0.0008f, 1), CWP_MAT_CAVITY,
            (CwgT){.x = r_rec - 0.006f, .y = bore + 0.002f, .z = port_z, .ry = CWG_HALF_PI_M});
    PgPoly lip_hole = cwg_rr(c, port_w, port_h, 0.0016f, 3);
    PgPoly lip_outer = cwg_rr(c, port_w + 0.004f, port_h + 0.004f, 0.002f, 3);
    PgMesh *lip = cwg_extrude(c, lip_outer.pts, lip_outer.count, 0.002f, 0.0005f, 1, CWG_HOLES(lip_hole));
    cwg_add(c, body, lip, CWP_MAT_ALU, (CwgT){.x = r_rec - 0.0012f, .y = bore + 0.002f, .z = port_z, .ry = CWG_HALF_PI_M});
    PgMesh *carrier = cwg_lathe_z(c, 16, {0.0f, r_rec * 0.5f}, {0.0f, r_rec * 0.82f}, {0.07f, r_rec * 0.82f}, {0.07f, r_rec * 0.5f});
    cwg_add(c, body, carrier, CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore, .z = port_z - 0.02f});

    cwg_add(c, body, cwg_box(c, 0.0245f, 0.028f, 0.13f, 0.0016f, 2), CWP_MAT_POLYMER, (CwgT){.y = bore - 0.0195f, .z = -0.02f});

    PgPoly well_hole = cwg_rr(c, mag_w - 0.002f, mag_d - 0.002f, 0.004f, 4);
    PgPoly well_outer = cwg_rr(c, mag_w + 0.003f, mag_d + 0.003f, 0.005f, 4);
    PgMesh *well = cwg_extrude(c, well_outer.pts, well_outer.count, well_h, 0.0011f, 1, CWG_HOLES(well_hole));
    cwg_add(c, body, well, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.038f, .z = mag_z, .rx = CWG_HALF_PI_M + mag_tilt});
    PgPoly liner_hole = cwg_rr(c, mag_w - 0.005f, mag_d - 0.005f, 0.003f, 4);
    PgPoly liner_outer = cwg_rr(c, mag_w - 0.0022f, mag_d - 0.0022f, 0.004f, 4);
    PgMesh *liner = cwg_extrude(c, liner_outer.pts, liner_outer.count, well_h - 0.004f, 0.0005f, 1, CWG_HOLES(liner_hole));
    cwg_add(c, body, liner, CWP_MAT_CAVITY, (CwgT){.y = bore - 0.038f, .z = mag_z, .rx = CWG_HALF_PI_M + mag_tilt});
    PgPoly flare_hole = cwg_rr(c, mag_w + 0.001f, mag_d + 0.001f, 0.004f, 4);
    PgPoly flare_outer = cwg_rr(c, mag_w + 0.007f, mag_d + 0.008f, 0.006f, 4);
    PgMesh *flare = cwg_extrude(c, flare_outer.pts, flare_outer.count, 0.007f, 0.0012f, 1, CWG_HOLES(flare_hole));
    cwg_add(c, body, flare, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.055f, .z = mag_z + 0.0016f, .rx = CWG_HALF_PI_M + mag_tilt});

    PgMesh *guard = cwg_extrude(c, CWG_PTS({-0.026f, 0.0f}, {0.028f, 0.0f}, {0.03f, -0.006f}, {0.026f, -0.021f}, {0.016f, -0.026f},
                                           {-0.018f, -0.026f}, {-0.026f, -0.02f}),
                                0.0155f, 0.0009f, 1,
                                CWG_HOLES(CWG_POLY({-0.021f, -0.003f}, {0.0225f, -0.003f}, {0.0235f, -0.008f}, {0.02f, -0.0195f},
                                                   {0.013f, -0.0225f}, {-0.015f, -0.0225f}, {-0.0205f, -0.018f})));
    cwg_add(c, body, guard, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.03f, .z = -0.008f});

    PgMesh *paddle = cwg_ext(c, 0.004f, 0.0006f, {-0.008f, -0.004f}, {0.009f, -0.005f}, {0.01f, 0.004f}, {-0.008f, 0.005f});
    for (u32 s = 0; s < 2; s++) {
      f32 sx = s == 0 ? -1.0f : 1.0f;
      cwg_add(c, body, paddle, CWP_MAT_ALU, (CwgT){.x = sx * 0.0132f, .y = bore - 0.026f, .z = -0.03f, .ry = CWG_HALF_PI_M});
    }
    CWG_SCOPE_END();
  }

  cwg_add_pistol_grip(c, body, CWP_MAT_POLYMER, CWP_MAT_RUBBER, &(CwgGripOpts){.y = 0.033f, .z = 0.018f, .angle = 0.36f, .len = 0.102f, .w = 0.03f});

  cwg_add_barrel(c, body, CWP_MAT_STEEL, CWP_MAT_CAVITY,
                 &(CwgBarrelOpts){.y = bore, .z_breech = -0.09f, .z_muzzle = z_barrel_end, .r_chamber = 0.0092f, .r_barrel = 0.0062f,
                                  .r_gas = 0.0072f, .gas_at = -0.2f, .no_knurl = true});
  CwgMuzzleOut muzzle = cwg_add_muzzle_device(c, body, CWP_MAT_STEEL_SOOT, CWP_MAT_CAVITY, CWG_MUZZLE_TRILUG, z_barrel_end, 0.0062f, bore);
  cwg_add_handguard(c, body, CWP_MAT_ALU,
                    &(CwgHandguardOpts){.mat_panel = CWP_MAT_ALU, .y = bore, .z0 = hg_z0, .z1 = hg_z1, .r = hg_r, .sides = 8, .slat_w = 0.0132f,
                                        .slat_t = 0.0032f, .slots = 3, .braces = 2});
  cwg_add_rail(c, body, CWP_MAT_ALU, hg_z1 + 0.004f, hg_z0 - 0.002f, rail_top, 0.0f, NULL);
  cwg_add_fore_grip(c, body, CWP_MAT_POLYMER, CWP_MAT_RUBBER, bore - hg_r - 0.004f, -0.208f, 0.2f, 0.058f);
  cwg_add_qd_socket(c, body, CWP_MAT_ALU, CWP_MAT_STEEL, -hg_r + 0.001f, bore - 0.006f, hg_z0 - 0.022f, 'x', 0.0045f);

  {
    CWG_SCOPE_BEGIN(c);
    cwg_add(c, body, cwg_blob(c, 0.026f, 0.03f, 0.024f, 0.003f, 3), CWP_MAT_ALU, (CwgT){.y = bore - 0.008f, .z = z_rec_rear + 0.008f});
    CWG_SCOPE_END();
  }
  cwg_add_pin(c, body, CWP_MAT_STEEL, 0.0f, bore - 0.008f, z_rec_rear + 0.014f, 0.003f, 0.028f);
  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *strut = cwg_box(c, 0.0075f, 0.011f, 0.145f, 0.0018f, 2);
    for (u32 s = 0; s < 2; s++) {
      f32 sx = s == 0 ? -1.0f : 1.0f;
      cwg_add(c, body, strut, CWP_MAT_ALU, (CwgT){.x = sx * 0.0125f, .y = bore - 0.014f, .z = z_rec_rear + 0.085f, .rx = -0.045f});
    }
    cwg_add(c, body, cwg_box(c, 0.032f, 0.009f, 0.0095f, 0.0016f, 2), CWP_MAT_ALU, (CwgT){.y = bore - 0.019f, .z = z_rec_rear + 0.12f});
    cwg_add(c, body, cwg_ext_rr(c, 0.042f, 0.058f, 0.006f, 4, 0.009f, 0.0012f), CWP_MAT_POLYMER,
            (CwgT){.y = bore - 0.026f, .z = z_rec_rear + 0.155f, .rx = 0.06f});
    cwg_add(c, body, cwg_blob(c, 0.04f, 0.05f, 0.0085f, 0.0035f, 3), CWP_MAT_RUBBER, (CwgT){.y = bore - 0.026f, .z = z_rec_rear + 0.162f, .rx = 0.06f});
    cwg_add(c, body, cwg_blob(c, 0.019f, 0.013f, 0.09f, 0.005f, 3), CWP_MAT_POLYMER, (CwgT){.y = bore + 0.012f, .z = z_rec_rear + 0.08f, .rx = -0.05f});
    CWG_SCOPE_END();
  }
  cwg_add_sling_loop(c, body, CWP_MAT_STEEL, 0.0165f, bore - 0.022f, z_rec_rear + 0.026f, 0.007f, (CwgT){.ry = CWG_HALF_PI_M});

  CwpOptic optic = cwg_build_optic(c, body,
                                   &(CwgOpticOpts){.r_tube = 0.0138f, .len = 0.044f, .hood = 0.006f, .y = optic_y, .z = optic_z,
                                                   .rail_top = rail_top, .mat_body = CWP_MAT_ALU_FINE, .mat_steel = CWP_MAT_STEEL});
  cwg_add_front_sight(c, body, CWP_MAT_POLYMER, CWP_MAT_ALU, 0.0f, rail_top, -0.248f, false);
  cwg_add_rear_sight(c, body, CWP_MAT_POLYMER, CWP_MAT_ALU, 0.0f, rail_top, -0.09f, false);

  CwgMag mag = cwg_build_magazine(c, &m->part[CWP_PART_MAGAZINE],
                                  &(CwgMagOpts){.w = 0.0235f, .d = 0.0335f, .len = 0.192f, .curve = 0.026f, .segs = 7, .witness = 5,
                                                .case_len = 0.0192f, .rim_r = 0.00478f, .bullet_len = 0.0132f, .poly = CWP_MAT_POLYMER});
  m->has_part[CWP_PART_MAGAZINE] = true;

  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *ch = cwg_mesh(c);
    pg_mesh_append(ch, cwg_rod_z(c, 0.0048f, 0.0048f, 0.12f, 12, 0.0004f));
    PgMesh *paddle = cwg_ext(c, 0.0055f, 0.0008f, {0.0f, -0.0075f}, {0.017f, -0.009f}, {0.019f, 0.0f}, {0.017f, 0.008f}, {0.0f, 0.007f});
    cwg_rotate_y(paddle, -CWG_HALF_PI_M);
    cwg_translate(paddle, -0.0075f, 0.0f, -0.05f);
    pg_mesh_append(ch, paddle);
    PgMesh *knob = cwg_dome(c, 0.0055f, 12, 0.6f);
    cwg_rotate_y(knob, -CWG_HALF_PI_M);
    cwg_translate(knob, -0.024f, 0.0f, -0.05f);
    pg_mesh_append(ch, knob);
    cwg_add(c, &m->part[CWP_PART_CHARGING], ch, CWP_MAT_STEEL_BRIGHT, (CwgT){0});
    m->has_part[CWP_PART_CHARGING] = true;

    CwpAsm *bolt = &m->part[CWP_PART_BOLT];
    PgMesh *bolt_body = cwg_lathe_z(c, 16, {0.0f, r_rec * 0.45f}, {0.0f, r_rec * 0.8f}, {0.078f, r_rec * 0.8f}, {0.078f, r_rec * 0.45f});
    cwg_add(c, bolt, bolt_body, CWP_MAT_STEEL_BRIGHT, (CwgT){.z = -0.078f});
    cwg_add(c, bolt, cwg_box(c, 0.014f, 0.014f, 0.003f, 0.0006f, 1), CWP_MAT_STEEL, (CwgT){.z = -0.0005f});
    CwgCartridge round = cwg_cartridge(c, 0.0192f, 0.00478f, 0.0132f);
    cwg_add(c, bolt, round.brass, CWP_MAT_BRASS, (CwgT){.z = -0.0215f, .ry = M_PI});
    m->has_part[CWP_PART_BOLT] = true;

    cwg_add(c, &m->part[CWP_PART_TRIGGER], cwg_trigger_part(c), CWP_MAT_STEEL_BRIGHT, (CwgT){0});
    m->has_part[CWP_PART_TRIGGER] = true;
    PgMesh *sel = cwg_selector_part(c, 0.006f);
    CwpAsm *selector = &m->part[CWP_PART_SELECTOR];
    cwg_add(c, selector, sel, CWP_MAT_ALU, (CwgT){0});
    cwg_add(c, selector, sel, CWP_MAT_ALU, (CwgT){.sx = -1.0f});
    m->has_part[CWP_PART_SELECTOR] = true;
    CWG_SCOPE_END();
  }

  m->muzzle = cwg_v(0.0f, bore, muzzle.crown_z);
  m->chamber = cwg_v(0.0f, bore, port_z);
  m->eject = cwg_v(r_rec + 0.006f, bore + 0.002f, port_z);
  m->eject_dir = v3_normalize(cwg_v(0.9f, 0.4f, 0.18f));
  m->sight = cwg_v(0.0f, optic_y, optic.lens_z);
  m->sight_axis = cwg_v(0.0f, 0.0f, -1.0f);
  m->iron_sight = cwg_v(0.0f, rail_top + 0.024f, 0.042f);
  m->grip_r = (CwpGrip){cwg_v(0.024f, 0.028f, 0.064f), cwg_v(-0.05f, -0.4f, -0.915f), cwg_v(0.97f, -0.05f, -0.22f)};
  m->grip_l = (CwpGrip){cwg_v(-0.056f, 0.015f, -0.153f), cwg_v(0.45f, 0.05f, -0.89f), cwg_v(-0.88f, -0.05f, -0.45f)};
  m->handguard = (CwpHandguard){0};
  m->mag_seat = cwg_node(cwg_v(0.0f, bore - 0.02f, mag_z), cwg_v(mag_tilt, 0.0f, 0.0f));
  m->mag_drop = cwg_v(0.0f, -0.4f, 0.02f);
  m->charge_rest = cwg_node(cwg_v(-r_rec + 0.0028f, bore + r_rec - 0.007f, -0.06f), V3_ZERO);
  m->charge_pull = cwg_v(0.0f, 0.0f, 0.062f);
  m->bolt_rest = cwg_node(cwg_v(0.0f, bore, port_z + 0.032f), V3_ZERO);
  m->bolt_travel = cwg_v(0.0f, 0.0f, 0.05f);
  m->trigger_pivot = cwg_node(cwg_v(0.0f, bore - 0.026f, -0.001f), V3_ZERO);
  m->trigger_pull = -0.36f;
  m->selector_pivot = cwg_node(cwg_v(0.0f, bore - 0.019f, 0.022f), V3_ZERO);
  m->optic = optic;
  m->case_len = 0.0192f;
  m->rim_r = 0.00478f;
  m->mag_len = mag.len;
  m->mag_w = mag.w;
  m->mag_d = mag.d;
}

// ----------------------------------------------------------------- pistol ---

hz_internal void cwg_build_pistol(Cwg *c, CwpModel *m) {
  f32 bore = 0.036f, slide_h = 0.0248f, slide_w = 0.0262f, slide_len = 0.183f;
  f32 z_slide_rear = 0.052f, z_slide_front = z_slide_rear - slide_len;
  f32 grip_angle = 0.32f;
  CwpAsm *body = &m->body;

  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *dust = cwg_ext(c, 0.108f, 0.001f, {-slide_w * 0.5f + 0.001f, 0.0f}, {slide_w * 0.5f - 0.001f, 0.0f},
                           {slide_w * 0.5f - 0.001f, -0.0125f}, {slide_w * 0.5f - 0.004f, -0.016f}, {-slide_w * 0.5f + 0.004f, -0.016f},
                           {-slide_w * 0.5f + 0.001f, -0.0125f});
    cwg_add(c, body, dust, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.0075f, .z = -0.062f});
    cwg_add(c, body, cwg_blob(c, slide_w - 0.001f, 0.05f, 0.062f, 0.004f, 3), CWP_MAT_POLYMER, (CwgT){.y = bore - 0.032f, .z = 0.012f});
    PgMesh *tang = cwg_ext(c, slide_w - 0.003f, 0.0012f, {-0.008f, 0.0f}, {0.03f, -0.004f}, {0.032f, -0.012f}, {-0.008f, -0.014f});
    cwg_add(c, body, tang, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.014f, .z = 0.034f, .ry = CWG_HALF_PI_M});
    CWG_SCOPE_END();
  }
  cwg_add_rail(c, body, CWP_MAT_POLYMER, -0.112f, -0.058f, bore - 0.0175f, 0.0f,
               &(CwgRail){.pic = {.width = 0.0175f, .waist = 0.013f, .base_h = 0.0026f, .top_h = 0.0024f, .pitch = 0.0092f, .slot = 0.0046f}});
  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *guard = cwg_extrude(c, CWG_PTS({-0.024f, 0.0f}, {0.026f, 0.0f}, {0.028f, -0.007f}, {0.024f, -0.022f}, {0.013f, -0.027f},
                                           {-0.016f, -0.027f}, {-0.024f, -0.021f}),
                                slide_w - 0.004f, 0.001f, 1,
                                CWG_HOLES(CWG_POLY({-0.019f, -0.003f}, {0.021f, -0.003f}, {0.0225f, -0.009f}, {0.0185f, -0.0205f},
                                                   {0.01f, -0.0235f}, {-0.013f, -0.0235f}, {-0.019f, -0.0185f})));
    cwg_add(c, body, guard, CWP_MAT_POLYMER, (CwgT){.y = bore - 0.0245f, .z = -0.03f});
    CWG_SCOPE_END();
  }

  cwg_add_pistol_grip(c, body, CWP_MAT_POLYMER, CWP_MAT_RUBBER, &(CwgGripOpts){.y = bore - 0.014f, .z = 0.016f, .angle = grip_angle, .len = 0.113f, .w = 0.0305f});
  {
    CWG_SCOPE_BEGIN(c);
    PgMesh *stipple_src = cwg_box(c, 0.0024f, 0.0024f, 0.0009f, 0.0003f, 1);
    PgMesh *stipple = cwg_mesh(c);
    for (u32 r = 0; r < 9; r++) {
      for (u32 ci = 0; ci < 5; ci++) {
        u32 from = stipple->pos.len;
        pg_mesh_append(stipple, stipple_src);
        pg_mesh_translate(stipple, (v3){-0.005f + (f32)ci * 0.0026f + (f32)(r % 2u) * 0.0013f, -0.012f - (f32)r * 0.0072f, 0.0f}, from);
      }
    }
    for (u32 s = 0; s < 2; s++) {
      f32 sx = s == 0 ? -1.0f : 1.0f;
      cwg_add(c, body, stipple, CWP_MAT_POLYMER,
              (CwgT){.x = sx * 0.0152f, .y = bore - 0.016f, .z = 0.017f, .ry = sx * M_PI * 0.5f, .rz = sx > 0.0f ? -grip_angle : grip_angle});
    }
    PgMesh *button = cwg_lathe_z(c, 12, {0.0f, 0.0f}, {0.0f, 0.0042f}, {0.0015f, 0.0048f}, {0.0038f, 0.0048f}, {0.0038f, 0.0f});
    cwg_add(c, body, button, CWP_MAT_POLYMER, (CwgT){.x = 0.0138f, .y = bore - 0.032f, .z = -0.014f, .ry = CWG_HALF_PI_M});
    PgMesh *stop = cwg_ext(c, 0.0032f, 0.0005f, {-0.014f, -0.0028f}, {0.012f, -0.0035f}, {0.014f, 0.0028f}, {-0.014f, 0.0035f});
    cwg_add(c, body, stop, CWP_MAT_STEEL, (CwgT){.x = -0.0132f, .y = bore - 0.0135f, .z = -0.022f, .ry = CWG_HALF_PI_M});
    cwg_add(c, body, stop, CWP_MAT_STEEL, (CwgT){.x = 0.0132f, .y = bore - 0.0135f, .z = -0.022f, .ry = CWG_HALF_PI_M});
    PgMesh *takedown = cwg_lathe_z(c, 12, {0.0f, 0.0f}, {0.0f, 0.0035f}, {0.0022f, 0.004f}, {0.0022f, 0.0f});
    cwg_add(c, body, takedown, CWP_MAT_STEEL, (CwgT){.x = -0.0138f, .y = bore - 0.0175f, .z = -0.046f, .ry = -CWG_HALF_PI_M});

    PgMesh *barrel = cwg_lathe_z(c, 18, {0.0f, 0.0f}, {0.0f, 0.0082f}, {0.0016f, 0.0088f}, {0.006f, 0.0088f}, {0.0072f, 0.0078f}, {0.0072f, 0.0048f});
    cwg_add(c, body, barrel, CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore, .z = z_slide_front + 0.0012f, .ry = M_PI});
    cwg_add(c, body, cwg_tube_z(c, 0.0048f, 0.0034f, 0.03f, 12, 0.0002f), CWP_MAT_CAVITY, (CwgT){.y = bore, .z = z_slide_front + 0.012f});
    PgMesh *spring = cwg_lathe_z(c, 12, {0.0f, 0.0032f}, {0.0f, 0.0048f}, {0.004f, 0.0048f}, {0.004f, 0.0032f});
    cwg_add(c, body, spring, CWP_MAT_STEEL_BRIGHT, (CwgT){.y = bore - 0.0125f, .z = z_slide_front + 0.0025f});
    CWG_SCOPE_END();
  }

  CwpAsm *slide_asm = &m->part[CWP_PART_SLIDE];
  cwg_build_slide(c, slide_asm, &(CwgSlideOpts){.w = slide_w, .h = slide_h, .len = slide_len, .mat = CWP_MAT_STEEL_BLACK, .z_rear = z_slide_rear});
  CwpOptic reflex = cwg_build_mini_reflex(c, slide_asm,
                                          &(CwgReflexOpts){.w = 0.0246f, .h = 0.021f, .len = 0.0455f, .y = slide_h * 0.5f + 0.0018f,
                                                           .z = z_slide_rear - 0.038f, .mat_body = CWP_MAT_ALU_FINE});
  m->has_part[CWP_PART_SLIDE] = true;
  f32 optic_y = bore + slide_h * 0.5f + 0.0018f + 0.021f * 0.56f;
  f32 optic_z = z_slide_rear - 0.038f + 0.0455f * 0.14f;

  CwgMag mag = cwg_build_magazine(c, &m->part[CWP_PART_MAGAZINE],
                                  &(CwgMagOpts){.w = 0.0212f, .d = 0.0295f, .len = 0.108f, .curve = 0.004f, .segs = 5, .witness = 3,
                                                .case_len = 0.0192f, .rim_r = 0.00478f, .bullet_len = 0.0132f, .poly = CWP_MAT_POLYMER});
  m->has_part[CWP_PART_MAGAZINE] = true;

  {
    CWG_SCOPE_BEGIN(c);
    CwpAsm *trigger = &m->part[CWP_PART_TRIGGER];
    cwg_add(c, trigger, cwg_trigger_part(c), CWP_MAT_POLYMER, (CwgT){0});
    PgMesh *blade = cwg_ext(c, 0.0028f, 0.0004f, {-0.0022f, 0.003f}, {0.0022f, 0.003f}, {0.0022f, -0.016f}, {-0.0022f, -0.017f});
    cwg_add(c, trigger, blade, CWP_MAT_STEEL, (CwgT){.x = 0.0f, .y = -0.001f, .z = 0.0022f});
    m->has_part[CWP_PART_TRIGGER] = true;
    CWG_SCOPE_END();
  }

  m->muzzle = cwg_v(0.0f, bore, z_slide_front - 0.004f);
  m->chamber = cwg_v(0.0f, bore, z_slide_rear - 0.05f);
  m->eject = cwg_v(slide_w * 0.5f + 0.004f, bore + 0.005f, z_slide_rear - 0.05f);
  m->eject_dir = v3_normalize(cwg_v(0.82f, 0.52f, 0.24f));
  m->sight = cwg_v(0.0f, optic_y, optic_z);
  m->sight_axis = cwg_v(0.0f, 0.0f, -1.0f);
  m->iron_sight = cwg_v(0.0f, bore + slide_h * 0.5f + 0.0065f, z_slide_rear - 0.012f);
  m->grip_r = (CwpGrip){cwg_v(0.028f, 0.003f, 0.07f), cwg_v(0.0f, -0.315f, -0.949f), cwg_v(0.98f, 0.0f, -0.2f)};
  m->grip_l = (CwpGrip){cwg_v(-0.03f, -0.012f, 0.076f), cwg_v(0.34f, -0.28f, -0.9f), cwg_v(0.15f, 0.93f, -0.33f)};
  m->handguard = (CwpHandguard){0};
  m->mag_seat = cwg_node(cwg_v(0.0f, bore - 0.03f, 0.019f), cwg_v(-grip_angle, 0.0f, 0.0f));
  m->mag_drop = cwg_v(0.0f, -0.42f, 0.05f);
  m->slide_rest = cwg_node(cwg_v(0.0f, bore, 0.0f), V3_ZERO);
  m->slide_travel = cwg_v(0.0f, 0.0f, 0.0225f);
  m->trigger_pivot = cwg_node(cwg_v(0.0f, bore - 0.0135f, -0.0165f), V3_ZERO);
  m->trigger_pull = -0.3f;
  m->optic = reflex;
  m->case_len = 0.0192f;
  m->rim_r = 0.00478f;
  m->mag_len = mag.len;
  m->mag_w = mag.w;
  m->mag_d = mag.d;
}

// -------------------------------------------------------------- the build ---

// builds one weapon on `arena`: the assemblies, then the per-bucket finish (merge, curvature masks, shaping)
void cwg_build_model(CwpModel *out, u32 weapon, ArenaAllocator *arena) {
  memzero_struct(out);
  out->id = weapon;
  ArenaAllocator *scratch = arena_create(MB(256), MB(2));
  Cwg ctx = {.scratch = scratch, .out = arena};
  Cwg *c = &ctx;
  switch (weapon) {
  case COD_WEAPON_RIFLE: cwg_build_rifle(c, out); break;
  case COD_WEAPON_SMG: cwg_build_smg(c, out); break;
  default: cwg_build_pistol(c, out); break;
  }
  arena_destroy(scratch);
  u32 seed = 0x57e40000u + weapon * 977u;
  u32 tris = 0;
  cwg_asm_finish(&out->body, 1.0f, seed, &tris);
  for (u32 p = 0; p < CWP_PART_COUNT; p++)
    if (out->has_part[p])
      cwg_asm_finish(&out->part[p], p == CWP_PART_MAGAZINE ? 0.8f : 1.0f, seed + (p + 1) * 131u, &tris);
  out->tris = tris;
}

