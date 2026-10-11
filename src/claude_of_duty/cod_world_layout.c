// Claude of Duty world layout: the map's footprints, facade programmes and hand-placed set pieces, in level space.

#define CW_STREET_HW 4.5f
#define CW_STREET_KERB 6.5f
#define CW_STREET_WALK_H 0.145f
#define CW_STREET_ZMIN -58.0f
#define CW_STREET_ZMAX 46.0f

typedef struct CwAlley {
  f32 x0, z0, x1, z1;
  u32 key; // CwKey of the floor
} CwAlley;

hz_internal const CwAlley CW_ALLEYS[] = {
    {-27.0f, -12.2f, -6.5f, -8.2f, CW_DIRT},  {-26.0f, 20.2f, -6.5f, 24.2f, CW_DIRT}, {-25.0f, 5.6f, -6.5f, 9.6f, CW_DIRT},
    {6.5f, 1.8f, 29.0f, 7.8f, CW_DIRT},       {6.5f, -14.2f, 29.0f, -30.2f, CW_GRAVEL}, {-30.0f, -50.0f, 30.0f, -44.0f, CW_DIRT},
};
#define CW_ALLEY_COUNT 6u

typedef enum CwRoomKind { CWR_SHOP = 0, CWR_STORAGE, CWR_LIVING, CWR_RUIN } CwRoomKind;

typedef struct CwRoomWall {
  f32 ax, az, bx, bz, door;
} CwRoomWall;

typedef struct CwFurnish {
  u32 kind;
  f32 x0, z0, x1, z1;
} CwFurnish;

typedef struct CwRoomPlan {
  u32 wall_count;
  CwRoomWall walls[3];
  u32 furnish_count;
  CwFurnish furnish[4];
} CwRoomPlan;

typedef struct CwStairFlight {
  i32 floor;
  f32 x, z, ry, w;
  b32 railing_right;
} CwStairFlight;

typedef struct CwStairHole {
  b32 present;
  f32 x0, x1, z0, z1;
} CwStairHole;

typedef struct CwBuildingSpec {
  const char *id;
  f32 x, z, w, d;
  u32 floors;
  b32 has_setback;
  i32 setback_from;
  f32 setback_depth;
  i32 setback_side;
  u32 wall_key;
  i32 trim_key; // -1: concrete
  i32 street_side;
  i32 secondary_side; // -1 none
  f32 damage;
  f32 balconies;
  b32 arches;
  i32 door_bay[4]; // bay index per side, -1 none
  b32 has_forced;
  i32 forced_side, forced_floor, forced_bay;
  f32 forced_drop;
  b32 enterable, roof_access, ruin, collapse;
  i32 ruin_side;
  u32 skip_sides; // bit per side
  u32 roof_props;
  u32 room_count;
  CwRoomPlan rooms[3];
  u32 flight_count;
  CwStairFlight flights[2];
  CwStairHole holes[4]; // by level
} CwBuildingSpec;

#define CW_NONE4 {-1, -1, -1, -1}

