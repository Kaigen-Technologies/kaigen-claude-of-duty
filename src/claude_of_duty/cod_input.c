// Claude of Duty input: the actions on the engine action map, collected on the main lane before the scope.

typedef enum CodAction {
  COD_ACT_FORWARD = 0,
  COD_ACT_BACK,
  COD_ACT_LEFT,
  COD_ACT_RIGHT,
  COD_ACT_JUMP,
  COD_ACT_CROUCH,
  COD_ACT_PRONE,
  COD_ACT_SPRINT,
  COD_ACT_FIRE,
  COD_ACT_ADS,
  COD_ACT_RELOAD,
  COD_ACT_USE,
  COD_ACT_MELEE,
  COD_ACT_LEAN_LEFT,
  COD_ACT_LEAN_RIGHT,
  COD_ACT_SWAP,
  COD_ACT_GRENADE,
  COD_ACT_FLASHLIGHT,
  COD_ACT_PAUSE,
  COD_ACT_SLOT_1,
  COD_ACT_SLOT_2,
  COD_ACT_SLOT_3,
  COD_ACT_FIRE_MODE,
  COD_ACT_INSPECT,
  COD_ACT_COUNT,
} CodAction;

#define COD_STICK_DEADZONE 0.15f
#define COD_PAD_LOOK_PX 900.0f // right stick at full deflection: pointer pixels per second

// main-lane input state: the action map and the pointer lock
typedef struct CodInputMap {
  InputActionMap map;
  InputPointerLock lock;
} CodInputMap;
ECS_COMPONENT_TYPEDEF(CodInputMap);

hz_internal void cod_bind(InputActionMap *m, u32 id, const char *name, App_InputButtonType k0, App_InputButtonType k1, i32 pad) {
  u32 a = input_action_define(m, name);
  assert_msg(a == id, "cod input: actions defined out of order");
  input_action_bind_key(m, a, k0);
  if (k1 != KEY_MAX)
    input_action_bind_key(m, a, k1);
  if (pad >= 0)
    input_action_bind_pad_button(m, a, (GamepadButton)pad);
}

hz_internal void cod_input_map_init(CodInputMap *im) {
  InputActionMap *m = &im->map;
  cod_bind(m, COD_ACT_FORWARD, "forward", KEY_W, KEY_UP, -1);
  cod_bind(m, COD_ACT_BACK, "back", KEY_S, KEY_DOWN, -1);
  cod_bind(m, COD_ACT_LEFT, "left", KEY_A, KEY_LEFT, -1);
  cod_bind(m, COD_ACT_RIGHT, "right", KEY_D, KEY_RIGHT, -1);
  input_action_bind_pad_axis(m, COD_ACT_FORWARD, GAMEPAD_AXIS_LEFT_Y, 1.0f, COD_STICK_DEADZONE);
  input_action_bind_pad_axis(m, COD_ACT_BACK, GAMEPAD_AXIS_LEFT_Y, -1.0f, COD_STICK_DEADZONE);
  input_action_bind_pad_axis(m, COD_ACT_LEFT, GAMEPAD_AXIS_LEFT_X, -1.0f, COD_STICK_DEADZONE);
  input_action_bind_pad_axis(m, COD_ACT_RIGHT, GAMEPAD_AXIS_LEFT_X, 1.0f, COD_STICK_DEADZONE);
  cod_bind(m, COD_ACT_JUMP, "jump", KEY_SPACE, KEY_MAX, GAMEPAD_A);
  cod_bind(m, COD_ACT_CROUCH, "crouch", KEY_LEFT_CONTROL, KEY_C, GAMEPAD_B);
  cod_bind(m, COD_ACT_PRONE, "prone", KEY_Z, KEY_MAX, -1);
  cod_bind(m, COD_ACT_SPRINT, "sprint", KEY_LEFT_SHIFT, KEY_MAX, GAMEPAD_LSTICK);
  cod_bind(m, COD_ACT_FIRE, "fire", MOUSE_LEFT, KEY_MAX, -1);
  cod_bind(m, COD_ACT_ADS, "ads", MOUSE_RIGHT, KEY_MAX, -1);
  input_action_bind_pad_axis(m, COD_ACT_FIRE, GAMEPAD_AXIS_RIGHT_TRIGGER, 1.0f, GAMEPAD_TRIGGER_THRESHOLD);
  input_action_bind_pad_axis(m, COD_ACT_ADS, GAMEPAD_AXIS_LEFT_TRIGGER, 1.0f, GAMEPAD_TRIGGER_THRESHOLD);
  cod_bind(m, COD_ACT_RELOAD, "reload", KEY_R, KEY_MAX, GAMEPAD_X);
  cod_bind(m, COD_ACT_USE, "use", KEY_F, KEY_MAX, -1);
  cod_bind(m, COD_ACT_MELEE, "melee", KEY_V, KEY_MAX, GAMEPAD_RSTICK);
  cod_bind(m, COD_ACT_LEAN_LEFT, "leanLeft", KEY_Q, KEY_MAX, -1);
  cod_bind(m, COD_ACT_LEAN_RIGHT, "leanRight", KEY_E, KEY_MAX, -1);
  cod_bind(m, COD_ACT_SWAP, "swapWeapon", KEY_TAB, KEY_MAX, GAMEPAD_Y);
  cod_bind(m, COD_ACT_GRENADE, "grenade", KEY_G, KEY_MAX, GAMEPAD_RB);
  cod_bind(m, COD_ACT_FLASHLIGHT, "flashlight", KEY_T, KEY_MAX, GAMEPAD_DPAD_UP);
  cod_bind(m, COD_ACT_PAUSE, "pause", KEY_ESCAPE, KEY_MAX, GAMEPAD_START);
  // reads these keys directly; actions here so they rebind like the rest
  cod_bind(m, COD_ACT_SLOT_1, "slot1", KEY_1, KEY_MAX, -1);
  cod_bind(m, COD_ACT_SLOT_2, "slot2", KEY_2, KEY_MAX, -1);
  cod_bind(m, COD_ACT_SLOT_3, "slot3", KEY_3, KEY_MAX, -1);
  cod_bind(m, COD_ACT_FIRE_MODE, "fireMode", KEY_B, KEY_MAX, -1);
  cod_bind(m, COD_ACT_INSPECT, "inspect", KEY_I, KEY_MAX, -1);
}

