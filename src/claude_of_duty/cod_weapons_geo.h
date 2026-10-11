// Claude of Duty weapons, gun builders only: the hard-surface kit shared by cod_weapons_geo/parts/models.c.
#pragma once
#include "cod_weapons_internal.h"

// a build context: piece meshes live on `scratch`, assembly buckets on `out`
typedef struct Cwg {
  ArenaAllocator *scratch;
  ArenaAllocator *out;
} Cwg;

// a placement: position, Euler XYZ, scale
typedef struct CwgT {
  f32 x, y, z;
  f32 rx, ry, rz;
  f32 sx, sy, sz;
} CwgT;

#define CWG_BEVEL 0.0008f

// muzzle device kinds
typedef enum CwgMuzzle {
  CWG_MUZZLE_BRAKE = 0,
  CWG_MUZZLE_A2,
  CWG_MUZZLE_COMP,
  CWG_MUZZLE_TRILUG,
} CwgMuzzle;

// results of the part builders the models read back
typedef struct CwgBarrel {
  f32 gas_at;
  f32 r_bore;
} CwgBarrel;

typedef struct CwgMuzzleOut {
  f32 len;
  f32 crown_z;
} CwgMuzzleOut;

typedef struct CwgLower {
  f32 trigger_z;
  f32 mag_z;
} CwgLower;

typedef struct CwgMag {
  f32 len;
  f32 w;
  f32 d;
} CwgMag;

// kit (cod_weapons_geo.c)
PgMesh *cwg_mesh(Cwg *c);
PgMesh *cwg_clone(Cwg *c, const PgMesh *src);
PgMesh *cwg_merge(Cwg *c, PgMesh *const *list, u32 count);
void cwg_translate(PgMesh *m, f32 x, f32 y, f32 z);
void cwg_rotate_x(PgMesh *m, f32 a);
void cwg_rotate_y(PgMesh *m, f32 a);
void cwg_rotate_z(PgMesh *m, f32 a);
void cwg_scale(PgMesh *m, f32 x, f32 y, f32 z);
PgMesh *cwg_box(Cwg *c, f32 w, f32 h, f32 d, f32 chamfer, u32 seg);
PgMesh *cwg_box3(Cwg *c, f32 w, f32 h, f32 d);
PgMesh *cwg_blob(Cwg *c, f32 w, f32 h, f32 d, f32 radius, u32 seg);
PgMesh *cwg_lathe_zv(Cwg *c, const v2 *profile, u32 count, u32 seg, f32 phi_start, f32 phi_length);
PgMesh *cwg_tube_z(Cwg *c, f32 r_outer, f32 r_inner, f32 len, u32 seg, f32 crown);
PgMesh *cwg_rod_z(Cwg *c, f32 r0, f32 r1, f32 len, u32 seg, f32 chamfer);
PgMesh *cwg_dome(Cwg *c, f32 r, u32 seg, f32 cut);
u32 cwg_round_rect(v2 *out, f32 w, f32 h, f32 r, u32 seg);
PgMesh *cwg_extrude(Cwg *c, const v2 *pts, u32 count, f32 depth, f32 bevel, u32 bevel_segments, const PgPoly *holes, u32 hole_count);
PgMesh *cwg_ring(Cwg *c, f32 radius, f32 thickness, u32 seg, u32 rings);
PgMesh *cwg_screw(Cwg *c, f32 r_head, f32 r_shank, f32 head_h, f32 shank_l, u32 seg);
PgMesh *cwg_knurl_band(Cwg *c, f32 radius, f32 len, u32 count, f32 depth, u32 rows);
PgMesh *cwg_serrations(Cwg *c, f32 w, f32 h, f32 len, u32 count, f32 depth, char axis);
typedef struct CwgPicatinny {
  f32 width, waist, base_h, top_h, pitch, slot, crown_chamfer; // zero = the MIL-STD-1913 default
} CwgPicatinny;
PgMesh *cwg_picatinny(Cwg *c, f32 len, const CwgPicatinny *o);
PgMesh *cwg_mlok_slot(Cwg *c, f32 len, f32 wide, f32 depth);
void cwg_add(Cwg *c, CwpAsm *a, const PgMesh *g, u32 mat, CwgT t);
void cwg_add_mirrored(Cwg *c, CwpAsm *a, const PgMesh *g, u32 mat, CwgT t);
void cwg_asm_finish(CwpAsm *a, f32 wear_scale, u32 seed, u32 *tris);

