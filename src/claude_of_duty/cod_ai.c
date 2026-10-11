// Claude of Duty AI: navigation, perception, cover, squads, combat and the hit volumes of the soldiers.

// asks for the nav build once the world has settled, then hands the grid to every system
ecs_system(cai_gate_system, EcsResInOut(CodAiNavGate), EcsResInOut(CodAiShared), EcsResIn(CodWorld), EcsResIn(EcsPhysicsWorld),
           EcsResIn(EcsNavigationWorld)) {
  cai_nav_gate(it->world);
  const CodAiNavGate *g = ecs_sys_get_resource(CodAiNavGate);
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  if (g->stage == CAI_NAV_READY && !sh->nav)
    sh->nav = g->nav;
  // the navmesh class is ready once every tile over the built world has been built and none is dirty
  const EcsNavigationWorld *nav = ecs_sys_get_resource(EcsNavigationWorld);
  b32 ready = false;
  if (ecs_sys_get_resource(CodWorld)->ready) {
    for (i32 i = 0; i < nav->class_count; i++)
      if (nav->classes[i].key.radius == sh->nav_key.radius && nav->classes[i].key.height == sh->nav_key.height)
        ready = nav->classes[i].ready;
  }
  sh->navmesh_ready = ready;
}

// the soldiers stage of the load gate: the nav build and the body's assets, whichever is behind
ecs_system(cai_load_system, EcsResIn(CodAiNavGate), EcsResIn(CodAiShared), EcsResIn(CodBodyProgress), EcsResInOut(CodAiState), EcsResInOut(CodLoad)) {
  const CodAiNavGate *g = ecs_sys_get_resource(CodAiNavGate);
  CodAiState *st = ecs_sys_get_resource_mut(CodAiState);
  b32 ready = g->stage == CAI_NAV_READY && ecs_sys_get_resource(CodAiShared)->navmesh_ready;
  if (ready && !st->ready) {
    st->walkable = g->nav->walkable;
    st->cover_points = g->nav->cover_count;
    st->nav_ms = g->nav->grid_ms + g->nav->cover_ms;
  }
  st->ready = ready;
  f32 body = ecs_sys_get_resource(CodBodyProgress)->progress;
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_SOLDIERS] = m_minf(ready ? 1.0f : 0.0f, body);
}

void cod_ai_init(EcsWorld *w) {
  ecs_resource_set(w, CodAiState, ((CodAiState){.next_id = 1}));
  ecs_resource_set(w, CodHitboxes, ((CodHitboxes){0}));
  ecs_resource_set_zero(w, CodAiShared);
  CodAiShared *sh = ecs_resource_get_mut(w, CodAiShared);
  sh->rng = cod_rng_make(COD_SEED ^ 0xa1u);
  ecs_resource_set(w, CodAiNavGate, ((CodAiNavGate){.arena = arena_create(MB(64), MB(4))}));

  // the soldiers' routes: the engine navmesh over the world's static colliders, one class sized for the largest variant
  aabb bounds = cod_world_level_bounds();
  NavConfig cfg = nav_config_default(bounds);
  cfg.cell_size = CAI_SOLDIER_RADIUS * CAI_SOLDIER_SCALE_MAX / 4.0f; // the capsule radius erodes exactly four cells (erosion rounds up)
  cfg.cell_height = 0.05f;
  cfg.walkable_slope_deg = 46.0f; // the character's 48 degree limit, less a margin
  cfg.agent_climb = 0.45f;        // the 0.42 m plinths, at the character's step
  cfg.tile_size_cells = 48;
  ecs_navigation_init(w, &(EcsNavigationInitDesc){.bounds = bounds, .class_template = cfg, .max_agent_classes = 1});
  sh = ecs_resource_get_mut(w, CodAiShared);
  sh->nav_key = (NavAgentKey){.radius = CAI_SOLDIER_RADIUS * CAI_SOLDIER_SCALE_MAX, .height = CAI_SOLDIER_HEIGHT * CAI_SOLDIER_SCALE_MAX};
  ecs_navigation_register_agent(w, sh->nav_key);
  ecs_resource_set(w, EcsNavigationDebug, ((EcsNavigationDebug){.agent_class = 0, .color_tiles = true, .radius = 40.0f, .lift_m = 0.05f}));

  // the soldiers' hit volumes: bullets query this layer alongside the world
  EcsQuery *hit_q = ecs_query(w, EcsIn(ColliderBody), EcsIn(CodHitbox));
  HzHandleT(CollisionLayerHandle) layer = ecs_physics_register_layer(w, hit_q, COLLISION_LAYER_BUILD_EVERY_FRAME);
  CodHitboxes *hb = ecs_resource_get_mut(w, CodHitboxes);
  mem_cpy(&hb->layer, &layer, sizeof(layer) < sizeof(hb->layer) ? sizeof(layer) : sizeof(hb->layer));

#if COD_EDITOR
  ecs_resource_set(w, CodAiDebug, ((CodAiDebug){.hitboxes = hit_q,
                                                .soldiers = ecs_query_cached(w, EcsIn(CodSoldierBrain)),
                                                .cameras = ecs_query_cached(w, EcsIn(Camera))}));
#endif
}

void cod_ai_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cai_gate_system);         // writes the nav gate and the shared resource
  ecs_schedule_single(w, cai_load_system);         // writes the CodAiState, CodLoad resources
  ecs_schedule_single(w, cai_totals_reset_system); // writes the CodAiState resource
  ecs_schedule_single(w, cai_frame_system);        // writes the shared resource (clock, player, sky)
  ecs_schedule_single(w, cai_camera_system);       // writes the shared resource (frustum)
  ecs_schedule_single(w, cai_switch_system);       // writes the shared resource (dev switches)
  ecs_schedule_single(w, cai_roster_system);       // writes the shared resource (roster)
  ecs_schedule_single(w, cai_events_system);       // writes the shared resource (events, squads, cover claims)
  ecs_schedule_single(w, cai_spawn_system);        // writes the shared and CodAiState resources, spawns soldiers
  ecs_schedule(w, cai_pull_system);
  ecs_schedule(w, cai_brain_system);
  ecs_schedule(w, cai_push_system);
  ecs_schedule(w, cai_publish_system);
  ecs_schedule(w, cai_hitbox_system);
  ecs_schedule_single(w, cai_grenade_system); // spawns grenade bodies (weapons' contract: main lane or a single system)
  ecs_schedule_single(w, cai_intake_system);  // writes the player's inbox
  ecs_schedule_single(w, cai_totals_system);  // writes the CodAiState resource
}

// the soldiers' steps are the engine character's, scheduled once by the player's fixed step
void cod_ai_schedule_fixed(EcsWorld *w) { (void)w; }

void cod_ai_lanes(EcsWorld *w) { cai_nav_build_lanes(w); }

void cod_ai_frame(EcsWorld *w) {
#if COD_EDITOR
  cai_debug_draw(w);
#else
  (void)w;
#endif
}
