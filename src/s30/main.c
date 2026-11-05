/*
 * Ocarina of Time on the PocketGo S30: process entry.
 *
 * Environment:
 *   OOT_DATA     asset segment directory (default: "data" next to the binary)
 *   OOT_SAVE     save file (default: $XDG_DATA_HOME/oot/oot.sav, i.e.
 *                ~/.local/share/oot/oot.sav; else oot.sav next to the binary)
 *   OOT_NOAUDIO  set to run without sound
 *
 * Crash reports also go to crash.log next to the binary (on the SD card;
 * stderr is the launcher's log in /tmp, lost on reboot).
 */
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ultra64.h"
#include "s30_audio.h"
#include "s30_app.h"
#include "s30_display.h"
#include "s30_gfx.h"
#include "s30_input.h"
#include "s30_os.h"
#include "s30_platform.h"

#include <errno.h> // after the game headers: controller.h has an errno field

const char* gS30DataDir;
const char* gS30SaveFile;

extern void Main(void* arg);
extern void AudioRom_LoadAll(void);
extern const S30ButtonMap gS30Buttons[];
extern const int gS30ButtonCount;
extern uintptr_t gSegments[];
extern void* AudioThread_Update(void); // returns AudioTask* (the audio headers aren't included here)

/* The audio thread's update (the shared library's audio thread, audio lock held) */
static void audio_update(void) {
    AudioThread_Update();
}

void S30_StartAudio(void) {
    S30Audio_Init(audio_update);
}


/* The save was ~/oot.sav before it moved to the XDG data directory */
static void move_old_save(const char* path) {
    const char* home = getenv("HOME");
    char* old;

    if (home == NULL) {
        return;
    }
    old = malloc(strlen(home) + 16);
    sprintf(old, "%s/oot.sav", home);
    if (access(path, F_OK) != 0 && access(old, F_OK) == 0) {
        if (rename(old, path) == 0) {
            printf("oot: moved save %s -> %s\n", old, path);
        } else {
            perror("oot: moving old save");
        }
    }
    free(old);
}

int main(int argc, char** argv) {
    char* exe = strdup(argv[0]);
    const char* exeDir = dirname(exe);

    setvbuf(stdout, NULL, _IOLBF, 0); // logs survive a crash
    S30App_InstallCrashHandler(S30App_PathIn(exeDir, "crash.log"));

    gS30DataDir = getenv("OOT_DATA") ? getenv("OOT_DATA") : S30App_PathIn(exeDir, "data");
    if (getenv("OOT_SAVE")) {
        gS30SaveFile = getenv("OOT_SAVE");
    } else {
        gS30SaveFile = S30App_SavePath("oot", "oot.sav", exeDir);
        move_old_save(gS30SaveFile);
    }
    gS30EnableAudio = getenv("OOT_NOAUDIO") == NULL;
    printf("oot: data %s, save %s, audio %s\n", gS30DataDir, gS30SaveFile, gS30EnableAudio ? "on" : "off");
    {
        // The game loop runs on this thread (Main -> Graph_ThreadEntry); the
        // game's threads stay off. ROM data comes through DmaMgr (src/linux/reimpl.c).
        S30OsConfig os = {
            .savePath = gS30SaveFile,
            .saveType = S30_SAVE_SRAM,
            .noWait = 1, // no scheduler thread to wait for: an empty queue fails
        };
        S30Os_Init(&os);
    }

    {
        // F3DEX2 (S2DEX backgrounds), float Mtx, textures as u64 arrays
        S30GfxConfig gfx = { S30_UCODE_F3DEX2, (uint32_t*)gSegments, 1, NULL, "OOT" };
        S30Gfx_Configure(&gfx);
    }
    S30Display_SetRefresh(60.0f); // N64 NTSC: 3 refreshes a frame is 20 fps
    S30Display_Init();
    S30Gfx_Init();
    S30Input_Init(gS30Buttons, gS30ButtonCount);
    if (gS30EnableAudio) {
        AudioRom_LoadAll();
    }

    Main(NULL);
    return 0;
}
