---
title: Stalker
description: stalker block — an enemy character that perceives StalkerTarget entities by sight (range, cone, raycast) and noise, decides IDLE / INVESTIGATE / CHASE / SEARCH, paths on the engine navmesh (rate-limited requests) and steers CharacterInput; Stalker, StalkerSenses, StalkerTarget, held mode for game-owned states
resource: src/gameplay/stalker/stalker.h
tags: [gameplay, ai, enemy, navigation, character]
updated: 2026-10-13
---

# What it provides

- `StalkerSenses` (reflected): sight range and cone, `hear_scale`, `lose_after_s` (an unseen chased target is still
  tracked this long), `search_s`, walk and chase speeds, `repath_s`, `sight_layer` (category mask of what blocks sight
  on the static and kinematic layers).
- `Stalker` (runtime): `state`, `target`, `goal`, `alert`, `timer`, `path`, plus the frame's perception (`sees`,
  `last_seen`, `heard`, `noise`) and steering (`steer`, `arrived`, `halt`, `speed`).
- `StalkerTarget` tag on what it hunts (the player); remove it to hide the target (a locker).
- Systems, in order: begin + gather (single), perceive (parallel), decide (parallel) — `stalker_schedule`; path
  (parallel; `ecs_navigation_request` when the goal left the path's goal and `repath_s` passed, the old path followed
  until the answer) and move (writes `CharacterInput` and the tuning's speeds and facing) — `stalker_schedule_move`.

# Game-owned states

Set `Stalker.held` in a system scheduled between the two calls: decide skips the stalker and the game writes `state`,
`goal`, `speed` and `halt` (stand still facing the goal). Clearing `held` hands it back. last-signal drives its
Watchman's spawn hold, locker search and walk-off this way, and leaves the chase to the block.

# Usage

```c
ecs_navigation_init(w, &(EcsNavigationInitDesc){.bounds = level_bounds});
stalker_init(w, (NavAgentKey){.radius = 0.3f, .height = 1.8f});
EcsEntity e = character_spawn(w, "Stalker", pos, &tuning);
stalker_add(w, e, &(StalkerSenses){.sight_range = 20, .sight_cone_deg = 110, .hear_scale = 1, .lose_after_s = 3,
                                   .search_s = 8, .walk_speed = 1.6f, .chase_speed = 3.8f, .repath_s = 0.25f});
ecs_component_add(w, player, StalkerTarget);
// scope
ecs_physics_schedule(w);
ecs_navigation_schedule(w);
stalker_schedule(w);
ecs_schedule_single(w, game_enemy_system); // optional: held stalkers
stalker_schedule_move(w);
// fixed loop: character_schedule_fixed(w)
```

# Gotchas

- Closed doors that the stalker should open must not cut the navmesh: give their colliders `NavVolume` (walkable
  area) so paths go through and the game opens the door when the stalker reaches it.
- A noise emitted this frame is heard next frame.
