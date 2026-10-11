#include "test_clock.h"
#include "lib/thread_context.h"

#define TEST_CLOCK_STEP_DT (1.0f / 60.0f)

// the schedule call's arguments for the system, written by the scheduling lane
ECS_COMPONENT_TYPEDEF(TestClockInput);
typedef struct TestClockInput {
  f32 dt;
  b32 ready;
} TestClockInput;

ecs_system(test_clock_system, EcsIn(TestClock), EcsResIn(TestClockInput), EcsResInOut(TestClockFrame)) {
  const TestClockInput *in = ecs_sys_get_resource(TestClockInput);
  TestClockFrame *f = ecs_sys_get_resource_mut(TestClockFrame);
  u32 steps = ecs_sys_get_component(TestClock, 0)->steps;

  // a capture counts from the first ready frame: step > 0 is the capture running
  if (steps > 0 && f->step == 0 && in->ready) {
    f->time = 0.0f;
    f->frozen = false;
    log_info("capture: % steps of 1/60 s", fmt_u32(steps));
  }

  f32 dt = in->dt;
  if (f->step > 0 || (steps > 0 && in->ready)) {
    b32 stepping = f->step < steps;
    if (stepping)
      f->step++;
    dt = stepping ? TEST_CLOCK_STEP_DT : 0.0f;
    b32 was_frozen = f->frozen;
    f->frozen = !stepping;
    if (f->frozen && !was_frozen)
      log_info("capture frozen at step %", fmt_u32(f->step));
  }
  f->dt = dt;
  f->time += dt;
}

void test_clock_init(EcsWorld *w) {
  ecs_resource_set(w, TestClockInput, {0});
  ecs_resource_set(w, TestClockFrame, {0});
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, TestClock, {0});
}

void test_clock_schedule(EcsWorld *w, f32 dt, b32 ready) {
  // the schedule call runs on the main lane only; the system reads the input after the flush's barrier
  if (is_main_thread()) {
    TestClockInput *in = ecs_resource_get_mut(w, TestClockInput);
    in->dt = dt;
    in->ready = ready;
  }
  ecs_schedule_single(w, test_clock_system);
}