#define cwg_lathe_z(c, seg, ...)                                                                                   \
  cwg_lathe_zv((c), (const v2[]){__VA_ARGS__}, (u32)(sizeof((const v2[]){__VA_ARGS__}) / sizeof(v2)), (seg), 0.0f, 0.0f)

// literal point lists as (pointer, count) argument pairs and as polygons
#define CWG_PTS(...) (const v2[]){__VA_ARGS__}, (u32)(sizeof((const v2[]){__VA_ARGS__}) / sizeof(v2))
#define CWG_POLY(...) ((PgPoly){(const v2[]){__VA_ARGS__}, (u32)(sizeof((const v2[]){__VA_ARGS__}) / sizeof(v2))})
#define CWG_HOLES(...) (const PgPoly[]){__VA_ARGS__}, (u32)(sizeof((const PgPoly[]){__VA_ARGS__}) / sizeof(PgPoly))
// a polygon extruded to depth, with bevel, without holes
#define cwg_ext(c, depth, bevel, ...) cwg_extrude((c), CWG_PTS(__VA_ARGS__), (depth), (bevel), 1, NULL, 0)
#define cwg_ext_seg(c, depth, bevel, bseg, ...) cwg_extrude((c), CWG_PTS(__VA_ARGS__), (depth), (bevel), (bseg), NULL, 0)

// a rounded rectangle as a polygon on the scratch arena
PgPoly cwg_rr(Cwg *c, f32 w, f32 h, f32 r, u32 seg);
// a rounded rectangle extruded to depth, with bevel and holes
PgMesh *cwg_ext_rr(Cwg *c, f32 w, f32 h, f32 r, u32 seg, f32 depth, f32 bevel);

typedef struct CwgRail {
  CwgPicatinny pic;     // base_h and top_h also set the rail's own height; zero = the defaults
  b32 no_slot_floor;
} CwgRail;

typedef struct CwgBarrelOpts {
  f32 y, z_breech, z_muzzle;
  f32 r_chamber, r_barrel, r_gas, gas_at; // zero = the default
  u32 seg;
  b32 no_knurl;
} CwgBarrelOpts;

typedef struct CwgGasOpts {
  f32 y, z, r_barrel, w, h, len, tube_to;
} CwgGasOpts;

typedef struct CwgHandguardOpts {
  u32 mat_panel;
  f32 y, z0, z1, r, slat_w, slat_t;
  u32 sides, slots, braces;
  b32 has_top;
  f32 top_from, top_to;
} CwgHandguardOpts;

typedef struct CwgUpperOpts {
  f32 z_rear, z_front, bore, r, port_z, rail_top;
} CwgUpperOpts;

typedef struct CwgLowerOpts {
  f32 bore, z_rear, z_front, w, mag_w, mag_d, mag_top, mag_bottom, mag_z, mag_tilt, trigger_z, grip_angle;
} CwgLowerOpts;

typedef struct CwgGripOpts {
  f32 y, z, angle, len, w;
} CwgGripOpts;

typedef struct CwgStockOpts {
  f32 bore, z_front, z_rear, y;
} CwgStockOpts;

typedef struct CwgMagOpts {
  f32 w, d, len, curve;
  u32 segs, witness;
  f32 case_len, rim_r, bullet_len; // zero = 5.56
  u32 poly;
} CwgMagOpts;

typedef struct CwgOpticOpts {
  f32 r_tube, len, hood, y, z, rail_top;
  u32 mat_body, mat_steel;
} CwgOpticOpts;

typedef struct CwgReflexOpts {
  f32 w, h, len, y, z, tilt;
  u32 mat_body;
} CwgReflexOpts;

typedef struct CwgSlideOpts {
  f32 w, h, len, z_rear;
  u32 mat;
} CwgSlideOpts;

