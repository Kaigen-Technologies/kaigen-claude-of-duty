// Deterministic capture clock for scripted runs: `set:TestClock.steps=N` steps N frames of exactly 1/60 s, then freezes.
//
// Usage: test_clock_init(w) once, then each frame test_clock_schedule(w, dt, ready) before the system that reads
// TestClockFrame; the game keeps its own dt clamp, pause and speed and passes the dt it wants when no capture runs.
//
// Does NOT: pause or scale the game's time, or count loading frames: a capture starts on the first `ready` frame.
#pragma once

#include "ecs/ecs.h"
#include "lib/reflect.h"
#include "lib/typedefs.h"

// the step budget; a singleton entity so a scripted run can set it
HZ_REFLECT()
ECS_COMPONENT(TestClock) {
  u32 steps; // > 0 starts a capture of this many 1/60 s steps once the game is ready
};

// this frame's clock, a resource written by the test clock system
typedef struct TestClockFrame {
  f32 dt;     // the game's dt, 1/60 s on a capture step, 0 once frozen
  f32 time;   // the sum of dt, restarted from 0 when a capture starts
  u32 step;   // capture steps taken so far; 0 when no capture runs
  b32 frozen; // the capture has used its steps and dt is 0
} TestClockFrame;
ECS_COMPONENT_TYPEDEF(TestClockFrame);

// true on the frame a capture takes its first step: the moment a game restarts its state for the capture
force_inline b32 test_clock_restarted(const TestClockFrame *f) { return f->step == 1 && !f->frozen; }

// once, main lane: creates the TestClock entity and the TestClockFrame resource.
void test_clock_init(EcsWorld *w);
// every frame inside the ecs_scope, before the systems that read TestClockFrame. `dt` is what the game wants this
// frame when no capture runs; `ready` gates the capture start (loading frames never count). Reads of game state for
// `ready` see the previous frame's values.
void test_clock_schedule(EcsWorld *w, f32 dt, b32 ready);
