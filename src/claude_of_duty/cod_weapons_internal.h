// Claude of Duty weapons, package-private: the types the gun, hand and viewmodel files share (never included by other packages).
#pragma once
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "procgen/pg_geo.h"
#include "cod_weapons.h"

// the weapon material keys
typedef enum CwpMat {
  CWP_MAT_ALU = 0,
  CWP_MAT_ALU_FINE,
  CWP_MAT_STEEL,
  CWP_MAT_STEEL_SOOT,
  CWP_MAT_STEEL_BRIGHT,
  CWP_MAT_STEEL_BLACK,
  CWP_MAT_POLYMER,
  CWP_MAT_POLYMER_TAN,
  CWP_MAT_RUBBER,
  CWP_MAT_BRASS,
  CWP_MAT_COPPER,
  CWP_MAT_GLOVE,
  CWP_MAT_GLOVE_PAD,
  CWP_MAT_GLOVE_SEAM,
  CWP_MAT_SLEEVE,
  CWP_MAT_CAVITY,
  CWP_MAT_OPTIC_TUBE,
  CWP_MAT_GLASS,
  CWP_MAT_LENS_RING,
  CWP_MAT_LENS_VIG,
  CWP_MAT_COUNT,
} CwpMat;

// an assembly after build: one merged mesh per material in the assembly's own space
typedef struct CwpAsm {
  PgMesh mesh[CWP_MAT_COUNT]; // positions, normals, uv; COLOR = wear, grime, extra ao (baked and shaped); empty = unused
  b32 used[CWP_MAT_COUNT];
} CwpAsm;

// the moving assemblies of a model, each its own node under the weapon
typedef enum CwpPart {
  CWP_PART_MAGAZINE = 0,
  CWP_PART_CHARGING,
  CWP_PART_BOLT,
  CWP_PART_SLIDE,
  CWP_PART_TRIGGER,
  CWP_PART_SELECTOR,
  CWP_PART_COUNT,
} CwpPart;

// a node's rest transform: position and Euler XYZ rotation, weapon space
typedef struct CwpNode {
  v3 pos;
  v3 rot;
} CwpNode;

// a hand target in weapon space: the wrist, the finger direction and the back-of-hand direction
typedef struct CwpGrip {
  v3 pos;
  v3 finger;
  v3 back;
} CwpGrip;

// the handguard cylinder the support hand's fingertips are fitted against
typedef struct CwpHandguard {
  b32 valid;
  v3 axis; // a point on the axis
  v3 dir;
  f32 r;
  f32 z0;
  f32 z1;
} CwpHandguard;

// the optic the reticle collimates through
typedef struct CwpOptic {
  b32 valid;
  v3 center;      // tube centre, weapon space
  f32 lens_z;
  f32 aperture_r; // the ocular clear aperture the reticle vignettes against
  f32 tube_r;
  f32 len;
} CwpOptic;

// one built weapon; every position is weapon space: +X right, +Y up, -Z toward the muzzle
typedef struct CwpModel {
  u32 id; // CodWeaponId
  CwpAsm body;
  CwpAsm part[CWP_PART_COUNT];
  b32 has_part[CWP_PART_COUNT];
  v3 muzzle;
  v3 chamber;
  v3 eject;
  v3 eject_dir; // normalised
  v3 sight;     // the lens point ADS puts on the camera axis
  v3 sight_axis;
  v3 iron_sight;
  CwpGrip grip_r;
  CwpGrip grip_l;
  CwpHandguard handguard;
  CwpNode mag_seat;
  v3 mag_drop;
  CwpNode charge_rest;
  v3 charge_pull;
  CwpNode bolt_rest;
  v3 bolt_travel;
  CwpNode slide_rest;
  v3 slide_travel;
  CwpNode trigger_pivot;
  f32 trigger_pull; // radians about X at full pull
  CwpNode selector_pivot;
  CwpOptic optic;
  f32 case_len;
  f32 rim_r;
  f32 mag_len;
  f32 mag_w;
  f32 mag_d;
  u32 tris;
} CwpModel;

// Euler 'XYZ' to a quaternion (matrix = Rx * Ry * Rz)
force_inline quat cwp_quat_euler_xyz(v3 e) {
  quat qx = quat_axis_angle((v3){1.0f, 0.0f, 0.0f}, e.x);
  quat qy = quat_axis_angle((v3){0.0f, 1.0f, 0.0f}, e.y);
  quat qz = quat_axis_angle((v3){0.0f, 0.0f, 1.0f}, e.z);
  return quat_mul(quat_mul(qx, qy), qz);
}

// ---- implemented in cod_weapons_geo.c / _parts.c / _models.c (guns) ----
// builds one weapon on `arena` (any lane, deterministic, touches only `arena`): parts, assemblies, masks baked and shaped
void cwg_build_model(CwpModel *out, u32 weapon, ArenaAllocator *arena);
// raises the AO channel of every body and part vertex within `radius` of a contact point
void cwg_bake_contact_ao(CwpModel *m, const v3 *contacts, u32 count, f32 radius, f32 peak);
