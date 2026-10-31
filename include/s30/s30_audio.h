#ifndef S30_AUDIO_H
#define S30_AUDIO_H

#include "ultra64.h"

// ALSA output for the audio thread (src/s30/audio.c). The game's audio update
// renders separate L/R buffers at 32 kHz; S30Audio_Play blocks while the ALSA
// buffer is full, which paces the audio thread like the N64 AI interrupt.
extern int gS30EnableAudio;

void S30Audio_Init(void);
u32 S30Audio_QueuedFrames(void);
void S30Audio_Play(const s16* left, const s16* right, s32 frames);

#endif
