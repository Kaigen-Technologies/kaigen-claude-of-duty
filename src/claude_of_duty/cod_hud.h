// Claude of Duty HUD and menus on HZ UI with the hud and world_ui blocks.
#pragma once
#include "ecs/ecs.h"
#include "lib/typedefs.h"
#include "ui/ui.h"
#include "claude_of_duty.h"

// the pause menu's requests to the flow; the flow consumes them in its system next frame
typedef enum CodMenuAction {
  COD_MENU_NONE = 0,
  COD_MENU_RESUME = 1 << 0,
  COD_MENU_SETTINGS = 1 << 1, // CodMenuRequests.settings holds the new values
} CodMenuAction;

typedef struct CodMenuRequests {
  u32 queue; // CodMenuAction bits, written on the main lane after the scope
  CodSettings settings;
} CodMenuRequests;
ECS_COMPONENT_TYPEDEF(CodMenuRequests);

// main lane, app_init: the UI system, fonts, the HUD model resources
void cod_hud_init(EcsWorld *w, Allocator *alloc);
// every lane, before the UI frame (collective font update)
void cod_hud_font_update(EcsWorld *w);
// inside the scope: the HUD model consolidation (reads player, weapons, AI, flow)
void cod_hud_schedule(EcsWorld *w);
// inside the scope after ecs_camera_schedule: the world_ui and hud_msg schedules and the projection gathers
void cod_hud_schedule_late(EcsWorld *w);
// main lane after the scope: the UI frame
void cod_hud_frame(EcsWorld *w);
