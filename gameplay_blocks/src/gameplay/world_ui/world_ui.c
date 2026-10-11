#include "world_ui.h"
#include "lib/assert.h"
#include "lib/random.h"
#include "lib/string_builder.h"
#include "lib/thread_context.h"
#include "ui/ui.h"

// -----------------------------------------------------------------------------
// Block 1: BillboardGui
// -----------------------------------------------------------------------------

BillboardGui billboard_create(UISystem *ui, RendererState *r, const BillboardGuiDesc *desc) {
  UISurface *surface = ui_surface_create(
      ui, r,
      &(UISurfaceDesc){
          .canvas_w = desc->canvas_w,
          .canvas_h = desc->canvas_h,
          .pixels_per_unit = desc->pixels_per_unit,
          .mode = UI_SURFACE_BILLBOARD,
          .position = V3_ZERO,
          .rotation = quat_identity(),
          .scale = 1.0f,
          .clear_color = desc->clear_color,
          .always_on_top = desc->always_on_top,
      });
  return (BillboardGui){
      .surface = surface,
      .size_mode = desc->size_mode,
      .pixel_height = desc->pixel_height,
      .max_distance = desc->max_distance,
      .off_world = desc->studs_offset_world,
      .off_camera = desc->studs_offset_camera,
      .visible = true,
  };
}

void billboard_destroy(UISystem *ui, RendererState *r, BillboardGui *bg) {
  ui_surface_destroy(ui, r, bg->surface);
  bg->surface = NULL;
}

b32 billboard_place(BillboardGui *bg, const Camera *cam, u32 screen_h_px, v3 anchor) {
  v3 right = camera_right(cam);
  v3 up = camera_up(cam);
  v3 fwd = camera_forward(cam);

  v3 world_anchor = v3_add(anchor, bg->off_world);
  world_anchor = v3_add(world_anchor, v3_scale(right, bg->off_camera.x));
  world_anchor = v3_add(world_anchor, v3_scale(up, bg->off_camera.y));
  world_anchor = v3_add(world_anchor, v3_scale(fwd, bg->off_camera.z));

  v3 to_anchor = v3_sub(world_anchor, cam->pos);
  f32 dist = v3_length(to_anchor);
  f32 depth = v3_dot(to_anchor, fwd);

  // BEHIND the camera is not a position, it is a cull. Without this a
  // billboard whose anchor is behind you stays "visible" and gets placed from
  // a negative depth, which pins it to a screen edge where it reads as a HUD
  // panel that no game drew. One run lost two build cycles to a gate sign that
  // appeared to be part of its own HUD.
  if (depth <= 0.05f) {
    bg->visible = false;
    ui_surface_set_visible(bg->surface, false);
    return false;
  }
  if (bg->max_distance > 0.0f && dist > bg->max_distance) {
    bg->visible = false;
    ui_surface_set_visible(bg->surface, false);
    return false;
  }

  bg->visible = true;
  ui_surface_set_visible(bg->surface, true);

  f32 scale = 1.0f;
  if (bg->size_mode == BILLBOARD_SIZE_PIXELS) {
    f32 h = (f32)(screen_h_px > 0 ? screen_h_px : 1);
    f32 frac = bg->pixel_height / h;
    f32 tbounds[4];
    camera_effective_tangents(cam, cam->aspect > 0 ? cam->aspect : 1.0f,
                              tbounds);
    f32 world_h_want = frac * depth * (tbounds[3] - tbounds[2]);
    scale = world_h_want / bg->surface->world_h;
  }

  ui_surface_set_transform(bg->surface, world_anchor, quat_identity(), scale);
  return true;
}

// -----------------------------------------------------------------------------
// module-owned runtime side table for the SurfaceGui blocks (non-reflected)
// -----------------------------------------------------------------------------

#define WORLD_UI_MAX_SURFACE_GUIS 64

typedef struct {
  b32 in_use;
  EcsEntity entity;
  UISurface *surface;
  SurfaceFace face;
} SurfaceGuiRuntimeSlot;

ECS_COMPONENT_TYPEDEF(EcsWorldUiState);
typedef struct EcsWorldUiState {
  UISystem *ui;
  RendererState *r;
  HzHandleT(UIFontData) default_font; // surface_gui_text_draw's font (Block 3b)
  SurfaceGuiRuntimeSlot surfaces[WORLD_UI_MAX_SURFACE_GUIS];
} EcsWorldUiState;

hz_internal SurfaceGuiRuntimeSlot *surface_gui_slot_find(EcsWorldUiState *st, EcsEntity e) {
  for (u32 i = 0; i < WORLD_UI_MAX_SURFACE_GUIS; i++) {
    if (st->surfaces[i].in_use && st->surfaces[i].entity == e)
      return &st->surfaces[i];
  }
  return NULL;
}

hz_internal SurfaceGuiRuntimeSlot *surface_gui_slot_alloc(EcsWorldUiState *st) {
  for (u32 i = 0; i < WORLD_UI_MAX_SURFACE_GUIS; i++) {
    if (!st->surfaces[i].in_use)
      return &st->surfaces[i];
  }
  assert_msg(false, "world_ui: surface_gui pool exhausted (WORLD_UI_MAX_SURFACE_GUIS)");
  return NULL;
}

