---
title: Stamina
description: stamina block — a run meter that drains while running and moving, refills after a rest delay, and gates CharacterInput.run (empty, or below recover_at to start); StaminaTuning, Stamina, stamina_add, stamina_schedule
resource: src/gameplay/stamina/stamina.h
tags: [gameplay, character, stamina, sprint]
updated: 2026-10-13
---

# What it provides

- `StaminaTuning` (reflected): `max` (0 -> 100), `drain_per_s`, `regen_per_s`, `regen_delay_s`, `recover_at`.
- `Stamina` (runtime): `value`, `since_drain`, `running` (the run passed this step), `exhausted`.
- `stamina_system`, parallel per character: a run passes while `value > 0` and either it was already running or
  `value >= recover_at`; it writes the gate back into `CharacterInput.run`. Running with `move` drains; otherwise
  after `regen_delay_s` it refills.

# Usage

```c
stamina_add(w, player, &(StaminaTuning){.max = 100, .drain_per_s = 20, .regen_per_s = 14, .regen_delay_s = 1, .recover_at = 10});
// fixed step, in the scope
ecs_schedule_single(w, game_move_system); // writes CharacterInput.run = the player's wish
stamina_schedule(w);
character_schedule_fixed(w);
```

Refill on a respawn by writing `Stamina` directly. `stamina_fraction(s, t)` is the 0..1 bar.
