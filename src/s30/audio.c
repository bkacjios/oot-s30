/*
 * Audio output: the game's audio update (AudioThread_Update) runs on its own
 * thread and renders separate L/R buffers at 32 kHz; they are interleaved and
 * written to ALSA. snd_pcm_writei blocks while the device buffer is full, so
 * the update runs at the rate the codec consumes samples (about 60 per
 * second), like the N64's AI-driven audio thread.
 */
// Game headers first: musl's errno macro would rename their "errno" fields
#include "audio.h"
#include "s30_audio.h"

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>

#define S30_AUDIO_RATE 32000
#define S30_AUDIO_PERIOD 512   // frames
#define S30_AUDIO_PERIODS 4    // buffer = 64 ms

int gS30EnableAudio = 1;

static snd_pcm_t* sPcm;
static snd_pcm_uframes_t sBufferFrames;

extern AudioTask* AudioThread_Update(void);

static void* audio_thread(void* arg) {
    (void)arg;
    for (;;) {
        AudioThread_Update();
    }
    return NULL;
}

void S30Audio_Init(void) {
    snd_pcm_hw_params_t* hw;
    snd_pcm_uframes_t period = S30_AUDIO_PERIOD;
    unsigned int rate = S30_AUDIO_RATE;
    pthread_t thread;
    int err;

    err = snd_pcm_open(&sPcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        printf("audio: snd_pcm_open: %s, audio disabled\n", snd_strerror(err));
        sPcm = NULL;
        return;
    }
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(sPcm, hw);
    snd_pcm_hw_params_set_access(sPcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(sPcm, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(sPcm, hw, 2);
    snd_pcm_hw_params_set_rate_near(sPcm, hw, &rate, NULL);
    snd_pcm_hw_params_set_period_size_near(sPcm, hw, &period, NULL);
    sBufferFrames = period * S30_AUDIO_PERIODS;
    snd_pcm_hw_params_set_buffer_size_near(sPcm, hw, &sBufferFrames);
    err = snd_pcm_hw_params(sPcm, hw);
    if (err < 0) {
        printf("audio: snd_pcm_hw_params: %s, audio disabled\n", snd_strerror(err));
        snd_pcm_close(sPcm);
        sPcm = NULL;
        return;
    }
    printf("audio: %u Hz, period %lu, buffer %lu frames\n", rate, (unsigned long)period,
           (unsigned long)sBufferFrames);

    pthread_create(&thread, NULL, audio_thread, NULL);
    pthread_detach(thread);
}

u32 S30Audio_QueuedFrames(void) {
    snd_pcm_sframes_t avail;

    if (sPcm == NULL) {
        return 0;
    }
    avail = snd_pcm_avail_update(sPcm);
    if (avail < 0 || (snd_pcm_uframes_t)avail > sBufferFrames) {
        return 0;
    }
    return sBufferFrames - avail;
}

void S30Audio_Play(const s16* left, const s16* right, s32 frames) {
    s16 buf[2 * 1024];
    s32 done = 0;

    if (sPcm == NULL) {
        return;
    }
    while (done < frames) {
        s32 n = frames - done;
        s32 i;
        snd_pcm_sframes_t written;

        if (n > 1024) {
            n = 1024;
        }
        for (i = 0; i < n; i++) {
            buf[2 * i] = left[done + i];
            buf[2 * i + 1] = right[done + i];
        }
        written = snd_pcm_writei(sPcm, buf, n);
        if (written < 0) {
            // Underrun (-EPIPE) or suspend: recover and drop this chunk
            snd_pcm_recover(sPcm, written, 1);
            return;
        }
        done += written;
    }
}
