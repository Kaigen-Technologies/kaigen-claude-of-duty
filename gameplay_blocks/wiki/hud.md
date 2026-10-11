---
title: HUD Messages and Map
description: banner/toast/hint message channels with priority queue, dedupe, delay and enter/exit tweens, plus HUD map math (world to map px, rotation, circle/rect clip, edge-clamped markers), compass placement and a polyline distance-field bake
resource: src/gameplay/hud/hud_messages.h
tags: [gameplay, hud, ui, messages, map]
updated: 2026-10-01
---

Also `src/gameplay/hud/hud_map.h`. The game keeps its own look: styles are parameters, and the widgets that are
game-specific (crosshair, hit marker, damage indicator, speedometer, XP bar) are not here.

# What it provides

- `hud_messages`: three channels (`HUD_MSG_BANNER/TOAST/HINT`) in the `HudMessages` resource. `hud_msg_push(w, &msg)`
  works from a system on any lane (it raises a request entity the schedule applies); `hud_msg_clear` ends a channel.
- Per message: `title`, `body`, `key`, `kind` (style override), `priority`, `duration`, `delay`, `dedupe`. A channel is
  `HUD_MSG_POLICY_QUEUE` (highest priority first, a higher one preempts) or `REPLACE` (the newest takes over), with
  `enter_s`/`exit_s` tweens; `restart_on_replace` re-enters instead of swapping the text in place.
- `hud_msg_layout(h, ui, channel, &style)` draws the active message as one panel (fade, slide or pop; optional panel
  background, border, blur, shadow and skew; a key cap; `*` marks emphasis and a newline breaks a line);
  `hud_msg_view` gives the phase and level to draw your own panel.
- `hud_map`: `hud_map_project(&map, world_xz, clamp)` returns px from the box centre, north-up or rotated by `heading`,
  circle or rect clip, edge-clamped when `clamp` is set; `hud_bearing` and `hud_compass_place` position a compass strip
  item (x and opacity from a bearing); `hud_map_fit` + `hud_map_bake` bake a polyline into an R16 distance-field texture.

# Example usage

```c
// init (main lane): a toast that replaces the last one and fades in 0.3 s
hud_msg_init(w, &(HudMsgDesc){.channels = {[HUD_MSG_TOAST] = {.policy = HUD_MSG_POLICY_REPLACE, .enter_s = 0.3f, .exit_s = 0.3f}}});
// in the scope, before the last flush
hud_msg_schedule(w);
// from any system
HudMsg m = {.channel = HUD_MSG_TOAST, .duration = 1.6f};
cstr_format(m.title.value, sizeof(m.title.value), "Quality: %", fmt_cstr(name));
m.title.len = cstr_len(m.title.value);
hud_msg_push(it->world, &m);
// UI frame (main lane)
hud_msg_layout(ecs_resource_get(w, HudMessages), ui, HUD_MSG_TOAST, &style);

// radar contact: world offset from the player, rotated so forward is up
HudMap map = {.px_per_m = r / range, .heading = -yaw, .rotate = true, .half_px = {r, r}, .clip = HUD_MAP_CLIP_CIRCLE};
HudMapPoint pt = hud_map_project(&map, (v2){dx, dz}, true);
```

# Gotchas

- `duration` counts from the end of the enter tween; a message that must leave after N seconds total uses `N - enter_s`.
- Messages advance with the flush's `delta_time`: a frozen or scaled game clock freezes or scales them.
- `hud_msg_layout` allocates its text from the frame temp allocator and runs on the main lane inside the UI frame.
- A pushed message is applied at the next flush that runs `hud_msg_schedule`, so schedule it in the same scope that pushes.
- `hud_map_bake` creates a GPU texture: main lane only.