// touch look in pointer pixels: a full landscape swipe (~800 pt) turns ~180 degrees at sensitivity 1
#define COD_TOUCH_LOOK_SCALE 1.8f
// the stick pushed this far past full speed, mostly forward, sprints (Left Shift on a keyboard)
#define COD_TOUCH_SPRINT_TRAVEL 1.6f
#define COD_TOUCH_SPRINT_FORWARD 0.7f

hz_internal TouchControlsLayout cod_touch_layout(TouchOrientation o) {
  TouchControlsLayout l = touch_controls_layout_default(o);
  // the stick floats anywhere on the left, below the minimap
  l.region_pos_pct = (v2){0.0f, 0.3f};
  l.region_size_pct = (v2){o == TOUCH_ORIENTATION_PORTRAIT ? 0.5f : 0.42f, 0.7f};
  l.stick_outer_radius_px = 56.0f;
  l.stick_ring_radius_px = 26.0f;
  l.stick_middle_radius_px = 16.0f;
  l.deadzone_radius_px = 4.0f;
  l.max_speed_radius_px = 36.0f;
  l.pinch_grace_s = 0.0f;
  l.buttons[COD_TOUCH_FIRE] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {130.0f, 110.0f}, 84.0f};
  l.buttons[COD_TOUCH_ADS] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {52.0f, 206.0f}, 58.0f};
  l.buttons[COD_TOUCH_JUMP] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {46.0f, 62.0f}, 62.0f};
  l.buttons[COD_TOUCH_CROUCH] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {46.0f, 134.0f}, 54.0f};
  l.buttons[COD_TOUCH_RELOAD] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {226.0f, 46.0f}, 54.0f};
  l.buttons[COD_TOUCH_SWAP] = (TouchButtonPlacement){TOUCH_ANCHOR_BOTTOM_RIGHT, {196.0f, 186.0f}, 54.0f};
  l.buttons[COD_TOUCH_PAUSE] = (TouchButtonPlacement){TOUCH_ANCHOR_TOP_RIGHT, {30.0f, 30.0f}, 44.0f};
  return l;
}

