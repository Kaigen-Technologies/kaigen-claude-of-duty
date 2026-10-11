---
title: Test Clock
description: how do I run a scripted capture on a fixed 1/60 s step clock that freezes after N steps — TestClock.steps, TestClockFrame, test_clock_schedule, "capture frozen at step N"
resource: src/gameplay/test_clock/test_clock.h
tags: [gameplay, testing, capture, clock]
updated: 2026-10-03
---

# What it provides

- `TestClock { u32 steps; }`: a reflected component on one singleton entity
  (`test_clock_init`). A scripted run sets it: `1.0:set:TestClock.steps=120`.
- `TestClockFrame { dt, time, step, frozen }`: the resource every game clock
  reads. `dt` is the game's own dt until a capture runs, then exactly 1/60 s
  for `steps` frames, then 0; `time` is the sum of dt, restarted from 0 when a
  capture starts; `step` counts capture steps (0 when none runs); `frozen`
  is true once the budget is used.
- `test_clock_schedule(w, dt, ready)`: one single-lane system. `dt` is what the
  game wants when no capture runs (its clamp, pause and speed stay in the
  game); `ready` gates the capture start, so loading frames never count.
- `test_clock_restarted(&frame)`: true on the first capture step, the moment a
  game restarts its state for the capture.
- Logs `capture: N steps of 1/60 s` at the start and `capture frozen at step N`
  once at the freeze: the line the threejsparity harness waits for before it
  takes the screenshot.

# Example usage

```c
// app_init, main lane
test_clock_init(w);

// each frame inside ecs_scope(w), before the game's clock system
test_clock_schedule(w, m_minf(memory->dt, MAX_DT), flow->state != LOADING);
ecs_schedule_single(w, game_clock_system);

ecs_system(game_clock_system, EcsResIn(TestClockFrame), EcsResInOut(GameClock)) {
  const TestClockFrame *tc = ecs_sys_get_resource(TestClockFrame);
  GameClock *c = ecs_sys_get_resource_mut(GameClock);
  if (test_clock_restarted(tc))
    restart_state();            // a capture replays from `ready`
  c->dt = tc->dt;
  c->time = tc->time;
}
```

# Gotchas

- `ready` is read where `test_clock_schedule` is called, so it sees the
  previous frame's game state: a capture starts one frame after the game is
  ready.
- Call it before the systems that read `TestClockFrame`: the resource edge
  orders readers after the clock only when the clock was scheduled first.
- A game that needs a different freeze rule writes `TestClock.steps` itself
  from a system scheduled before `test_clock_schedule` (e.g. arm an open-ended
  `0xFFFFFFFF` and set the budget when the run ends).