hz_internal EcsWorldUiState *world_ui_state(EcsWorld *w) {
  EcsWorldUiState *st = ecs_resource_get_mut(w, EcsWorldUiState);
  assert_msg(st, "world_ui: surface_gui_init not called");
  return st;
}

// -----------------------------------------------------------------------------
// Block 3: SurfaceGui
// -----------------------------------------------------------------------------

hz_internal quat surface_gui_basis_to_rotation(v3 right, v3 normal) {
  v3 up = v3_cross(normal, right);
  m3 basis = {{
      {right.x, right.y, right.z},
      {normal.x, normal.y, normal.z},
      {-up.x, -up.y, -up.z},
  }};
  return quat_from_m3(basis);
}

quat surface_gui_face_rotation(SurfaceFace face) {
  switch (face) {
  case FACE_TOP:
    return surface_gui_basis_to_rotation((v3){0, 0, -1}, (v3){0, 1, 0});
  case FACE_BOTTOM:
    return surface_gui_basis_to_rotation((v3){0, 0, -1}, (v3){0, -1, 0});
  case FACE_FRONT:
    return surface_gui_basis_to_rotation((v3){1, 0, 0}, (v3){0, 0, 1});
  case FACE_BACK:
    return surface_gui_basis_to_rotation((v3){-1, 0, 0}, (v3){0, 0, -1});
  case FACE_LEFT:
    return surface_gui_basis_to_rotation((v3){0, 0, 1}, (v3){-1, 0, 0});
  case FACE_RIGHT:
  default:
    return surface_gui_basis_to_rotation((v3){0, 0, -1}, (v3){1, 0, 0});
  }
}

void surface_gui_init(EcsWorld *w, UISystem *ui, RendererState *r) {
  if (!ecs_resource_get(w, EcsWorldUiState)) {
    ecs_resource_set_zero(w, EcsWorldUiState);
  }
  EcsWorldUiState *st = ecs_resource_get_mut(w, EcsWorldUiState);
  st->ui = ui;
  st->r = r;
  st->default_font = ui_default_font(ui);
}

void surface_gui_add(EcsWorld *w, EcsEntity e, const SurfaceGuiDesc *desc) {
  assert_msg(ecs_component_exists(w, e, WorldTransform),
             "surface_gui_add: entity needs WorldTransform -- the adornee the panel follows");
  EcsWorldUiState *st = world_ui_state(w);
  SurfaceGuiRuntimeSlot *slot = surface_gui_slot_alloc(st);
  slot->in_use = true;
  slot->entity = e;
  slot->face = desc->face;
  slot->surface = ui_surface_create(
      st->ui, st->r,
      &(UISurfaceDesc){
          .canvas_w = (u32)(desc->face_w_studs * desc->pixels_per_stud),
          .canvas_h = (u32)(desc->face_h_studs * desc->pixels_per_stud),
          .pixels_per_unit = desc->pixels_per_stud,
          .mode = UI_SURFACE_SURFACE,
          .position = V3_ZERO,
          .rotation = quat_identity(),
          .scale = 1.0f,
          .clear_color = desc->clear_color,
          .always_on_top = desc->always_on_top,
      });

  ecs_component_set(w, e, SurfaceGui,
                    {.face = desc->face,
                     .pixels_per_stud = desc->pixels_per_stud,
                     .face_w_studs = desc->face_w_studs,
                     .face_h_studs = desc->face_h_studs,
                     .always_on_top = desc->always_on_top,
                     .clear_color = desc->clear_color});
}

void surface_gui_adopt(EcsWorld *w) {
  EcsWorldUiState *st = world_ui_state(w);
  EcsQuery *q = ecs_query(w, EcsIn(SurfaceGui));
  ecs_query_foreach(q, it) {
    for (i32 i = 0; i < it.count; i++) {
      EcsEntity e = it.entities[i];
      if (surface_gui_slot_find(st, e))
        continue;
      const SurfaceGui *sg = ecs_iter_get_component(&it, SurfaceGui, 0);
      SurfaceGuiRuntimeSlot *slot = surface_gui_slot_alloc(st);
      slot->in_use = true;
      slot->entity = e;
      slot->face = sg[i].face;
      slot->surface = ui_surface_create(
          st->ui, st->r,
          &(UISurfaceDesc){
              .canvas_w = (u32)(sg[i].face_w_studs * sg[i].pixels_per_stud),
              .canvas_h = (u32)(sg[i].face_h_studs * sg[i].pixels_per_stud),
              .pixels_per_unit = sg[i].pixels_per_stud,
              .mode = UI_SURFACE_SURFACE,
              .position = V3_ZERO,
              .rotation = quat_identity(),
              .scale = 1.0f,
              .clear_color = sg[i].clear_color,
              .always_on_top = sg[i].always_on_top,
          });
    }
  }
}