hz_internal const CwBuildingSpec CW_BUILDINGS[] = {
    {.id = "W5", .x = -12.5f, .z = 31, .w = 12, .d = 14, .floors = 2, .has_setback = true, .setback_from = 1, .setback_depth = 2.2f,
     .setback_side = 1, .wall_key = CW_PLASTER_CREAM, .trim_key = -1, .street_side = 1, .secondary_side = 0, .damage = 0.15f,
     .balconies = 0.3f, .door_bay = {-1, 1, -1, -1}, .roof_props = 3},
    {.id = "W1", .x = -13.5f, .z = 15, .w = 14, .d = 10, .floors = 2, .has_setback = true, .setback_from = 1, .setback_depth = 2.6f,
     .setback_side = 1, .wall_key = CW_PLASTER_CREAM, .trim_key = CW_CONCRETE, .street_side = 1, .secondary_side = 2, .damage = 0.25f,
     .balconies = 0.55f, .arches = true, .door_bay = {-1, 1, -1, -1}, .roof_props = 4},
    {.id = "W2", .x = -14, .z = -1.5f, .w = 15, .d = 13, .floors = 2, .has_setback = true, .setback_from = 1, .setback_depth = 2.4f,
     .setback_side = 1, .wall_key = CW_PLASTER_SAND, .trim_key = -1, .street_side = 1, .secondary_side = 0, .damage = 0.3f,
     .balconies = 0.6f, .door_bay = {-1, 2, -1, -1}, .has_forced = true, .forced_side = 1, .forced_floor = 0, .forced_bay = 1,
     .forced_drop = 0.0f, .enterable = true, .roof_access = false, .roof_props = 5,
     .room_count = 3,
     .rooms = {{.wall_count = 2,
                .walls = {{0.55f, 0.0f, 0.55f, 1.0f, 0.34f}, {0.0f, 0.52f, 0.55f, 0.52f, 0.7f}},
                .furnish_count = 3,
                .furnish = {{CWR_SHOP, 0.55f, 0.0f, 1.0f, 1.0f}, {CWR_STORAGE, 0.0f, 0.0f, 0.55f, 0.52f}, {CWR_LIVING, 0.0f, 0.52f, 0.55f, 1.0f}}},
               {.wall_count = 2,
                .walls = {{0.48f, 0.0f, 0.48f, 1.0f, 0.62f}, {0.48f, 0.45f, 1.0f, 0.45f, 0.25f}},
                .furnish_count = 3,
                .furnish = {{CWR_LIVING, 0.48f, 0.45f, 1.0f, 1.0f}, {CWR_STORAGE, 0.48f, 0.0f, 1.0f, 0.45f}, {CWR_LIVING, 0.0f, 0.0f, 0.48f, 1.0f}}},
               {.wall_count = 1,
                .walls = {{0.5f, 0.0f, 0.5f, 1.0f, 0.5f}},
                .furnish_count = 2,
                .furnish = {{CWR_RUIN, 0.5f, 0.0f, 1.0f, 1.0f}, {CWR_STORAGE, 0.0f, 0.0f, 0.5f, 1.0f}}}},
     .flight_count = 1, .flights = {{0, 0.14f, 0.28f, 0.0f, 1.2f, true}},
     .holes = {{0}, {true, -20.4f, -18.0f, -4.8f, 1.5f}, {0}, {0}}},
    {.id = "W3", .x = -13, .z = -19, .w = 13, .d = 14, .floors = 2, .wall_key = CW_PLASTER_BLUE, .trim_key = -1, .street_side = 1,
     .secondary_side = 2, .damage = 0.55f, .ruin = true, .ruin_side = 1, .balconies = 0.3f, .door_bay = {-1, 0, -1, -1}, .roof_props = 2},
    {.id = "W4", .x = -14.5f, .z = -34.5f, .w = 16, .d = 15, .floors = 2, .has_setback = true, .setback_from = 1, .setback_depth = 2.8f,
     .setback_side = 1, .wall_key = CW_PLASTER_PINK, .trim_key = -1, .street_side = 1, .secondary_side = 0, .damage = 0.3f,
     .balconies = 0.5f, .arches = true, .door_bay = {-1, 2, -1, -1}, .roof_props = 4},
    {.id = "E5", .x = 12.5f, .z = 33, .w = 12, .d = 14, .floors = 2, .wall_key = CW_PLASTER_BLUE, .trim_key = -1, .street_side = 3,
     .secondary_side = -1, .damage = 0.2f, .door_bay = {-1, -1, -1, 2}, .roof_props = 3},
    {.id = "E1", .x = 14, .z = 16, .w = 15, .d = 16, .floors = 3, .wall_key = CW_PLASTER_CREAM, .trim_key = -1, .street_side = 3,
     .secondary_side = 0, .damage = 0.3f, .balconies = 0.45f, .door_bay = {1, -1, -1, 2}, .enterable = true, .roof_access = true,
     .roof_props = 6,
     .room_count = 2,
     .rooms = {{.wall_count = 2,
                .walls = {{0.0f, 0.42f, 0.62f, 0.42f, 0.3f}, {0.62f, 0.0f, 0.62f, 0.42f, 0.5f}},
                .furnish_count = 2,
                .furnish = {{CWR_SHOP, 0.0f, 0.42f, 0.62f, 1.0f}, {CWR_STORAGE, 0.0f, 0.0f, 0.62f, 0.42f}}},
               {.wall_count = 1,
                .walls = {{0.0f, 0.45f, 0.62f, 0.45f, 0.72f}},
                .furnish_count = 2,
                .furnish = {{CWR_LIVING, 0.0f, 0.45f, 0.62f, 1.0f}, {CWR_RUIN, 0.0f, 0.0f, 0.62f, 0.45f}}}},
     .flight_count = 2, .flights = {{0, 0.72f, 0.12f, 0.0f, 1.2f, true}, {1, 0.72f, 0.12f, 0.0f, 1.2f, true}},
     .holes = {{0}, {true, 16.3f, 18.1f, 9.4f, 16.2f}, {true, 16.3f, 18.1f, 9.4f, 16.2f}, {0}}},
    {.id = "E2", .x = 13.5f, .z = -5, .w = 14, .d = 14, .floors = 3, .wall_key = CW_PLASTER_BLUE, .trim_key = -1, .street_side = 3,
     .secondary_side = 2, .damage = 0.3f, .balconies = 0.7f, .door_bay = {-1, -1, -1, 1}, .roof_props = 5},
    {.id = "E3", .x = 14, .z = -22, .w = 15, .d = 16, .floors = 2, .wall_key = CW_PLASTER_SAND, .trim_key = -1, .street_side = 3,
     .secondary_side = 0, .damage = 0.75f, .ruin = true, .ruin_side = 3, .collapse = true, .door_bay = {-1, -1, -1, 1}, .enterable = true,
     .roof_props = 2,
     .room_count = 2,
     .rooms = {{.wall_count = 1,
                .walls = {{0.45f, 0.0f, 0.45f, 0.7f, 0.4f}},
                .furnish_count = 3,
                .furnish = {{CWR_RUIN, 0.45f, 0.0f, 1.0f, 1.0f}, {CWR_STORAGE, 0.0f, 0.0f, 0.45f, 0.7f}, {CWR_RUIN, 0.0f, 0.7f, 0.45f, 1.0f}}},
               {.wall_count = 0, .furnish_count = 1, .furnish = {{CWR_RUIN, 0.0f, 0.0f, 1.0f, 1.0f}}}}},
    {.id = "E4", .x = 13.5f, .z = -39, .w = 14, .d = 14, .floors = 3, .wall_key = CW_PLASTER_PINK, .trim_key = -1, .street_side = 3,
     .secondary_side = 2, .damage = 0.35f, .balconies = 0.4f, .arches = true, .door_bay = {-1, -1, -1, 2}, .roof_props = 4},
    {.id = "BS3", .x = -4, .z = -53, .w = 9, .d = 8, .floors = 4, .wall_key = CW_PLASTER_SAND, .trim_key = -1, .street_side = 2,
     .secondary_side = -1, .damage = 0.3f, .balconies = 0.2f, .door_bay = CW_NONE4, .roof_props = 4},
    {.id = "BW1", .x = -30, .z = 8, .w = 16, .d = 22, .floors = 3, .wall_key = CW_PLASTER_SAND, .trim_key = -1, .street_side = 1,
     .secondary_side = -1, .damage = 0.15f, .door_bay = CW_NONE4, .skip_sides = 1u << 1, .roof_props = 3},
    {.id = "BW2", .x = -31, .z = -18, .w = 18, .d = 24, .floors = 2, .wall_key = CW_PLASTER_CREAM, .trim_key = -1, .street_side = 1,
     .secondary_side = -1, .damage = 0.2f, .door_bay = CW_NONE4, .skip_sides = 1u << 1, .roof_props = 2},
    {.id = "BE1", .x = 31, .z = 18, .w = 18, .d = 20, .floors = 3, .wall_key = CW_PLASTER_PINK, .trim_key = -1, .street_side = 3,
     .secondary_side = -1, .damage = 0.15f, .door_bay = CW_NONE4, .skip_sides = 1u << 3, .roof_props = 3},
    {.id = "BE2", .x = 32, .z = -8, .w = 18, .d = 20, .floors = 2, .wall_key = CW_PLASTER_BLUE, .trim_key = -1, .street_side = 3,
     .secondary_side = -1, .damage = 0.2f, .door_bay = CW_NONE4, .skip_sides = 1u << 3, .roof_props = 2},
    {.id = "BE3", .x = 30, .z = -34, .w = 16, .d = 18, .floors = 3, .wall_key = CW_PLASTER_CREAM, .trim_key = -1, .street_side = 3,
     .secondary_side = -1, .damage = 0.25f, .door_bay = CW_NONE4, .skip_sides = 1u << 3, .roof_props = 2},
    {.id = "BS1", .x = -19, .z = -58, .w = 20, .d = 14, .floors = 3, .wall_key = CW_PLASTER_SAND, .trim_key = -1, .street_side = 2,
     .secondary_side = -1, .damage = 0.2f, .door_bay = CW_NONE4, .roof_props = 2},
    {.id = "BS2", .x = 14, .z = -60, .w = 24, .d = 16, .floors = 2, .wall_key = CW_PLASTER_BLUE, .trim_key = -1, .street_side = 2,
     .secondary_side = -1, .damage = 0.2f, .door_bay = CW_NONE4, .roof_props = 2},
    {.id = "BN1", .x = -16, .z = 50, .w = 20, .d = 14, .floors = 2, .wall_key = CW_PLASTER_CREAM, .trim_key = -1, .street_side = 0,
     .secondary_side = -1, .damage = 0.15f, .door_bay = CW_NONE4, .roof_props = 2},
    {.id = "BN2", .x = 14, .z = 52, .w = 22, .d = 16, .floors = 3, .wall_key = CW_PLASTER_PINK, .trim_key = -1, .street_side = 0,
     .secondary_side = -1, .damage = 0.15f, .door_bay = CW_NONE4, .roof_props = 2},
};
#define CW_BUILDING_COUNT 20u