typedef struct CwgSlideOut {
  f32 z_rear, z_front, w, h, len, sight_y;
} CwgSlideOut;

typedef struct CwgCartridge {
  PgMesh *brass;
  PgMesh *bullet;
  f32 length;
} CwgCartridge;

typedef struct CwgRollmark {
  f32 x, y, z, h, stroke, depth, pitch;
  const u32 *pattern;
  u32 pattern_len;
  u32 count;
} CwgRollmark;

void cwg_add_pin(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 r, f32 len);
void cwg_add_screw(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 r_head, char axis, f32 len);
void cwg_add_qd_socket(Cwg *c, CwpAsm *a, u32 mat_body, u32 mat_steel, f32 x, f32 y, f32 z, char axis, f32 r);
void cwg_add_sling_loop(Cwg *c, CwpAsm *a, u32 mat, f32 x, f32 y, f32 z, f32 radius, CwgT rot);
CwgCartridge cwg_cartridge(Cwg *c, f32 case_len, f32 rim_r, f32 bullet_len);
PgMesh *cwg_empty_case(Cwg *c, f32 case_len, f32 rim_r);
void cwg_add_rail(Cwg *c, CwpAsm *a, u32 mat, f32 z0, f32 z1, f32 y, f32 x, const CwgRail *opts);
CwgBarrel cwg_add_barrel(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_cavity, const CwgBarrelOpts *o);
void cwg_add_gas_block(Cwg *c, CwpAsm *a, u32 mat_steel, const CwgGasOpts *o);
CwgMuzzleOut cwg_add_muzzle_device(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_cavity, u32 kind, f32 z_barrel_end, f32 r_barrel, f32 y);
void cwg_add_handguard(Cwg *c, CwpAsm *a, u32 mat_alu, const CwgHandguardOpts *o);
void cwg_add_upper_receiver(Cwg *c, CwpAsm *a, u32 mat, u32 mat_steel, u32 mat_cavity, const CwgUpperOpts *o);
void cwg_add_bolt_carrier(Cwg *c, CwpAsm *a, u32 mat_steel, f32 y, f32 r, f32 len, f32 z);
void cwg_add_lower_receiver(Cwg *c, CwpAsm *a, u32 mat, u32 mat_steel, const CwgLowerOpts *o);
PgMesh *cwg_selector_part(Cwg *c, f32 r);
PgMesh *cwg_trigger_part(Cwg *c);
void cwg_add_pistol_grip(Cwg *c, CwpAsm *a, u32 mat_poly, u32 mat_rubber, const CwgGripOpts *o);
void cwg_add_carbine_stock(Cwg *c, CwpAsm *a, u32 mat_alu, u32 mat_poly, u32 mat_rubber, const CwgStockOpts *o);
CwgMag cwg_build_magazine(Cwg *c, CwpAsm *a, const CwgMagOpts *o);
CwpOptic cwg_build_optic(Cwg *c, CwpAsm *a, const CwgOpticOpts *o);
void cwg_add_rollmark(Cwg *c, CwpAsm *a, u32 mat, const CwgRollmark *o);
void cwg_add_front_sight(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_alu, f32 x, f32 rail_top, f32 z, b32 up);
void cwg_add_rear_sight(Cwg *c, CwpAsm *a, u32 mat_steel, u32 mat_alu, f32 x, f32 rail_top, f32 z, b32 up);
PgMesh *cwg_charging_handle(Cwg *c);
void cwg_add_fore_grip(Cwg *c, CwpAsm *a, u32 mat_poly, u32 mat_rubber, f32 y, f32 z, f32 angle, f32 len);
CwpOptic cwg_build_mini_reflex(Cwg *c, CwpAsm *a, const CwgReflexOpts *o);
CwgSlideOut cwg_build_slide(Cwg *c, CwpAsm *a, const CwgSlideOpts *o);

#define CWG_SCOPE_BEGIN(c) TempArena cwg_scope_ = temp_begin((c)->scratch)
#define CWG_SCOPE_END() temp_end(cwg_scope_)