void surface_gui_destroy(EcsWorld *w, EcsEntity e) {
  EcsWorldUiState *st = world_ui_state(w);
  SurfaceGuiRuntimeSlot *slot = surface_gui_slot_find(st, e);
  if (!slot)
    return;
  ui_surface_destroy(st->ui, st->r, slot->surface);
  slot->in_use = false;
  slot->entity = 0;
  slot->surface = NULL;
  ecs_component_remove(w, e, SurfaceGui);
}

void surface_gui_update(EcsWorld *w, const Camera *cam) {
  (void)cam;
  EcsWorldUiState *st = world_ui_state(w);
  for (u32 i = 0; i < WORLD_UI_MAX_SURFACE_GUIS; i++) {
    SurfaceGuiRuntimeSlot *slot = &st->surfaces[i];
    if (!slot->in_use)
      continue;
    const WorldTransform *xf = ecs_component_get(w, slot->entity, WorldTransform);
    if (!xf)
      continue;
    quat face_rot = surface_gui_face_rotation(slot->face);
    quat world_rot = quat_mul(xf->rotation, face_rot);
    ui_surface_set_transform(slot->surface, xf->position, world_rot, 1.0f);
  }
}

UISurface *surface_gui_get_surface(EcsWorld *w, EcsEntity e) {
  EcsWorldUiState *st = world_ui_state(w);
  SurfaceGuiRuntimeSlot *slot = surface_gui_slot_find(st, e);
  return slot ? slot->surface : NULL;
}

// -----------------------------------------------------------------------------
// Block 3b: SurfaceGuiText
// -----------------------------------------------------------------------------

#define WORLD_UI_LABEL_FIXED_FONT_SIZE 40.0f
#define WORLD_UI_LABEL_SCALED_FRACTION 0.7f

void surface_gui_text_draw(EcsWorld *w) {
  EcsWorldUiState *st = world_ui_state(w);
  EcsQuery *q = ecs_query(w, EcsIn(SurfaceGui), EcsIn(SurfaceGuiText));
  ecs_query_foreach(q, it) {
    for (i32 i = 0; i < it.count; i++) {
      EcsEntity e = it.entities[i];
      SurfaceGuiRuntimeSlot *slot = surface_gui_slot_find(st, e);
      if (!slot)
        continue;
      const SurfaceGui *sg = ecs_iter_get_component(&it, SurfaceGui, 0);
      const SurfaceGuiText *lbl = ecs_iter_get_component(&it, SurfaceGuiText, 1);
      f32 canvas_h = sg[i].face_h_studs * sg[i].pixels_per_stud;
      f32 font_size = lbl[i].text_scaled ? canvas_h * WORLD_UI_LABEL_SCALED_FRACTION
                                          : WORLD_UI_LABEL_FIXED_FONT_SIZE;
      HzUIString str = {.chars = lbl[i].text.value, .length = (i32)lbl[i].text.len};
      ColorF32 c = lbl[i].text_color;
      ui_surface_scope(st->ui, slot->surface) {
        ui_element({
            .id = ui_idi("SurfaceGuiTextPanel", (u32)e),
            .layout = {.sizing = {.width = ui_sizing_grow(), .height = ui_sizing_grow()},
                      .child_alignment = {.x = UI_ALIGN_X_CENTER, .y = UI_ALIGN_Y_CENTER}},
        }) {
          ui_text(str, ui_text_config({.fontId = (u16)st->default_font.idx,
                                       .fontSize = font_size,
                                       .textColor = {(u8)(c.r * 255.0f), (u8)(c.g * 255.0f),
                                                     (u8)(c.b * 255.0f), 255}}));
        }
      }
    }
  }
}


// -----------------------------------------------------------------------------
// Block 2: WorldUi
// -----------------------------------------------------------------------------

#define WORLD_UI_POPUP_DEFAULT_LIFE 1.3f
#define WORLD_UI_POPUP_DEFAULT_FADE_FROM 0.545f
#define WORLD_UI_PROMPT_DEFAULT_RANGE 10.0f
#define WORLD_UI_OCCLUSION_DEFAULT_EVERY 3u
#define WORLD_UI_OCCLUSION_DEFAULT_MARGIN 0.02f
#define WORLD_UI_STACK_MAX 256

void world_ui_init(EcsWorld *w, const WorldUiDesc *desc) {
  WorldUiDesc d = desc ? *desc : (WorldUiDesc){0};
  if (d.occlusion_every == 0)
    d.occlusion_every = WORLD_UI_OCCLUSION_DEFAULT_EVERY;
  if (d.occlusion_margin <= 0.0f)
    d.occlusion_margin = WORLD_UI_OCCLUSION_DEFAULT_MARGIN;
  ecs_resource_set(w, WorldUiSettings, {.desc = d});
  ecs_resource_set_zero(w, WorldUiView);
}

void world_ui_add(EcsWorld *w, EcsEntity e, const WorldUi *ui, const WorldUiLabel *label) {
  WorldUiLabel l = *label;
  l.fill_prev = l.fill;
  l.fill_age = 1.0e9f;
  ecs_component_set(w, e, WorldUi, *ui);
  ecs_component_set(w, e, WorldUiLabel, l);
  ecs_component_set(w, e, WorldUiScreen, {0});
}

