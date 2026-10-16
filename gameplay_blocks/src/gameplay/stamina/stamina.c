#include "stamina.h"
#include "lib/assert.h"

hz_internal f32 stamina_max(const StaminaTuning *t) { return t->max > 0.0f ? t->max : 100.0f; }

f32 stamina_fraction(const Stamina *s, const StaminaTuning *t) { return m_saturatef(s->value / stamina_max(t)); }

void stamina_add(EcsWorld *w, EcsEntity e, const StaminaTuning *tuning) {
  assert_msg(is_main_thread(), "stamina_add: main lane only"); // adds components outside a scope
  ecs_component_set(w, e, StaminaTuning, *tuning);
  ecs_component_set(w, e, Stamina, {.value = stamina_max(tuning), .since_drain = tuning->regen_delay_s});
}

// per character, parallel: gate the run on the stamina, drain while running and moving, refill after the rest delay
ecs_system(stamina_system, EcsIn(StaminaTuning), EcsInOut(Stamina), EcsInOut(CharacterInput)) {
  f32 dt = it->delta_time;
  for (i32 i = 0; i < it->count; i++) {
    const StaminaTuning *t = ecs_sys_get_component(StaminaTuning, i);
    Stamina *s = ecs_sys_get_component_mut(Stamina, i);
    CharacterInput *in = ecs_sys_get_component_mut(CharacterInput, i);
    f32 max = stamina_max(t);
    b32 can = s->value > 0.0f && (s->running || s->value >= t->recover_at);
    s->running = in->run && can;
    in->run = s->running;
    if (s->running && v2_length(in->move) > 0.1f) {
      s->value = m_maxf(0.0f, s->value - t->drain_per_s * dt);
      s->since_drain = 0.0f;
    } else {
      s->since_drain += dt;
      if (s->since_drain >= t->regen_delay_s)
        s->value = m_minf(max, s->value + t->regen_per_s * dt);
    }
    s->exhausted = s->value <= 0.0f || (!s->running && s->value < t->recover_at);
  }
}

void stamina_schedule(EcsWorld *w) { ecs_schedule(w, stamina_system); }
