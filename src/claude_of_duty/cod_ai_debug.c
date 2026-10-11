// Claude of Duty AI debugging: the hit capsules, soldier capsules, paths and cover points drawn as gizmos behind CodDevSwitches.ai_debug.

#if COD_EDITOR
typedef struct CodAiDebug {
  EcsQuery *hitboxes; // ColliderBody + CodHitbox
  EcsQuery *soldiers; // CodSoldierBrain
  EcsQuery *cameras;
} CodAiDebug;
ECS_COMPONENT_TYPEDEF(CodAiDebug);

hz_internal ColorF32 cai_zone_color(u32 zone) {
  switch (zone) {
  case COD_ZONE_HEAD: return (ColorF32){1.0f, 0.2f, 0.2f, 1.0f};
  case COD_ZONE_TORSO: return (ColorF32){1.0f, 0.75f, 0.1f, 1.0f};
  case COD_ZONE_PELVIS: return (ColorF32){1.0f, 0.55f, 0.1f, 1.0f};
  case COD_ZONE_ARM: return (ColorF32){0.2f, 0.8f, 1.0f, 1.0f};
  default: return (ColorF32){0.3f, 1.0f, 0.4f, 1.0f};
  }
}

hz_internal ColorF32 cai_mode_color(u32 mode) {
  switch (mode) {
  case COD_AI_IDLE: return (ColorF32){0.7f, 0.7f, 0.7f, 1.0f};
  case COD_AI_PATROL: return (ColorF32){0.3f, 0.9f, 0.4f, 1.0f};
  case COD_AI_ALERT: return (ColorF32){1.0f, 0.9f, 0.2f, 1.0f};
  case COD_AI_COMBAT: return (ColorF32){1.0f, 0.2f, 0.2f, 1.0f};
  case COD_AI_SUPPRESSED: return (ColorF32){0.8f, 0.3f, 0.9f, 1.0f};
  case COD_AI_FLANK: return (ColorF32){0.2f, 0.7f, 1.0f, 1.0f};
  case COD_AI_RETREAT: return (ColorF32){1.0f, 0.5f, 0.1f, 1.0f};
  default: return (ColorF32){0.3f, 0.3f, 0.3f, 1.0f};
  }
}

// a capsule between two points as the gizmo's centre, rotation and tip-to-tip height
hz_internal void cai_draw_capsule(GizmoSystem *g, v3 a, v3 b, f32 radius, ColorF32 color) {
  v3 d = v3_sub(b, a);
  f32 len = v3_length(d);
  quat rot = len > 1e-5f ? quat_from_to((v3){0.0f, 1.0f, 0.0f}, v3_scale(d, 1.0f / len)) : quat_identity();
  gizmos_draw_capsule(g, v3_scale(v3_add(a, b), 0.5f), rot, len + radius * 2.0f, radius, color, true);
}

// main lane, after the scope: queues the debug shapes and prepares the gizmos (the editor prepares them itself while it is open)
hz_internal void cai_debug_draw(EcsWorld *w) {
  const CodAiShared *sh = ecs_resource_get(w, CodAiShared);
  if (!sh || sh->debug <= 0 || ecs_resource_get(w, CodEditor)->open)
    return;
  GizmoSystem *g = ecs_resource_get_mut(w, GizmoSystem);
  const CodAiDebug *dbg = ecs_resource_get(w, CodAiDebug);
  if (!g || !dbg)
    return;
  const Camera *cam = ecs_query_get_singleton(Camera, dbg->cameras);
  const AppMemory *mem = ecs_resource_get(w, AppMemory);
  if (!cam || !mem)
    return;

  ecs_query_foreach(dbg->hitboxes, qit) {
    const ColliderBody *cb = ecs_iter_get_component(&qit, ColliderBody, 0);
    const CodHitbox *hb = ecs_iter_get_component(&qit, CodHitbox, 1);
    for (i32 i = 0; i < qit.count; i++) {
      if (cb[i].collider.type != ColliderType_Capsule)
        continue;
      v3 a = v3_add(cb[i].pos, cb[i].collider.capsule.point_a), b = v3_add(cb[i].pos, cb[i].collider.capsule.point_b);
      cai_draw_capsule(g, a, b, cb[i].collider.capsule.radius, cai_zone_color(hb[i].zone));
    }
  }
  ecs_query_foreach(dbg->soldiers, qit) {
    const CodSoldierBrain *br = ecs_iter_get_component(&qit, CodSoldierBrain, 0);
    for (i32 i = 0; i < qit.count; i++) {
      const CodSoldierBrain *b = &br[i];
      ColorF32 col = cai_mode_color(b->mode);
      v3 top = v3_add(b->position, (v3){0.0f, b->height, 0.0f});
      gizmos_draw_line(g, b->position, top, col, 2.0f, false);
      if (!b->alive)
        continue;
      v3 fwd = {m_sinf(b->yaw), 0.0f, m_cosf(b->yaw)};
      gizmos_draw_line(g, v3_add(b->position, (v3){0.0f, 1.3f, 0.0f}), v3_add(b->position, v3_add((v3){0.0f, 1.3f, 0.0f}, v3_scale(fwd, 1.2f))), col, 2.0f, false);
      v3 from = b->position;
      for (u32 k = b->path_index; k < b->path_len && b->has_move_target; k++) {
        gizmos_draw_line(g, v3_add(from, (v3){0.0f, 0.1f, 0.0f}), v3_add(b->path[k], (v3){0.0f, 0.1f, 0.0f}), col, 1.0f, false);
        from = b->path[k];
      }
      if (b->cover >= 0 && sh->nav && (u32)b->cover < sh->nav->cover_count)
        gizmos_draw_line(g, b->position, sh->nav->cover[b->cover].pos, (ColorF32){0.2f, 0.6f, 1.0f, 1.0f}, 1.0f, false);
    }
  }
  if (sh->debug >= 2 && sh->nav && sh->nav->ready) {
    for (u32 i = 0; i < sh->nav->cover_count; i++) {
      const CaiCover *c = &sh->nav->cover[i];
      gizmos_draw_point(g, v3_add(c->pos, (v3){0.0f, c->high ? 1.2f : 0.5f, 0.0f}), 4.0f,
                        c->high ? (ColorF32){1.0f, 0.6f, 0.2f, 1.0f} : (ColorF32){0.4f, 0.8f, 1.0f, 1.0f}, false);
    }
  }
  gizmos_prepare(g, &(GizmoFlushDesc){.camera = cam, .line_width = 2.0f, .screen_width = mem->canvas_width, .screen_height = mem->canvas_height});
}
#endif