EcsEntity world_ui_popup(EcsWorld *w, const WorldUiPopupDesc *d) {
  EcsEntity e = ecs_entity_create(w);
  v3 at = d->at;
  if (d->scatter > 0.0f) {
    Random rng = random_create((u64)e | 1u);
    f32 s = d->scatter;
    at = (v3){at.x + random_f32_range(&rng, -s, s), at.y + random_f32_range(&rng, -s * 0.6f, s),
              at.z + random_f32_range(&rng, -s, s)};
  }
  WorldUi ui = {.kind = WORLD_UI_POPUP,
                .offset = at,
                .screen_anchored = d->screen_anchored,
                .screen_frac = d->screen_frac,
                .screen_px = d->screen_px,
                .stack_group = d->stack_group,
                .stack_size = d->stack_size};
  WorldUiLabel label = {.text = d->text, .color = d->color};
  WorldUiPopup popup = {.life = d->life > 0.0f ? d->life : WORLD_UI_POPUP_DEFAULT_LIFE,
                        .rise_world = d->rise_world,
                        .drift_px = d->drift_px,
                        .enter_px = d->enter_px,
                        .enter_frac = d->enter_frac,
                        .fade_from = d->fade_from > 0.0f ? d->fade_from : WORLD_UI_POPUP_DEFAULT_FADE_FROM,
                        .scale_from = d->scale_from > 0.0f ? d->scale_from : 1.0f};
  ecs_component_set(w, e, WorldUi, ui);
  ecs_component_set(w, e, WorldUiLabel, label);
  ecs_component_set(w, e, WorldUiScreen, {0});
  ecs_component_set(w, e, WorldUiPopup, popup);
  return e;
}

const WorldUiScreen *world_ui_screen(EcsWorld *w, EcsEntity e) { return ecs_component_get(w, e, WorldUiScreen); }

// a scene-authored widget gets its runtime state one flush after it appears
ecs_system(world_ui_ensure_system, EcsIn(WorldUi), EcsNot(WorldUiScreen)) {
  for (i32 i = 0; i < it->count; i++)
    ecs_component_set(it->world, it->entities[i], WorldUiScreen, {0});
}

ecs_system(world_ui_popup_system, EcsInOut(WorldUiPopup)) {
  for (i32 i = 0; i < it->count; i++) {
    WorldUiPopup *p = ecs_sys_get_component_mut(WorldUiPopup, i);
    p->age += it->delta_time;
    if (p->age >= p->life)
      ecs_entity_destroy(it->world, it->entities[i]);
  }
}

// the scene camera's matrices (the last Camera row wins) and the UI's logical canvas
ecs_system(world_ui_view_system, EcsIn(Camera), EcsResInOut(WorldUiView)) {
  WorldUiView *view = ecs_sys_get_resource_mut(WorldUiView);
  for (i32 i = 0; i < it->count; i++) {
    const Camera *cam = ecs_sys_get_component(Camera, i);
    view->view_proj = cam->view_proj;
    view->cam_pos = cam->pos;
    view->valid = true;
  }
  ScreenDesc screen = os_get_screen_desc();
  view->screen = (v2){(f32)screen.width, (f32)screen.height};
  view->frame++;
}

ecs_system(world_ui_occluder_system, EcsResInOut(WorldUiView), EcsResIn(EcsPhysicsWorld), EcsResIn(WorldUiSettings)) {
  WorldUiView *view = ecs_sys_get_resource_mut(WorldUiView);
  const EcsPhysicsWorld *phys = ecs_sys_get_resource(EcsPhysicsWorld);
  const WorldUiSettings *st = ecs_sys_get_resource(WorldUiSettings);
  HzHandleT(CollisionLayerHandle) layer = handle_is_valid(st->desc.occluder_layer) ? st->desc.occluder_layer : phys->static_layer;
  const EcsCollisionLayerEntry *entry = ha_get(&phys->layers, HANDLE_CAST(HzHandle, layer), EcsCollisionLayerEntry);
  view->occluders = entry && entry->layer.layer.count > 0 ? (CollisionLayer *)&entry->layer.layer : NULL;
}

hz_internal f32 world_ui_ease_out_quad(f32 t) { return 1.0f - (1.0f - t) * (1.0f - t); }

// anchor -> logical px; a point behind the camera is mirrored through the centre so its direction stays meaningful
hz_internal void world_ui_project(const WorldUiView *view, const WorldUi *u, v3 anchor, WorldUiScreen *s) {
  v4 clip = m4_mulv4(view->view_proj, (v4){anchor.x, anchor.y, anchor.z, 1.0f});
  s->in_front = clip.w > 0.0f;
  s->distance = v3_length(v3_sub(anchor, view->cam_pos));
  f32 xy[2];
  camera_project_to_screen(view->view_proj, anchor, view->screen.x, view->screen.y, xy);
  v2 centre = v2_scale(view->screen, 0.5f);
  v2 pos = {xy[0], xy[1]};
  if (!s->in_front)
    pos = (v2){2.0f * centre.x - pos.x, 2.0f * centre.y - pos.y};
  s->on_screen = s->in_front && pos.x >= 0.0f && pos.x <= view->screen.x && pos.y >= 0.0f && pos.y <= view->screen.y;
  s->clamped = false;
  v2 dir = v2_sub(pos, centre);
  s->angle = m_atan2f(dir.y, dir.x);
  if (u->clamp_edge && !s->on_screen) {
    f32 hx = centre.x - u->edge_margin, hy = centre.y - u->edge_margin;
    f32 ax = m_absf(dir.x), ay = m_absf(dir.y);
    if (ax < 1e-4f && ay < 1e-4f) {
      dir = (v2){0.0f, 1.0f};
      ay = 1.0f;
    }
    f32 k = m_minf(ax > 1e-4f ? hx / ax : 1e30f, ay > 1e-4f ? hy / ay : 1e30f);
    pos = v2_add(centre, v2_scale(dir, k));
    s->clamped = true;
  }
  s->pos = pos;
}

