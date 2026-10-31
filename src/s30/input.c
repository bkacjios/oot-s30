/*
 * N64 controller from the S30 gamepad ("PocketGo S30 Gamepad" evdev device:
 * GPIO buttons + MCU analog stick, 0-255 per axis, Y up = low).
 *
 * Layout: Y -> B, B -> A, X -> C-left, A -> C-down, R1 -> C-right, L1 -> L,
 * L2 -> Z, R2 -> R, Select -> C-up, Start -> Start, D-pad -> N64 D-pad
 * (unused by the stock game; reserved for D-pad item slots).
 * L1 + R1 + Select + Start (physical buttons) quits.
 */
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* controller.h redefines BTN_A, BTN_B, BTN_Z and BTN_START as N64 button
   bits; keep the evdev Start code first */
enum { EV_BTN_START = BTN_START };
#undef BTN_A
#undef BTN_B
#undef BTN_Z
#undef BTN_START
#include "controller.h"
#include "s30_input.h"

#define GAMEPAD_NAME "PocketGo S30 Gamepad"
#define STICK_MAX 80     // N64 stick range used by the game (about +-80)
#define STICK_DEADZONE 8 // raw units around the centre
#define STICK_MIN_REACH 60 // raw reach assumed until a larger deflection is seen

static const struct {
    u16 code;
    u16 button;
} sButtonMap[] = {
    { BTN_WEST, BTN_B },          { BTN_SOUTH, BTN_A },          { BTN_NORTH, BTN_CLEFT },
    { BTN_EAST, BTN_CDOWN },      { BTN_TR, BTN_CRIGHT },        { BTN_TL, BTN_L },
    { BTN_TL2, BTN_Z },           { BTN_TR2, BTN_R },            { BTN_SELECT, BTN_CUP },
    { EV_BTN_START, BTN_START },     { BTN_DPAD_UP, BTN_DUP },      { BTN_DPAD_DOWN, BTN_DDOWN },
    { BTN_DPAD_LEFT, BTN_DLEFT }, { BTN_DPAD_RIGHT, BTN_DRIGHT },
};

static int sFd = -1;
static u32 sKeys;     // pressed state per sButtonMap entry
static int sAxis[2];  // raw ABS_X, ABS_Y
static int sCentre[2];
static int sReachNeg[2], sReachPos[2];

static int open_gamepad(void) {
    DIR* dir = opendir("/dev/input");
    struct dirent* e;

    if (dir == NULL) {
        return -1;
    }
    while ((e = readdir(dir)) != NULL) {
        char path[300];
        char name[128] = "";
        int fd;

        if (strncmp(e->d_name, "event", 5) != 0) {
            continue;
        }
        snprintf(path, sizeof(path), "/dev/input/%s", e->d_name);
        fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        if (strcmp(name, GAMEPAD_NAME) == 0) {
            closedir(dir);
            printf("input: %s (%s)\n", name, path);
            return fd;
        }
        close(fd);
    }
    closedir(dir);
    return -1;
}

void S30Input_Init(void) {
    int i;

    sFd = open_gamepad();
    if (sFd < 0) {
        printf("input: no \"%s\" device, controls disabled\n", GAMEPAD_NAME);
        return;
    }
    for (i = 0; i < 2; i++) {
        struct input_absinfo abs;

        ioctl(sFd, EVIOCGABS(i == 0 ? ABS_X : ABS_Y), &abs);
        sAxis[i] = sCentre[i] = abs.value; // the stick rests at startup
        sReachNeg[i] = sReachPos[i] = STICK_MIN_REACH;
    }
}

static s8 stick_axis(int i) {
    int d = sAxis[i] - sCentre[i];
    int reach;

    // Widen the calibrated range as the stick reaches further
    if (d > sReachPos[i]) {
        sReachPos[i] = d;
    }
    if (-d > sReachNeg[i]) {
        sReachNeg[i] = -d;
    }
    if (d > -STICK_DEADZONE && d < STICK_DEADZONE) {
        return 0;
    }
    reach = d > 0 ? sReachPos[i] : sReachNeg[i];
    return (s8)(d * STICK_MAX / reach);
}

void S30Input_Read(S30Pad* pad) {
    struct input_event ev[32];
    ssize_t n;
    size_t i, k;
    int held;

    while (sFd >= 0 && (n = read(sFd, ev, sizeof(ev))) > 0) {
        for (i = 0; i < n / sizeof(ev[0]); i++) {
            if (ev[i].type == EV_KEY) {
                for (k = 0; k < sizeof(sButtonMap) / sizeof(sButtonMap[0]); k++) {
                    if (sButtonMap[k].code == ev[i].code) {
                        sKeys = ev[i].value ? (sKeys | (1u << k)) : (sKeys & ~(1u << k));
                    }
                }
            } else if (ev[i].type == EV_ABS && (ev[i].code == ABS_X || ev[i].code == ABS_Y)) {
                sAxis[ev[i].code == ABS_X ? 0 : 1] = ev[i].value;
            }
        }
    }

    pad->button = 0;
    held = 0;
    for (k = 0; k < sizeof(sButtonMap) / sizeof(sButtonMap[0]); k++) {
        if (sKeys & (1u << k)) {
            pad->button |= sButtonMap[k].button;
            // Exit combo on the physical buttons, whatever they map to
            held += sButtonMap[k].code == BTN_TL || sButtonMap[k].code == BTN_TR ||
                    sButtonMap[k].code == BTN_SELECT || sButtonMap[k].code == EV_BTN_START;
        }
    }
    if (held == 4) {
        printf("input: L1+R1+Select+Start, exiting\n");
        exit(0);
    }
    pad->stick_x = stick_axis(0);
    pad->stick_y = -stick_axis(1); // N64: up is positive
}