// the street terminator (GATE)
#define CW_GATE_Z -42.5f
#define CW_GATE_DEPTH 3.2f
#define CW_GATE_SPAN 5.6f
#define CW_GATE_HEIGHT 4.9f
#define CW_GATE_OUTER_W 17.0f
#define CW_GATE_BODY_H 6.7f
#define CW_GATE_XL0 -8.6f
#define CW_GATE_XL1 -2.8f
#define CW_GATE_HL 7.9f
#define CW_GATE_XR0 2.8f
#define CW_GATE_XR1 6.1f
#define CW_GATE_HR 9.5f
#define CW_GATE_EAST_PROUD 0.55f
#define CW_GATE_XT0 6.1f
#define CW_GATE_XT1 9.4f
#define CW_GATE_HT 12.4f
#define CW_GATE_TOWER_PROUD 1.5f

// hand-placed set pieces
typedef struct CwStall {
  f32 x, z, ry, w;
} CwStall;
hz_internal const CwStall CW_STALLS[] = {{-3.2f, 6.4f, 0.08f, 2.4f}, {-3.0f, 2.2f, -0.05f, 2.2f}, {3.1f, 9.5f, 3.2f, 2.4f},
                                         {3.4f, 4.0f, 3.05f, 2.6f},  {-0.4f, 2.6f, 1.62f, 2.3f},  {3.0f, -9.0f, 3.25f, 2.2f},
                                         {-3.3f, -14.5f, 0.12f, 2.4f}, {2.9f, -20.0f, 3.0f, 2.3f}};

