/*
 * The N64 versions' boot and 64DD pieces the game calls (spec/spec: src/boot/cic6105.c, src/boot/z_locale.c,
 * src/n64dd, libleo), for an N64 build (VERSION=ntsc-1.0 and the like; Makefile.s30's PLATFORM_OBJECTS):
 * - no 64DD drive: the disk flags stay 0, so the game never calls into the drive code below;
 * - the CIC-6105 and its RSP boot task as on a retail cartridge: the values the anti-piracy checks look for
 *   (the fishing reel, z_fishing.c; Zelda's courtyard, z_en_zl2.c), no task run;
 * - the region of the US ROM (z_locale.c read it from the cartridge header): the language is English.
 */
#include "ultra64.h"
#include "cic6105.h"
#include "dma.h"
#include "n64dd.h"
#include "region.h"

// cic6105.c
u32 gCICBootMagic0 = 0xAD090010;
s32 osCicId = 6105;

void CIC6105_EnableAudio(void) {
}

void CIC6105_AddFaultClient(void) {
}

void CIC6105_RemoveFaultClient(void) {
}

void CIC6105_RunBootTask(void) {
}

// z_locale.c
s32 gCurrentRegion = REGION_US;

// 64DD: no drive, no disk
u8 D_80121210;
u8 D_80121211;
u8 D_80121212;
n64ddStruct_80121220* B_80121220;

u32 func_801C6E80(void) {
    return 0;
}

void func_801C6EA0(Gfx** gfxP) {
}

s32 func_801C70FC(void) {
    return 0;
}

void func_801C7268(void) {
}

s32 func_801C7658(void) {
    return 0;
}

s32 func_801C7818(void) {
    return 0;
}

// A file from the disk: the same file from the ROM
void func_801C7C1C(void* dest, s32 offset, s32 size) {
    DmaMgr_RequestSync(dest, offset, size);
}

void func_801C7E78(void) {
}

void n64dd_SetDiskVersion(s32 arg0) {
}
