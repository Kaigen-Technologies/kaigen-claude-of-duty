// Curvature vertex masks: COLOR rgb = wear, grime, extra AO; shared by world, weapons, soldiers.
#pragma once
#include "lib/typedefs.h"
#include "procgen/pg_geo.h"
#include "cod_rng.h"

// mask bake options; cod_mask_opts_default() gives the defaults
typedef struct CodMaskOpts {
  f32 wear;
  f32 grime;
  f32 ao;
  f32 edge_threshold; // convexity above this is a hard edge
  f32 down_grime;     // extra grime on downward faces
  f32 up_wear;        // extra wear on upward faces
} CodMaskOpts;

force_inline CodMaskOpts cod_mask_opts_default(void) { return (CodMaskOpts){1.0f, 1.0f, 1.0f, 0.06f, 0.35f, 0.15f}; }

// writes the masks into the mesh's COLOR stream (enabling it); thread-safe, touches only `m`; rng may be NULL
void cod_bake_masks(PgMesh *m, const CodMaskOpts *opts, CodRng *rng);