typedef struct CwXZR {
  f32 x, z, ry;
} CwXZR;
hz_internal const CwXZR CW_JERSEYS[] = {{-2.6f, 17.5f, 0.12f}, {-0.4f, 16.2f, 1.5f}, {2.9f, 12.0f, -0.1f},
                                        {1.6f, -2.5f, 1.62f},  {-2.4f, -6.0f, 0.05f}, {3.2f, -16.0f, 0.1f},
                                        {-1.0f, -24.0f, 1.55f}, {1.2f, -30.0f, 0.2f}, {-3.0f, -34.0f, 0.0f}};

typedef struct CwXZRL {
  f32 x, z, ry, len;
} CwXZRL;
hz_internal const CwXZRL CW_SANDBAG_WALLS[] = {{-3.6f, 11.0f, 0.0f, 3.0f}, {3.6f, -2.0f, 0.0f, 2.6f}, {-1.6f, -18.5f, 1.57f, 2.4f},
                                               {3.4f, -27.0f, 0.0f, 3.2f}};

hz_internal const CwXZRL CW_WRECKS[] = {{2.5f, 0.5f, 0.42f, 0.0f}, {-2.8f, -28.5f, -2.6f, 4.0f}, {4.9f, 24.0f, 1.5f, 0.0f}};

