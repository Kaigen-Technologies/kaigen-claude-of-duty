// HUD map: world -> map pixels (north-up or rotating, circle or rect clip, edge-clamped markers), a compass strip
// placement, and a polyline distance-field bake. Pure functions on values: any lane, no ECS state, drawing stays yours.
#pragma once

#include "gpu/gpu.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"

typedef enum HudMapClip {
  HUD_MAP_CLIP_RECT = 0,
  HUD_MAP_CLIP_CIRCLE,
} HudMapClip;

// north is -Z; heading = 0 faces north and grows clockwise towards +X
typedef struct HudMap {
  v2 center;       // world xz at the map centre
  f32 px_per_m;
  f32 heading;     // radians; rotates the map so `heading` points up when `rotate` is set
  b32 rotate;
  v2 half_px;      // half size of the map box in px (the radius in both axes for a circle)
  HudMapClip clip;
  f32 edge_margin; // px kept between a clamped marker and the clip edge
} HudMap;

typedef struct HudMapPoint {
  v2 px;           // from the box centre, y down; clamped to the clip edge when outside
  b32 inside;      // before clamping
  b32 clamped;
  f32 angle;       // direction from the centre to the unclamped point, atan2(y, x) in px space
} HudMapPoint;

// world xz -> map px relative to the box centre; `clamp` pins an outside point to the clip edge
HudMapPoint hud_map_project(const HudMap *m, v2 world_xz, b32 clamp);

// compass bearing (0 = north, clockwise) of `to` seen from `from`, both xz
f32 hud_bearing(v2 from, v2 to);

typedef struct HudCompass {
  f32 width;       // strip width px
  f32 span;        // radians visible either side of the heading
  f32 fade_power;  // opacity = (1 - |rel| / span) ^ fade_power
  f32 mask_edge;   // fraction of the width faded at each end; 0 = none
} HudCompass;

// x (px from the strip's left edge) and opacity of a bearing; false when outside the span
b32 hud_compass_place(const HudCompass *c, f32 bearing, f32 heading, f32 *x, f32 *alpha);

typedef struct HudMapFit {
  f32 scale;       // px per metre
  v2 offset;       // world xz (0,0) in px
} HudMapFit;

// scale and offset that centre the bounds of the points in a square canvas with `pad_frac` of margin
HudMapFit hud_map_fit(const v2 *points, u32 count, f32 canvas_px, f32 pad_frac);

// R16 unorm distance (0..range_px, sampled at texel centres) to the polyline in canvas px; main lane (creates a GPU texture)
GpuTexture hud_map_bake(const v2 *points, u32 count, b32 closed, const HudMapFit *fit, u32 canvas_px, f32 range_px);
