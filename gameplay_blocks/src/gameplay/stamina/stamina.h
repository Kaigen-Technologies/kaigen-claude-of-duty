// Stamina: a run drains it, rest refills it after a delay; empty, or below recover_at when a run would start, the
// block clears CharacterInput.run. Setup: stamina_add on the character; stamina_schedule in each fixed step after the
// game writes CharacterInput and before character_schedule_fixed.
#pragma once

#include "ecs/ecs.h"
#include "gameplay/character/character.h"
#include "lib/reflect.h"

HZ_REFLECT()
ECS_COMPONENT(StaminaTuning) {
  f32 max;           // 0 -> 100
  f32 drain_per_s;   // while running and moving
  f32 regen_per_s;
  f32 regen_delay_s; // rest after the last drain before it refills
  f32 recover_at;    // a run cannot start below this (one already running continues to empty)
};

// runtime
typedef struct Stamina {
  f32 value;
  f32 since_drain; // seconds since the last drain
  b32 running;     // the run passed this step
  b32 exhausted;   // a run cannot start now (empty, or below recover_at while not running)
} Stamina;
ECS_COMPONENT_TYPEDEF(Stamina);

// main lane: StaminaTuning and a full Stamina on a character entity
void stamina_add(EcsWorld *w, EcsEntity e, const StaminaTuning *tuning);
// inside the scope, each fixed step: after the game's CharacterInput write, before character_schedule_fixed
void stamina_schedule(EcsWorld *w);
// 0..1 of max
f32 stamina_fraction(const Stamina *s, const StaminaTuning *t);
