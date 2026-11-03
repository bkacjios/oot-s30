/*
 * Ocarina of Time's layout on the S30 gamepad (the shared library's input,
 * s30_input.h): Y -> B, B -> A, X -> C-left, A -> C-down, R1 -> C-right,
 * L1 -> L, L2 -> Z, R2 -> R, Select -> C-up, Start -> Start, D-pad -> N64
 * D-pad (D-pad item slots). Kept apart from the game's headers, whose
 * controller.h has BTN_* names of its own.
 */
#include <linux/input.h>

#include "s30_input.h"

const S30ButtonMap gS30Buttons[] = {
    { BTN_WEST, S30_N64_B },         { BTN_SOUTH, S30_N64_A },          { BTN_NORTH, S30_N64_CLEFT },
    { BTN_EAST, S30_N64_CDOWN },     { BTN_TR, S30_N64_CRIGHT },        { BTN_TL, S30_N64_L },
    { BTN_TL2, S30_N64_Z },          { BTN_TR2, S30_N64_R },            { BTN_SELECT, S30_N64_CUP },
    { BTN_START, S30_N64_START },    { BTN_DPAD_UP, S30_N64_DUP },      { BTN_DPAD_DOWN, S30_N64_DDOWN },
    { BTN_DPAD_LEFT, S30_N64_DLEFT }, { BTN_DPAD_RIGHT, S30_N64_DRIGHT },
};
const int gS30ButtonCount = sizeof(gS30Buttons) / sizeof(gS30Buttons[0]);