// per widget, parallel: anchor -> screen, distance cull, staggered occlusion ray, ON_HIT fill clock, popup age
ecs_system(world_ui_project_system, EcsIn(WorldUi), EcsInOut(WorldUiLabel), EcsInOut(WorldUiScreen),
           EcsOptional(WorldTransform), EcsOptional(WorldUiPopup), EcsResIn(WorldUiView), EcsResIn(WorldUiSettings)) {
  const WorldUiView *view = ecs_sys_get_resource(WorldUiView);
  const WorldUiSettings *st = ecs_sys_get_resource(WorldUiSettings);
  for (i32 i = 0; i < it->count; i++) {
    const WorldUi *u = ecs_sys_get_component(WorldUi, i);
    if (!view->valid && !u->screen_anchored)
      continue;
    WorldUiLabel *lab = ecs_sys_get_component_mut(WorldUiLabel, i);
    WorldUiScreen *s = ecs_sys_get_component_mut(WorldUiScreen, i);
    const WorldTransform *xf = ecs_sys_get_component_optional(WorldTransform, i);
    const WorldUiPopup *pop = ecs_sys_get_component_optional(WorldUiPopup, i);

    if (lab->fill < lab->fill_prev - 1.0e-4f)
      lab->fill_age = 0.0f;
    else
      lab->fill_age += it->delta_time;
    lab->fill_prev = lab->fill;

    if (u->screen_anchored) {
      s->pos = (v2){view->screen.x * u->screen_frac.x + u->screen_px.x, view->screen.y * u->screen_frac.y + u->screen_px.y};
      s->distance = 0.0f;
      s->in_front = true;
      s->on_screen = true;
      s->clamped = false;
      s->occluded = false;
      s->visible = true;
      continue;
    }

    v3 anchor = v3_add(xf ? xf->position : V3_ZERO, u->offset);
    if (pop && pop->rise_world != 0.0f && pop->life > 0.0f)
      anchor.y += pop->rise_world * world_ui_ease_out_quad(m_saturatef(pop->age / pop->life));
    world_ui_project(view, u, anchor, s);

    b32 visible = (s->in_front || u->clamp_edge) && !(u->max_distance > 0.0f && s->distance > u->max_distance);
    if (u->occlude && view->occluders && s->in_front && !s->clamped) {
      if (!s->occ_checked || (view->frame + (u32)it->entities[i]) % st->desc.occlusion_every == 0) {
        f32 margin = st->desc.occlusion_margin;
        s->occluded = false;
        if (s->distance > margin) {
          v3 to = v3_sub(anchor, view->cam_pos);
          v3 end = v3_add(view->cam_pos, v3_scale(to, (s->distance - margin) / s->distance));
          PhysicsRaycastResult hit;
          i32 body = -1;
          s->occluded = physics_raycast_layer_any(view->cam_pos, end, view->occluders, PHYSICS_QUERY_FILTER_ALL, &hit, &body);
        }
        s->occ_checked = true;
      }
    } else {
      s->occluded = false;
      s->occ_checked = false;
    }
    s->visible = visible && !s->occluded;
  }
}

typedef struct WorldUiStackItem {
  WorldUiScreen *s;
  const WorldUi *u;
  f32 order; // older popups first, then by entity
  u64 entity;
} WorldUiStackItem;

hz_internal b32 world_ui_stack_before(const WorldUiStackItem *a, const WorldUiStackItem *b) {
  if (a->u->stack_group != b->u->stack_group)
    return a->u->stack_group < b->u->stack_group;
  if (a->order != b->order)
    return a->order > b->order;
  return a->entity < b->entity;
}

