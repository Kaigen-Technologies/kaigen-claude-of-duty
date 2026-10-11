// Claude of Duty weapons: the arm and glove rig, implemented in cod_weapons_hands.c.
#pragma once
#include "cod_weapons_internal.h"

#define CWH_MAX_NODES 24
#define CWH_MAX_MESHES 48
#define CWH_FIT_SLOTS 4

// node indices: 0 root, then the bones, the hand (its meshes are the glove body), 4 fingers of root + 3 joints, the thumb
enum {
  CWH_NODE_ROOT = 0,
  CWH_NODE_UPPER = 1,
  CWH_NODE_FORE = 2,
  CWH_NODE_HAND = 3,
  CWH_NODE_FINGER0 = 4, // finger i: root 4 + 4i, joints 5 + 4i .. 7 + 4i (proximal, middle, distal)
  CWH_NODE_THUMB = 20,  // thumb root 20, joints 21 and 22
  CWH_NODE_COUNT = 23,
};

// the hand poses plus the per-weapon fitted clamps (cwh_arm_fit_pose_slot)
typedef enum CwhPoseId {
  CWH_POSE_GRIP = 0,
  CWH_POSE_WRAP,
  CWH_POSE_CLAMP,
  CWH_POSE_CUP,
  CWH_POSE_OPEN,
  CWH_POSE_PINCH,
  CWH_POSE_FIT0, // + weapon id
  CWH_POSE_COUNT = CWH_POSE_FIT0 + CWH_FIT_SLOTS,
} CwhPoseId;

typedef struct CwpArmNode {
  i32 parent;
  v3 pos;
  quat rot;
  v3 scale;
} CwpArmNode;

typedef struct CwpArmMesh {
  u32 node;
  u32 mat; // CwpMat: GLOVE, GLOVE_PAD, GLOVE_SEAM or SLEEVE
  PgMesh mesh;
} CwpArmMesh;

// finger curls per pose: radians per joint (positive curls), the thumb's two joints and its base Euler XYZ
typedef struct CwhPose {
  f32 fingers[4][3];
  f32 thumb[2];
  v3 thumb_base;
} CwhPose;

// nodes carry local TRS (parent < child, scale 1, the right hand's chirality baked into its meshes); never copy after build
typedef struct CwpArm {
  i32 side; // +1 right, -1 left
  f32 scale;
  f32 l1; // upper bone length
  f32 l2; // forearm bone length
  v3 pole;
  u32 node_count;
  CwpArmNode node[CWH_MAX_NODES];
  u32 mesh_count;
  CwpArmMesh mesh[CWH_MAX_MESHES];
  u32 pose; // CwhPoseId in use
  // private: unmirrored local TRS, joint state, fitted poses and the per-segment dimensions
  v3 rpos[CWH_MAX_NODES];
  quat rrot[CWH_MAX_NODES];
  f32 fj[4][3]; // finger joint rotation.x (= -curl)
  v3 tbase;     // thumb root Euler
  f32 tj[2];    // thumb joint rotation.x
  CwhPose fitted[CWH_FIT_SLOTS];
  b32 fitted_valid[CWH_FIT_SLOTS];
  f32 seg_r[4][4];
  f32 seg_l[4][3];
} CwpArm;

// any lane, deterministic, touches only `arena` (the meshes live there): side +1 right (scale 1), -1 left (scale 0.97)
void cwh_arm_build(CwpArm *a, i32 side, f32 scale, ArenaAllocator *arena);
// the wrist orientation for a finger direction and a back-of-hand direction (weapon space)
quat cwh_hand_basis(v3 finger, v3 back);
// instant; a fit slot that was never fitted falls back to wrap; writes the finger and thumb node rotations
void cwh_arm_set_pose(CwpArm *a, u32 pose);
// the CwhPoseId a weapon's fitted clamp is stored under
u32 cwh_arm_fit_pose_slot(u32 weapon);
// 0 off the trigger, 1 pressed: overrides the index finger of the current pose
void cwh_arm_set_trigger(CwpArm *a, f32 t);
// two-bone solve in rig space (the shoulder is body-fixed, expressed in rig space by the caller): writes upper, fore and hand nodes
void cwh_arm_solve(CwpArm *a, v3 shoulder, v3 hand_pos, quat hand_quat);
// fits the CLAMP pose against a cylinder (build time, rig space), stores it in `fitted_slot` and leaves the arm in it; returns the contact count
u32 cwh_arm_fit_to_cylinder(CwpArm *a, v3 hand_pos, quat hand_quat, v3 axis_point, v3 axis_dir, f32 radius, f32 clearance,
                            u32 fitted_slot, v3 *contacts_out, u32 max_contacts);
// contact AO into the glove meshes' AO channel, measured with the arm in its current pose (call after the fit, with the kept contacts)
void cwh_arm_bake_contact_ao(CwpArm *a, const v3 *contacts, u32 count, f32 radius, f32 peak);
// a node's pose in rig space
void cwh_arm_node_world(const CwpArm *a, u32 node, v3 *pos, quat *rot);
