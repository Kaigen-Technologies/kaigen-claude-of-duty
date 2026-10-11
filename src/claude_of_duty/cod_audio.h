// Claude of Duty audio: synthesized clips rendered at load, played from the packages' events through engine audio.
#pragma once
#include "audio.h"
#include "ecs/ecs.h"
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "procgen/pg_audio.h"
#include "claude_of_duty.h"

struct CodAudio; // the mixer, the clips, the live voices

typedef struct CodAudioState {
  b32 ready;      // every clip is rendered
  f32 progress;   // 0..1
  f64 render_ms;  // wall time of the clip render, first job to last
  u32 clips;      // clips rendered
  f32 clip_mb;    // PCM bytes held by the clips, in MB
  struct CodAudio *impl;
} CodAudioState;
ECS_COMPONENT_TYPEDEF(CodAudioState);

// main lane, app_init: the audio device, mixer buses, the clip render jobs
void cod_audio_init(EcsWorld *w);
// inside the scope, late: consolidation of every package's events into voice commands
void cod_audio_schedule(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_audio_lanes(EcsWorld *w);
// main lane after the scope: the commands become voices, the mixer ticks
void cod_audio_frame(EcsWorld *w);