// main lane, cod_hud_init: the on-screen controls; touches starting over the menu stay with the UI
void cod_touch_init(EcsWorld *w, UISystem *ui) {
  TouchControlsConfig cfg = {
      .disable_jump_button = true, // JUMP is an action button: the movement wants its press edge
      .ui_scale = 1.0f,
      .ui = ui,
      .disable_pinch = true,
      .landscape = cod_touch_layout(TOUCH_ORIENTATION_LANDSCAPE),
      .portrait = cod_touch_layout(TOUCH_ORIENTATION_PORTRAIT),
  };
  cfg.buttons[COD_TOUCH_FIRE] = (TouchButtonDesc){.enabled = true, .look_through = true, .label = "FIRE"};
  cfg.buttons[COD_TOUCH_ADS] = (TouchButtonDesc){.enabled = true, .look_through = true, .label = "AIM"};
  cfg.buttons[COD_TOUCH_JUMP] = (TouchButtonDesc){.enabled = true, .label = "JUMP"};
  cfg.buttons[COD_TOUCH_CROUCH] = (TouchButtonDesc){.enabled = true, .label = "CROUCH"};
  cfg.buttons[COD_TOUCH_RELOAD] = (TouchButtonDesc){.enabled = true, .label = "RELOAD"};
  cfg.buttons[COD_TOUCH_SWAP] = (TouchButtonDesc){.enabled = true, .label = "SWAP"};
  cfg.buttons[COD_TOUCH_PAUSE] = (TouchButtonDesc){.enabled = true, .label = "II"};
  ecs_resource_set(w, CodTouch, ((CodTouch){.tc = touch_controls_init(cfg)}));
}

// the touch frame folded into the devices' input: the stick adds to the move, the drag to the look
hz_internal void cod_touch_collect(CodInput *in, CodTouch *touch, InputSystem *input, b32 crouched, f32 now) {
  TouchControls *tc = &touch->tc;
  TouchControlsFrame tf = touch_controls_update(tc, input, now);
  const TouchButtonState *bt = tf.buttons;
  if (bt[COD_TOUCH_ADS].pressed)
    touch->ads = !touch->ads;
  touch_controls_set_button(tc, COD_TOUCH_ADS, true, touch->ads);
  touch_controls_set_button(tc, COD_TOUCH_CROUCH, true, crouched);

  in->move = v2_add(in->move, tf.move);
  if (v2_length(in->move) > 1.0f)
    in->move = v2_normalize(in->move);
  in->look = v2_add(in->look, v2_scale(tf.camera_pan_px, COD_TOUCH_LOOK_SCALE));
  b32 sprint = tf.stick_travel >= COD_TOUCH_SPRINT_TRAVEL && tf.move.y >= COD_TOUCH_SPRINT_FORWARD;
  in->sprint_pressed |= sprint && !touch->sprint; // each over-push is a press: two quick ones are the tactical sprint
  in->sprint |= sprint;
  touch->sprint = sprint;
  in->fire |= bt[COD_TOUCH_FIRE].held;
  in->fire_pressed |= bt[COD_TOUCH_FIRE].pressed;
  in->ads |= touch->ads;
  in->ads_pressed |= bt[COD_TOUCH_ADS].pressed && touch->ads;
  in->jump |= bt[COD_TOUCH_JUMP].held;
  in->jump_pressed |= bt[COD_TOUCH_JUMP].pressed;
  in->crouch |= bt[COD_TOUCH_CROUCH].held;
  in->crouch_pressed |= bt[COD_TOUCH_CROUCH].pressed;
  in->reload_pressed |= bt[COD_TOUCH_RELOAD].pressed;
  in->swap_pressed |= bt[COD_TOUCH_SWAP].pressed;
  in->pause_pressed |= bt[COD_TOUCH_PAUSE].pressed;
}

hz_internal b32 cod_act_down(const InputActionMap *m, u32 a) { return input_action_down(m, a); }
hz_internal b32 cod_act_pressed(const InputActionMap *m, u32 a) { return input_action_pressed(m, a); }