// groups of widgets whose boxes overlap push each other up; a widget keeps where it was pushed unless stack_relax settles it back
hz_internal void world_ui_stack(WorldUiStackItem *items, u32 n, f32 dt) {
  for (u32 i = 1; i < n; i++) {
    WorldUiStackItem key = items[i];
    u32 j = i;
    while (j > 0 && world_ui_stack_before(&key, &items[j - 1])) {
      items[j] = items[j - 1];
      j--;
    }
    items[j] = key;
  }
  u32 group_start = 0;
  for (u32 i = 0; i < n; i++) {
    if (items[i].u->stack_group != items[group_start].u->stack_group)
      group_start = i;
    WorldUiScreen *s = items[i].s;
    const WorldUi *u = items[i].u;
    f32 dy = m_minf(0.0f, s->stack_dy + u->stack_relax * dt);
    f32 half_w = u->stack_size.x * 0.5f;
    for (u32 pass = 0; pass <= i - group_start; pass++) {
      b32 moved = false;
      for (u32 k = group_start; k < i; k++) {
        const WorldUiScreen *o = items[k].s;
        f32 o_half = items[k].u->stack_size.x * 0.5f;
        if (m_absf(o->pos.x - s->pos.x) >= o_half + half_w)
          continue;
        f32 bottom = s->pos.y + dy, top = bottom - u->stack_size.y;
        f32 o_bottom = o->pos.y + o->stack_dy, o_top = o_bottom - items[k].u->stack_size.y;
        if (bottom <= o_top || top >= o_bottom)
          continue;
        dy = o_top - s->pos.y;
        moved = true;
      }
      if (!moved)
        break;
    }
    s->stack_dy = dy;
  }
}

// one task after the projection: the nearest prompt, key edges, eased visibility and stacking
ecs_system(world_ui_consolidate_system, EcsResInOut(WorldUiView), EcsResIn(InputSystem)) {
  EcsWorld *w = it->world;
  const InputSystem *input = ecs_sys_get_resource(InputSystem);
  f32 dt = it->delta_time;

  EcsQuery *iq = ecs_query(w, EcsIn(WorldUiInteractor), EcsIn(WorldTransform));
  b32 have_interactor = false;
  v3 interactor = V3_ZERO;
  ecs_query_foreach(iq, iit) {
    if (iit.count > 0 && !have_interactor) {
      interactor = ecs_iter_get_component(&iit, WorldTransform, 1)[0].position;
      have_interactor = true;
    }
  }

  EcsQuery *q = ecs_query(w, EcsIn(WorldUi), EcsInOut(WorldUiScreen), EcsOptional(WorldUiPopup), EcsOptional(WorldUiPrompt),
                          EcsOptional(WorldTransform));
  WorldUiScreen *nearest = NULL;
  const WorldUiPrompt *nearest_prompt = NULL;
  f32 nearest_d = 1.0e30f;
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  WorldUiStackItem *items = ALLOC_ARRAY(&ta.allocator, WorldUiStackItem, WORLD_UI_STACK_MAX);
  u32 item_count = 0;
  ecs_query_foreach(q, qit) {
    const WorldUi *us = ecs_iter_get_component(&qit, WorldUi, 0);
    WorldUiScreen *ss = ecs_iter_get_component(&qit, WorldUiScreen, 1);
    const WorldUiPrompt *prompts = ecs_iter_get_component(&qit, WorldUiPrompt, 3);
    const WorldTransform *xfs = ecs_iter_get_component(&qit, WorldTransform, 4);
    for (i32 i = 0; i < qit.count; i++) {
      WorldUiScreen *s = &ss[i];
      s->triggered = false;
      s->active = false;
      if (prompts && us[i].kind == WORLD_UI_PROMPT && prompts[i].enabled && s->visible &&
          (have_interactor || us[i].screen_anchored)) {
        f32 d = 0.0f;
        f32 range = 1.0f;
        if (!us[i].screen_anchored) {
          v3 at = v3_add(xfs ? xfs[i].position : V3_ZERO, us[i].offset);
          f32 dx = at.x - interactor.x, dz = at.z - interactor.z;
          d = m_sqrtf(dx * dx + dz * dz);
          range = prompts[i].range > 0.0f ? prompts[i].range : WORLD_UI_PROMPT_DEFAULT_RANGE;
        }
        if (d <= range && d < nearest_d) {
          nearest_d = d;
          nearest = s;
          nearest_prompt = &prompts[i];
        }
      }
    }
  }
  if (nearest) {
    nearest->active = true;
    nearest->triggered = nearest_prompt->key < KEY_MAX && input->buttons[nearest_prompt->key].pressed_this_frame;
  }
  ecs_query_foreach(q, qit) {
    const WorldUi *us = ecs_iter_get_component(&qit, WorldUi, 0);
    WorldUiScreen *ss = ecs_iter_get_component(&qit, WorldUiScreen, 1);
    const WorldUiPopup *pops = ecs_iter_get_component(&qit, WorldUiPopup, 2);
    for (i32 i = 0; i < qit.count; i++) {
      WorldUiScreen *s = &ss[i];
      b32 visible = s->visible && (us[i].kind != WORLD_UI_PROMPT || s->active);
      s->visible = visible;
      f32 target = visible ? 1.0f : 0.0f;
      if (us[i].fade_s <= 0.0f)
        s->alpha = target;
      else
        s->alpha = s->alpha < target ? m_minf(target, s->alpha + dt / us[i].fade_s) : m_maxf(target, s->alpha - dt / us[i].fade_s);
      if (us[i].stack_group != 0 && s->alpha > 0.0f && item_count < WORLD_UI_STACK_MAX)
        items[item_count++] = (WorldUiStackItem){.s = s, .u = &us[i], .order = pops ? pops[i].age : 0.0f, .entity = qit.entities[i]};
      else if (us[i].stack_group == 0)
        s->stack_dy = 0.0f;
    }
  }
  world_ui_stack(items, item_count, dt);
  tctx_temp_allocator_end(ta);
}