typedef struct CwXZS {
  f32 x, z, s;
} CwXZS;
hz_internal const CwXZS CW_PALMS[] = {{-5.4f, 20.0f, 1.0f}, {5.5f, 6.5f, 1.1f},  {-5.5f, -4.5f, 0.92f}, {5.6f, -20.5f, 1.05f},
                                      {-5.5f, -32.0f, 1.0f}, {8.5f, 5.0f, 0.85f}, {-9.0f, -10.2f, 0.9f}};

hz_internal const CwXZR CW_LAMPS[] = {{-5.9f, 15.0f, -1.5707963f}, {5.9f, 3.0f, 1.5707963f}, {-5.9f, -11.0f, -1.5707963f},
                                      {5.9f, -24.0f, 1.5707963f},  {-5.9f, -36.0f, -1.5707963f}};

typedef struct CwSpan {
  f32 x0, y0, z0, x1, y1, z1, sag;
} CwSpan;
hz_internal const CwSpan CW_CABLES[] = {{-6.4f, 7.2f, 10.0f, 6.4f, 6.6f, 12.5f, 1.1f},  {-6.4f, 8.4f, -2.0f, 6.4f, 7.9f, -0.5f, 1.4f},
                                        {-6.4f, 6.2f, -16.0f, 6.4f, 6.6f, -14.5f, 1.0f}, {-6.4f, 7.6f, -30.0f, 6.4f, 7.2f, -28.0f, 1.2f},
                                        {-6.4f, 5.4f, 19.0f, -6.4f, 5.6f, 24.5f, 0.6f},  {6.4f, 5.6f, 2.0f, 6.4f, 5.4f, 8.0f, 0.7f}};

hz_internal const CwSpan CW_LAUNDRY[] = {{6.35f, 3.6f, 9.0f, 6.35f, 3.75f, 14.2f, 0.0f},   {-6.35f, 3.7f, 1.0f, -6.35f, 3.6f, 5.4f, 0.0f},
                                         {-6.35f, 6.6f, -20.5f, -6.35f, 6.4f, -15.5f, 0.0f}, {6.35f, 6.5f, -6.0f, 6.35f, 6.7f, -1.0f, 0.0f},
                                         {-6.35f, 3.65f, -27.0f, -6.35f, 3.8f, -22.0f, 0.0f}, {6.4f, 3.7f, 21.0f, 6.4f, 3.6f, 25.5f, 0.0f}};

