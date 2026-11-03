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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ucontext.h>
#include <unistd.h>

#include "ultra64.h"
#include "s30_audio.h"
#include "s30_display.h"
#include "s30_gfx.h"
#include "s30_input.h"
#include "s30_platform.h"

#include <errno.h> // after the game headers: controller.h has an errno field
#include <fcntl.h>

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

/* mkdir -p for a path's parent directories */
static void make_parents(const char* path) {
    char* p = strdup(path);
    char* s;

    for (s = strchr(p + 1, '/'); s != NULL; s = strchr(s + 1, '/')) {
        *s = '\0';
        if (mkdir(p, 0755) != 0 && errno != EEXIST) {
            perror(p);
        }
        *s = '/';
    }
    free(p);
}

/* XDG data location for the save; moves a save from the old $HOME/oot.sav */
static const char* default_save(const char* home) {
    const char* xdg = getenv("XDG_DATA_HOME");
    char* path = malloc(strlen(xdg ? xdg : home) + 32);
    char* old = malloc(strlen(home) + 16);

    if (xdg != NULL && xdg[0] == '/') {
        sprintf(path, "%s/oot/oot.sav", xdg);
    } else {
        sprintf(path, "%s/.local/share/oot/oot.sav", home);
    }
    make_parents(path);
    sprintf(old, "%s/oot.sav", home);
    if (access(path, F_OK) != 0 && access(old, F_OK) == 0) {
        if (rename(old, path) == 0) {
            printf("oot: moved save %s -> %s\n", old, path);
        } else {
            perror("oot: moving old save");
        }
    }
    free(old);
    return path;
}

static const char* path_beside(const char* exeDir, const char* name) {
    char* p = malloc(strlen(exeDir) + strlen(name) + 2);

    sprintf(p, "%s/%s", exeDir, name);
    return p;
}

static const char* sCrashLog; // crash.log next to the binary
static int sCrashFd = -1;

/* To stderr and the crash log (async-signal-safe calls only) */
static void crash_write(const char* s) {
    write(2, s, strlen(s));
    if (sCrashFd >= 0) {
        write(sCrashFd, s, strlen(s));
    }
}

/* Crash report on stderr (the launcher logs it, in RAM) and appended to the
   crash log: fault address, registers and the code addresses found on the
   stack, for addr2line against the binary */
static void on_crash(int sig, siginfo_t* info, void* ctx) {
    extern char __executable_start[], etext[];
    const mcontext_t* mc = &((const ucontext_t*)ctx)->uc_mcontext;
    const uintptr_t* sp = (const uintptr_t*)mc->arm_sp;
    char buf[160];
    int i, n = 0;

    sCrashFd = open(sCrashLog, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);

    snprintf(buf, sizeof(buf), "crash: signal %d at %p pc %08lx lr %08lx sp %08lx\ncrash: stack", sig,
             info->si_addr, mc->arm_pc, mc->arm_lr, mc->arm_sp);
    crash_write(buf);
    for (i = 0; i < 512 && n < 24; i++) {
        if (sp[i] >= (uintptr_t)__executable_start && sp[i] < (uintptr_t)etext) {
            snprintf(buf, sizeof(buf), " %08lx", (unsigned long)sp[i]);
            crash_write(buf);
            n++;
        }
    }
    crash_write("\n");
    if (sCrashFd >= 0) {
        fsync(sCrashFd); // the device may be powered off next
        close(sCrashFd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}

int main(int argc, char** argv) {
    char* exe = strdup(argv[0]);
    const char* exeDir = dirname(exe);
    const char* home = getenv("HOME");

    setvbuf(stdout, NULL, _IOLBF, 0); // logs survive a crash
    sCrashLog = path_beside(exeDir, "crash.log");
    {
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = on_crash;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGBUS, &sa, NULL);
        sigaction(SIGILL, &sa, NULL);
        sigaction(SIGFPE, &sa, NULL);
    }

    gS30DataDir = getenv("OOT_DATA") ? getenv("OOT_DATA") : path_beside(exeDir, "data");
    if (getenv("OOT_SAVE")) {
        gS30SaveFile = getenv("OOT_SAVE");
    } else {
        gS30SaveFile = home != NULL ? default_save(home) : path_beside(exeDir, "oot.sav");
    }
    gS30EnableAudio = getenv("OOT_NOAUDIO") == NULL;
    printf("oot: data %s, save %s, audio %s\n", gS30DataDir, gS30SaveFile, gS30EnableAudio ? "on" : "off");

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

    __osTimerServicesInit();
    Main(NULL);
    return 0;
}