void world_ui_schedule(EcsWorld *w) {
  const WorldUiSettings *st = ecs_resource_get(w, WorldUiSettings);
  assert_msg(st, "world_ui_schedule: world_ui_init not called");
  ecs_schedule(w, world_ui_ensure_system);
  ecs_schedule(w, world_ui_popup_system);
  ecs_schedule_single(w, world_ui_view_system);
  if (st->desc.occlusion)
    ecs_schedule_single(w, world_ui_occluder_system);
  ecs_schedule(w, world_ui_project_system);
  ecs_schedule_single(w, world_ui_consolidate_system);
}

// ---- layout (main lane) ----

hz_internal HzUIString world_ui_copy(const char *chars, u32 len) {
  Allocator ta = tctx_temp_allocator(NULL);
  char *out = ALLOC_ARRAY_NO_ZERO(&ta, char, len + 1);
  mem_cpy(out, chars, len);
  return (HzUIString){.chars = out, .length = (i32)len};
}

hz_internal HzUIColor world_ui_color(ColorF32 c, f32 alpha) {
  return (HzUIColor){c.r * 255.0f, c.g * 255.0f, c.b * 255.0f, 255.0f * alpha};
}

hz_internal HzUIFloating world_ui_float(HzUIFloatingAttachPointType attach, v2 at, i16 z) {
  return (HzUIFloating){
      .attach_to = UI_ATTACH_TO_ROOT,
      .attach_points = {.element = attach, .parent = UI_ATTACH_POINT_LEFT_TOP},
      .offset = at,
      .z_index = z,
      .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH,
  };
}