typedef struct CwHanging {
  f32 x, y, z, ry, w, h;
} CwHanging;
hz_internal const CwHanging CW_HANGINGS[] = {{-6.45f, 2.6f, 8.5f, 1.5707963f, 1.5f, 2.1f}, {-6.45f, 2.4f, 4.5f, 1.5707963f, 1.2f, 1.7f},
                                             {6.45f, 2.7f, 6.0f, -1.5707963f, 1.6f, 2.2f}, {6.45f, 2.5f, -8.5f, -1.5707963f, 1.3f, 1.9f},
                                             {-6.45f, 2.5f, -16.0f, 1.5707963f, 1.4f, 2.0f}};

typedef struct CwRubble {
  f32 x, z, radius;
  u32 count;
} CwRubble;
hz_internal const CwRubble CW_RUBBLE[] = {{-4.2f, -20.5f, 2.4f, 34}, {5.0f, -14.5f, 2.8f, 40}, {-1.5f, -40.0f, 2.0f, 26},
                                          {7.6f, -30.5f, 2.2f, 28},  {-5.0f, 26.0f, 1.6f, 18}};

typedef struct CwTyres {
  f32 x, z;
  u32 n;
} CwTyres;
hz_internal const CwTyres CW_TYRES[] = {{-5.2f, 12.5f, 4}, {5.3f, -6.0f, 3}, {6.2f, 3.0f, 5}, {-5.4f, -28.0f, 3}};

// SPAWNS: level x, z, yaw (before the level yaw), tag
typedef struct CwSpawnDef {
  f32 x, z, yaw;
} CwSpawnDef;
hz_internal const CwSpawnDef CW_SPAWNS[] = {{0.4f, 22.5f, 3.14159265f}, {-2.4f, 30.0f, 3.14159265f}, {3.6f, 5.0f, 3.14159265f},
                                            {-3.4f, -12.0f, 0.0f},     {2.6f, -32.0f, 0.0f},       {-1.0f, -39.0f, 0.0f},
                                            {10.5f, 4.6f, -1.5707963f}, {-9.0f, -10.2f, 1.5707963f}};
#define CW_SPAWN_COUNT 8u

// ---- ground queries on the layout ----

hz_internal b32 cw_in_building(f32 x, f32 z, f32 m) {
  for (u32 i = 0; i < CW_BUILDING_COUNT; i++) {
    const CwBuildingSpec *b = &CW_BUILDINGS[i];
    if (x > b->x - b->w / 2 - m && x < b->x + b->w / 2 + m && z > b->z - b->d / 2 - m && z < b->z + b->d / 2 + m)
      return true;
  }
  return false;
}

hz_internal b32 cw_is_open(f32 x, f32 z, f32 m) {
  if (cw_in_building(x, z, m))
    return false;
  if (m_fabsf(x) < CW_STREET_KERB - 0.1f && z > CW_STREET_ZMIN && z < CW_STREET_ZMAX)
    return true;
  for (u32 i = 0; i < CW_ALLEY_COUNT; i++) {
    const CwAlley *a = &CW_ALLEYS[i];
    if (x > a->x0 + m && x < a->x1 - m && z > a->z0 + m && z < a->z1 - m)
      return true;
  }
  return false;
}

hz_internal f32 cw_ground_y(f32 x, f32 z) {
  if (m_fabsf(x) < CW_STREET_HW) {
    f32 q = x / CW_STREET_HW;
    return (1.0f - q * q) * 0.055f + 0.004f;
  }
  if (m_fabsf(x) < CW_STREET_KERB && z > CW_STREET_ZMIN && z < CW_STREET_ZMAX)
    return CW_STREET_WALK_H;
  return 0.03f;
}

// distance to the nearest building wall
hz_internal f32 cw_nearest_wall(f32 x, f32 z) {
  f32 best = 1e9f;
  for (u32 i = 0; i < CW_BUILDING_COUNT; i++) {
    const CwBuildingSpec *b = &CW_BUILDINGS[i];
    f32 dx = m_fabsf(x - b->x) - b->w / 2;
    f32 dz = m_fabsf(z - b->z) - b->d / 2;
    f32 d = m_maxf(dx, dz);
    if (d < best)
      best = d;
  }
  return best;
}
