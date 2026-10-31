#ifndef S30_INPUT_H
#define S30_INPUT_H

#include "ultra64.h"

// One N64 controller state built from the S30 gamepad
typedef struct S30Pad {
    u16 button; // BTN_* (N64 controller bits)
    s8 stick_x;
    s8 stick_y;
} S30Pad;

void S30Input_Init(void);
void S30Input_Read(S30Pad* pad);

#endif