hz_internal hz_noinline void world_ui_draw_nametag(const WorldUiStyle *st, const WorldUiLabel *l, const WorldUiScreen *s, EcsEntity e) {
  const WorldUiNametagStyle *n = &st->nametag;
  v2 at = {s->pos.x, s->pos.y + s->stack_dy};
  b32 bar = n->bar_size.x > 0.0f && n->bar_size.y > 0.0f &&
            (l->fill_mode == WORLD_UI_FILL_ALWAYS || (l->fill_mode == WORLD_UI_FILL_ON_HIT && l->fill_age < l->fill_hold));
  ui_element({.id = ui_idi("WorldUiNametag", (u32)e),
              .layout = {.layout_direction = UI_TOP_TO_BOTTOM, .child_gap = (u16)n->gap,
                         .child_alignment = {.x = UI_ALIGN_X_CENTER}},
              .floating = world_ui_float(n->attach, at, st->z_index),
              .opacity = opt_some(f32, s->alpha),
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    HzUITextConfig name = n->name;
    name.textColor = world_ui_color(l->color, 1.0f);
    ui_text(world_ui_copy(l->text.value, l->text.len), name);
    if (l->sub.len > 0)
      ui_text(world_ui_copy(l->sub.value, l->sub.len), n->sub);
    if (bar) {
      ui_element({.layout = {.sizing = {ui_sizing_fixed(n->bar_size.x), ui_sizing_fixed(n->bar_size.y)}},
                  .background_color = n->bar_back,
                  .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
        ui_element({.layout = {.sizing = {ui_sizing_percent(m_saturatef(l->fill)), ui_sizing_grow(0)}},
                    .background_color = n->bar_fill,
                    .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {}
      }
    }
  }
}

hz_internal hz_noinline void world_ui_draw_popup(const WorldUiStyle *st, const WorldUiLabel *l, const WorldUiPopup *p,
                                                 const WorldUiScreen *s, EcsEntity e) {
  f32 t = p->life > 0.0f ? p->age / p->life : 1.0f;
  if (t >= 1.0f || t < 0.0f)
    return;
  f32 ef = p->enter_frac;
  f32 op = 1.0f, scale = 1.0f;
  v2 off = {0.0f, 0.0f};
  if (ef > 0.0f && t < ef) {
    f32 k = ease_css_out(t / ef);
    op = k;
    scale = m_lerpf(p->scale_from, 1.0f, k);
    off = v2_scale(p->enter_px, 1.0f - k);
  } else {
    f32 k = ease_css_out(m_saturatef((t - ef) / m_maxf(1.0f - ef, 1.0e-4f)));
    off = v2_scale(p->drift_px, k);
    if (t > p->fade_from)
      op = 1.0f - ease_css_out(m_saturatef((t - p->fade_from) / m_maxf(1.0f - p->fade_from, 1.0e-4f)));
  }
  const WorldUiPopupStyle *ps = &st->popup;
  HzUITextConfig text = ps->text;
  text.textColor = world_ui_color(l->color, 1.0f);
  if (ps->outline_frac > 0.0f) {
    text.outlineColor = ps->outline;
    text.outlineWidth = (u16)m_clampf(text.fontSize * ps->outline_frac, 1.0f, 10.0f);
  }
  v2 at = {s->pos.x + off.x, s->pos.y + s->stack_dy + off.y};
  ui_element({.id = ui_idi("WorldUiPopup", (u32)e),
              .floating = world_ui_float(ps->attach, at, st->z_index),
              .opacity = opt_some(f32, op * s->alpha),
              .transform = opt_some(HzUITransform, ((HzUITransform){.scale = {scale, scale}})),
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    ui_text(world_ui_copy(l->text.value, l->text.len), text);
  }
}

hz_internal hz_noinline void world_ui_draw_prompt(const WorldUiStyle *st, const WorldUiLabel *l, const WorldUiPrompt *p,
                                                  const WorldUiScreen *s, EcsEntity e) {
  const WorldUiPromptStyle *ps = &st->prompt;
  v2 at = {s->pos.x, s->pos.y + s->stack_dy};
  ui_element({.id = ui_idi("WorldUiPrompt", (u32)e),
              .layout = {.padding = {(u16)ps->padding.x, (u16)ps->padding.x, (u16)ps->padding.y, (u16)ps->padding.y},
                         .child_gap = (u16)ps->gap,
                         .child_alignment = {.y = UI_ALIGN_Y_CENTER}},
              .floating = world_ui_float(ps->attach, at, st->z_index),
              .background_color = ps->background,
              .border = {.color = ps->border, .width = ui_border_outside(1)},
              .corner_radius = ui_corner_radius(ps->corner_radius),
              .opacity = opt_some(f32, s->alpha),
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    if (p->key < KEY_MAX) {
      const char *key = input_button_names[p->key];
      ui_element({.layout = {.sizing = {ui_sizing_fixed(ps->key_size.x), ui_sizing_fixed(ps->key_size.y)},
                             .child_alignment = {UI_ALIGN_X_CENTER, UI_ALIGN_Y_CENTER}},
                  .shadow = {.color = ps->key_ring, .spread = 1.5f, .inset = true},
                  .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
        ui_text(world_ui_copy(key, (u32)cstr_len(key)), ps->key);
      }
    }
    HzUITextConfig text = ps->text;
    if (l->color.a > 0.0f)
      text.textColor = world_ui_color(l->color, 1.0f);
    ui_element({.pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
      ui_text(world_ui_copy(l->text.value, l->text.len), text);
      if (l->sub.len > 0)
        ui_text(world_ui_copy(l->sub.value, l->sub.len), ps->emph);
    }
  }
}

hz_internal hz_noinline void world_ui_draw_pin(const WorldUiStyle *st, const WorldUiLabel *l, const WorldUiScreen *s, EcsEntity e) {
  const WorldUiPinStyle *ps = &st->pin;
  v2 at = {s->pos.x, s->pos.y + s->stack_dy};
  ui_element({.id = ui_idi("WorldUiPin", (u32)e),
              .layout = {.sizing = {ui_sizing_fixed(ps->diameter), ui_sizing_fixed(ps->diameter)},
                         .child_alignment = {UI_ALIGN_X_CENTER, UI_ALIGN_Y_CENTER}},
              .floating = world_ui_float(ps->attach, at, st->z_index),
              .background_color = ps->background,
              .corner_radius = ui_corner_radius(ps->diameter * 0.5f),
              .opacity = opt_some(f32, s->alpha),
              .pointer_capture_mode = UI_POINTER_CAPTURE_MODE_PASSTHROUGH}) {
    ui_text(world_ui_copy(l->text.value, l->text.len), ps->text);
  }
}

void world_ui_layout(EcsWorld *w, UISystem *ui, const WorldUiStyle *style) {
  UNUSED(ui);
  EcsQuery *q = ecs_query(w, EcsIn(WorldUi), EcsIn(WorldUiLabel), EcsIn(WorldUiScreen), EcsOptional(WorldUiPopup),
                          EcsOptional(WorldUiPrompt));
  ecs_query_foreach(q, it) {
    const WorldUi *us = ecs_iter_get_component(&it, WorldUi, 0);
    const WorldUiLabel *ls = ecs_iter_get_component(&it, WorldUiLabel, 1);
    const WorldUiScreen *ss = ecs_iter_get_component(&it, WorldUiScreen, 2);
    const WorldUiPopup *ps = ecs_iter_get_component(&it, WorldUiPopup, 3);
    const WorldUiPrompt *prs = ecs_iter_get_component(&it, WorldUiPrompt, 4);
    for (i32 i = 0; i < it.count; i++) {
      if (!(style->kinds & (1u << us[i].kind)) || ss[i].alpha <= 0.0f)
        continue;
      EcsEntity e = it.entities[i];
      switch (us[i].kind) {
      case WORLD_UI_NAMETAG: world_ui_draw_nametag(style, &ls[i], &ss[i], e); break;
      case WORLD_UI_POPUP:
        if (ps)
          world_ui_draw_popup(style, &ls[i], &ps[i], &ss[i], e);
        break;
      case WORLD_UI_PROMPT:
        if (prs)
          world_ui_draw_prompt(style, &ls[i], &prs[i], &ss[i], e);
        break;
      case WORLD_UI_PIN: world_ui_draw_pin(style, &ls[i], &ss[i], e); break;
      default: break;
      }
    }
  }
}