// main lane, after input_update and before the scope: devices into CodInput; the lock request lives here
void cod_input_collect(CodInput *in, CodInputMap *im, CodTouch *touch, InputSystem *input, u32 state, b32 debug_controls,
                       b32 focus_lost, b32 crouched, f32 dt, f32 now) {
  InputActionMap *m = &im->map;
  input_action_map_update(m, input);
  b32 playing = state == COD_STATE_PLAYING;
  b32 touched = input->last_category == INPUT_CATEGORY_TOUCH;
  // a click while playing requests the lock; Escape and focus loss release it; touch has no pointer to lock
  input_pointer_lock_update(&im->lock, input, playing && !touched, focus_lost);
  b32 locked = im->lock.locked;
  b32 live = locked || debug_controls || touched;
  *in = (CodInput){0};
  in->touch = touched;
  in->captured = live;
  in->lock_gained = im->lock.gained_this_frame;
  in->lock_lost = im->lock.lost_this_frame;
  in->focused = hz_is_focused();
  in->focus_lost = focus_lost;
  in->pause_pressed = cod_act_pressed(m, COD_ACT_PAUSE);
  // the on-screen controls exist only in a match; outside it every touch belongs to the menus
  for (u32 b = 0; b < COD_TOUCH_BUTTON_COUNT; b++)
    touch_controls_set_button(&touch->tc, b, playing, false);
  if (!playing || !touched) {
    touch->ads = false;
    touch->sprint = false;
  }
  if (!live)
    return;

  // the lock frame's click is the lock request, not a shot; a touch's bridged click is a look drag
  b32 pointer = locked ? !im->lock.gained_this_frame : !touched;
  in->move = (v2){input_action_value(m, COD_ACT_RIGHT) - input_action_value(m, COD_ACT_LEFT),
                  input_action_value(m, COD_ACT_FORWARD) - input_action_value(m, COD_ACT_BACK)};
  if (v2_length(in->move) > 1.0f)
    in->move = v2_normalize(in->move);
  in->look = locked ? input_pointer_lock_look(&im->lock, input) : (v2){0};
  const InputGamepad *pad = &input->gamepads[input->active_gamepad];
  if (pad->connected) {
    v2 rs = input_gamepad_stick(pad->right_stick, COD_STICK_DEADZONE);
    in->look = v2_add(in->look, v2_scale((v2){rs.x, -rs.y}, COD_PAD_LOOK_PX * dt));
  }
  in->wheel = (i32)m_roundf(input->scroll_delta.y);
  in->jump = cod_act_down(m, COD_ACT_JUMP);
  in->jump_pressed = cod_act_pressed(m, COD_ACT_JUMP);
  in->crouch = cod_act_down(m, COD_ACT_CROUCH);
  in->crouch_pressed = cod_act_pressed(m, COD_ACT_CROUCH);
  in->prone = cod_act_down(m, COD_ACT_PRONE);
  in->prone_pressed = cod_act_pressed(m, COD_ACT_PRONE);
  in->sprint = cod_act_down(m, COD_ACT_SPRINT);
  in->sprint_pressed = cod_act_pressed(m, COD_ACT_SPRINT);
  in->fire = pointer && cod_act_down(m, COD_ACT_FIRE);
  in->fire_pressed = pointer && cod_act_pressed(m, COD_ACT_FIRE);
  in->ads = pointer && cod_act_down(m, COD_ACT_ADS);
  in->ads_pressed = pointer && cod_act_pressed(m, COD_ACT_ADS);
  in->reload_pressed = cod_act_pressed(m, COD_ACT_RELOAD);
  in->use_pressed = cod_act_pressed(m, COD_ACT_USE);
  in->melee_pressed = cod_act_pressed(m, COD_ACT_MELEE);
  in->lean_left = cod_act_down(m, COD_ACT_LEAN_LEFT);
  in->lean_right = cod_act_down(m, COD_ACT_LEAN_RIGHT);
  in->swap_pressed = cod_act_pressed(m, COD_ACT_SWAP);
  in->grenade_pressed = cod_act_pressed(m, COD_ACT_GRENADE);
  in->flashlight_pressed = cod_act_pressed(m, COD_ACT_FLASHLIGHT);
  in->weapon_slot_pressed = cod_act_pressed(m, COD_ACT_SLOT_1)   ? 1
                            : cod_act_pressed(m, COD_ACT_SLOT_2) ? 2
                            : cod_act_pressed(m, COD_ACT_SLOT_3) ? 3
                                                                 : 0;
  in->fire_mode_pressed = cod_act_pressed(m, COD_ACT_FIRE_MODE);
  in->inspect_pressed = cod_act_pressed(m, COD_ACT_INSPECT);
  if (playing && touched)
    cod_touch_collect(in, touch, input, crouched, now);
}

// an open editor holds the match: no input and no lock or focus edges
void cod_input_hold(CodInput *in) {
  *in = (CodInput){.captured = in->captured, .touch = in->touch, .focused = in->focused, .held = true};
}

void cod_input_init(EcsWorld *w) {
  ecs_resource_set(w, CodInput, ((CodInput){.focused = true}));
  ecs_resource_set_zero(w, CodInputMap);
  cod_input_map_init(ecs_resource_get_mut(w, CodInputMap));
}
