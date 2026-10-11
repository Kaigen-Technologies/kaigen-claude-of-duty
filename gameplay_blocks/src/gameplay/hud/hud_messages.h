// HUD messages: banner, toast and hint channels with a queue, priority, dedupe, delay and enter/exit tweens.
// Push from any system (hud_msg_push), hud_msg_schedule in the scope, hud_msg_layout in the UI frame; a '*' pair marks emphasis, a newline breaks a line.
#pragma once

#include "ecs/ecs.h"
#include "lib/string.h"
#include "ui/ui.h"

typedef enum HudMsgChannel {
  HUD_MSG_BANNER = 0, // the big centre line: a wave, an objective, a result
  HUD_MSG_TOAST,      // a short note: an error, a notice
  HUD_MSG_HINT,       // standing help, shown until cleared
  HUD_MSG_CHANNEL_COUNT,
} HudMsgChannel;

#define HUD_MSG_KINDS 4      // style overrides a message can select with `kind`
#define HUD_MSG_QUEUE_MAX 8  // queued and delayed messages per channel

typedef struct HudMsg {
  HudMsgChannel channel;
  String256Bytes title;
  String128Bytes body;  // second line; empty = none
  String32Bytes key;    // key-cap label before the text (a prompt); empty = none
  u32 kind;             // < HUD_MSG_KINDS: selects HudMsgStyle.kinds
  i32 priority;         // QUEUE channels: a higher one preempts the active message and sorts first
  f32 duration;         // seconds fully shown; <=0 stays until hud_msg_clear
  f32 delay;            // seconds before it is submitted; the message showing meanwhile is not disturbed
  u32 dedupe;           // nonzero: a message with the same value, active or queued, is refreshed instead of added
} HudMsg;

typedef enum HudMsgPolicy {
  HUD_MSG_POLICY_QUEUE = 0, // messages wait their turn, highest priority first
  HUD_MSG_POLICY_REPLACE,   // the newest message takes the channel at once; nothing queues
} HudMsgPolicy;

typedef struct HudMsgChannelDesc {
  HudMsgPolicy policy;
  f32 enter_s;               // seconds the level rises 0 -> 1; 0 = instant
  f32 exit_s;                // seconds the level falls 1 -> 0; 0 = instant
  b32 restart_on_replace;    // REPLACE: a new message re-enters from level 0 instead of swapping its text in place
} HudMsgChannelDesc;

typedef struct HudMsgDesc {
  HudMsgChannelDesc channels[HUD_MSG_CHANNEL_COUNT];
} HudMsgDesc;

typedef enum HudMsgPhase {
  HUD_MSG_PHASE_IDLE = 0,
  HUD_MSG_PHASE_ENTER,
  HUD_MSG_PHASE_HOLD,
  HUD_MSG_PHASE_EXIT,
} HudMsgPhase;

typedef struct HudMsgPending {
  HudMsg msg;
  f32 wait;
} HudMsgPending;

typedef struct HudMsgChannelState {
  HudMsg active;
  HudMsgPhase phase;
  f32 level;     // 0..1, linear through enter and exit
  f32 hold_age;  // seconds in HOLD
  u32 serial;    // bumps when a message starts entering
  HudMsg queue[HUD_MSG_QUEUE_MAX];
  u32 queue_len;
  HudMsgPending pending[HUD_MSG_QUEUE_MAX];
  u32 pending_len;
} HudMsgChannelState;

// the block's resource; the UI frame reads it, only the schedule writes it
ECS_COMPONENT_TYPEDEF(HudMessages);
typedef struct HudMessages {
  HudMsgChannelDesc desc[HUD_MSG_CHANNEL_COUNT];
  HudMsgChannelState channels[HUD_MSG_CHANNEL_COUNT];
} HudMessages;

// a request a system raised; applied and destroyed by the schedule
ECS_COMPONENT_TYPEDEF(HudMsgRequest);
typedef struct HudMsgRequest {
  b32 clear;
  HudMsgChannel channel;
  HudMsg msg;
} HudMsgRequest;

// the active message of a channel as the UI sees it
typedef struct HudMsgView {
  const HudMsg *msg;
  HudMsgPhase phase;
  f32 level;   // 0..1 linear: rising in ENTER, 1 in HOLD, falling in EXIT
  u32 serial;  // changes whenever a new message starts entering
} HudMsgView;

typedef enum HudMsgAnim {
  HUD_MSG_ANIM_FADE = 0, // opacity follows the css `ease` curve of the level
  HUD_MSG_ANIM_SLIDE,    // fade plus a translation of slide_px that closes as the level rises
  HUD_MSG_ANIM_POP,      // cubic-bezier(0.2, 0.9, 0.3, 1.2): opacity and a scale from pop_scale to 1
} HudMsgAnim;

typedef struct HudMsgKindStyle {
  HzUITextConfig title; // fontSize 0 = use HudMsgStyle.title
  HzUITextConfig body;  // fontSize 0 = use HudMsgStyle.body
} HudMsgKindStyle;

typedef struct HudMsgStyle {
  HzUIFloatingAttachPointType attach; // the panel's point that sits on `parent`
  HzUIFloatingAttachPointType parent; // the screen's point
  v2 offset;                          // px
  v2 offset_screen;                   // fraction of the canvas, added to offset
  i16 z_index;
  f32 max_width;                      // px; 0 = fit the text
  HudMsgAnim anim;
  v2 slide_px;
  f32 pop_scale;
  HzUITextConfig title;
  HzUITextConfig body;
  HzUITextConfig emph;                // the '*' runs; fontSize 0 = same as the line's base style
  b32 title_upper;
  b32 body_upper;
  f32 body_gap;                       // px between title and body
  HudMsgKindStyle kinds[HUD_MSG_KINDS];
  // the panel behind the text; no panel when background and border are transparent
  HzUIColor background;
  UIBorder border;
  f32 corner_radius;
  HzUIPadding padding;
  f32 backdrop_blur;
  HzUIShadow shadow;
  f32 skew_x;                         // degrees, shears the panel and its text
  // key cap, drawn when the message has a key
  v2 key_size;
  HzUIColor key_ring;
  HzUITextConfig key;
  f32 key_gap;
} HudMsgStyle;

// once, main lane; a NULL desc leaves every channel QUEUE with instant enter and exit
void hud_msg_init(EcsWorld *w, const HudMsgDesc *desc);
// inside the frame's ecs_scope; advances with the flush's delta time
void hud_msg_schedule(EcsWorld *w);

// callable from a system on any lane (and the main lane outside a scope): the message is applied this flush
void hud_msg_push(EcsWorld *w, const HudMsg *msg);
// ends the active message through its exit and drops everything queued or delayed on the channel
void hud_msg_clear(EcsWorld *w, HudMsgChannel channel);

// false when the channel shows nothing
b32 hud_msg_view(const HudMessages *h, HudMsgChannel channel, HudMsgView *out);
// main lane, inside the UI frame; draws the channel's message with the style, nothing when idle
void hud_msg_layout(const HudMessages *h, UISystem *ui, HudMsgChannel channel, const HudMsgStyle *style);
