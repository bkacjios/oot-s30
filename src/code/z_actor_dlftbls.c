#include "fault.h"
#include "attributes.h"
#include "printf.h"
#include "segment_symbols.h"
#include "z_actor_dlftbls.h"

#ifndef LINUX
// Linker symbol declarations (used in the table below) - NOT needed for PC
#define DEFINE_ACTOR(name, _1, _2, _3) DECLARE_OVERLAY_SEGMENT(name)
#define DEFINE_ACTOR_INTERNAL(_0, _1, _2, _3)
#define DEFINE_ACTOR_UNSET(_0)

#include "tables/actor_table.h"

#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET
#endif

// Profile declarations (also used in the table below)
#define DEFINE_ACTOR(name, _1, _2, _3) extern ActorProfile name##_Profile;
#define DEFINE_ACTOR_INTERNAL(name, _1, _2, _3) extern ActorProfile name##_Profile;
#define DEFINE_ACTOR_UNSET(_0)

#include "tables/actor_table.h"

#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

// Actor Overlay Table definition
#ifdef LINUX

// PC: All actors compiled in, treat as internal (no overlay loading)
#define DEFINE_ACTOR(name, _1, allocType, nameString) \
    { ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, nameString, allocType, 0, },

#define DEFINE_ACTOR_INTERNAL(name, _1, allocType, nameString) \
    { ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, nameString, allocType, 0, },

#define DEFINE_ACTOR_UNSET(_0) { 0 },

#elif DEBUG_FEATURES

#define DEFINE_ACTOR(name, _1, allocType, nameString) \
    {                                                 \
        ROM_FILE(ovl_##name),                         \
        _ovl_##name##SegmentStart,                    \
        _ovl_##name##SegmentEnd,                      \
        NULL,                                         \
        &name##_Profile,                              \
        nameString,                                   \
        allocType,                                    \
        0,                                            \
    },

#define DEFINE_ACTOR_INTERNAL(name, _1, allocType, nameString)                       \
    {                                                                                \
        ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, nameString, allocType, 0, \
    },

#define DEFINE_ACTOR_UNSET(_0) { 0 },

#else

// Actor name is set to NULL in retail builds
#define DEFINE_ACTOR(name, _1, allocType, _3) \
    {                                         \
        ROM_FILE(ovl_##name),                 \
        _ovl_##name##SegmentStart,            \
        _ovl_##name##SegmentEnd,              \
        NULL,                                 \
        &name##_Profile,                      \
        NULL,                                 \
        allocType,                            \
        0,                                    \
    },

#define DEFINE_ACTOR_INTERNAL(name, _1, allocType, _3)                         \
    {                                                                          \
        ROM_FILE_UNSET, NULL, NULL, NULL, &name##_Profile, NULL, allocType, 0, \
    },

#define DEFINE_ACTOR_UNSET(_0) { 0 },

#endif

ActorOverlay gActorOverlayTable[] = {
#include "tables/actor_table.h"
};

#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

s32 gMaxActorId = 0;

static FaultClient sFaultClient;

void ActorOverlayTable_LogPrint(void) {
#if DEBUG_FEATURES
    ActorOverlay* overlayEntry;
    u32 i;

    PRINTF("actor_dlftbls %u\n", gMaxActorId);
    PRINTF("RomStart RomEnd   SegStart SegEnd   allocp   profile  segname\n");

    for (i = 0, overlayEntry = &gActorOverlayTable[0]; i < (u32)gMaxActorId; i++, overlayEntry++) {
        PRINTF("%08x %08x %08x %08x %08x %08x %s\n", overlayEntry->file.vromStart, overlayEntry->file.vromEnd,
               overlayEntry->vramStart, overlayEntry->vramEnd, overlayEntry->loadedRamAddr, &overlayEntry->profile->id,
               overlayEntry->name != NULL ? overlayEntry->name : "?");
    }
#endif
}

void ActorOverlayTable_FaultPrint(UNUSED void* arg0, UNUSED void* arg1) {
    ActorOverlay* overlayEntry;
    u32 overlaySize;
    uintptr_t ramStart;
    uintptr_t ramEnd;
#if PLATFORM_N64
    u32 offset;
    uintptr_t pc = gFaultFaultedThread != NULL ? gFaultFaultedThread->context.pc : 0;
    uintptr_t ra = gFaultFaultedThread != NULL ? gFaultFaultedThread->context.ra : 0;
    u32 i;
#else
    s32 i;
#endif

#if PLATFORM_N64
    func_800AE1F8();

    Fault_Printf("actor_dlftbls %u\n", gMaxActorId);
    Fault_Printf("No.  RamStart-RamEnd   Offset\n");
#else
    Fault_SetCharPad(-2, 0);

    Fault_Printf("actor_dlftbls %u\n", gMaxActorId);
    Fault_Printf("No. RamStart- RamEnd cn  Name\n");
#endif

    for (i = 0, overlayEntry = &gActorOverlayTable[0]; i < gMaxActorId; i++, overlayEntry++) {
        overlaySize = (uintptr_t)overlayEntry->vramEnd - (uintptr_t)overlayEntry->vramStart;
        ramStart = (uintptr_t)overlayEntry->loadedRamAddr;
        ramEnd = ramStart + overlaySize;
#if PLATFORM_N64
        offset = (uintptr_t)overlayEntry->vramStart - ramStart;
#endif
        if (ramStart != 0) {
#if PLATFORM_N64
            Fault_Printf("%3d %08x-%08x %08x", i, ramStart, ramEnd, offset);
            if (ramStart <= pc && pc < ramEnd) {
                Fault_Printf(" PC:%08x", pc + offset);
            } else if (ramStart <= ra && ra < ramEnd) {
                Fault_Printf(" RA:%08x", ra + offset);
            }
            Fault_Printf("\n");
#else
            Fault_Printf("%3d %08x-%08x %3d %s\n", i, ramStart, ramEnd, overlayEntry->numLoaded,
                         (DEBUG_FEATURES && overlayEntry->name != NULL) ? overlayEntry->name : "");
#endif
        }
    }
}

#ifdef LINUX
/*
 * Overlay statics. On the N64 an overlay's .data and .bss come fresh from ROM
 * each time it is loaded (first instance spawned after the last one went), and
 * actors rely on it, e.g. to convert segmented pointers once per load into the
 * object's current address. Here actors are linked in, so the build puts each
 * overlay's .data and .bss in sections of its own (Makefile.s30) and they are
 * reset on load: .data from a copy taken the first time, .bss zeroed.
 */
#include <stdlib.h>
#include <string.h>

typedef struct OverlayStatics {
    u8* data;
    u8* dataEnd;
    u8* bss;
    u8* bssEnd;
} OverlayStatics;

#define DEFINE_ACTOR(name, _1, _2, _3)                          \
    extern u8 __start_ovl_data_##name[] __attribute__((weak)); \
    extern u8 __stop_ovl_data_##name[] __attribute__((weak));  \
    extern u8 __start_ovl_bss_##name[] __attribute__((weak));  \
    extern u8 __stop_ovl_bss_##name[] __attribute__((weak));
#define DEFINE_ACTOR_INTERNAL(_0, _1, _2, _3)
#define DEFINE_ACTOR_UNSET(_0)
#include "tables/actor_table.h"
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

#define DEFINE_ACTOR(name, _1, _2, _3) \
    { __start_ovl_data_##name, __stop_ovl_data_##name, __start_ovl_bss_##name, __stop_ovl_bss_##name },
#define DEFINE_ACTOR_INTERNAL(_0, _1, _2, _3) { 0 },
#define DEFINE_ACTOR_UNSET(_0) { 0 },
static const OverlayStatics sOverlayStatics[] = {
#include "tables/actor_table.h"
};
#undef DEFINE_ACTOR
#undef DEFINE_ACTOR_INTERNAL
#undef DEFINE_ACTOR_UNSET

static u8* sOverlayDataCopy[ACTOR_ID_MAX];
static u8 sOverlayLoaded[ACTOR_ID_MAX];

void ActorOverlayTable_LoadStatics(s32 id) {
    const OverlayStatics* ovl = &sOverlayStatics[id];
    size_t dataSize = ovl->dataEnd - ovl->data;

    if (sOverlayLoaded[id]) {
        return;
    }
    sOverlayLoaded[id] = true;
    if (dataSize != 0) {
        if (sOverlayDataCopy[id] == NULL) {
            // First load: .data is still as linked
            sOverlayDataCopy[id] = malloc(dataSize);
            if (sOverlayDataCopy[id] != NULL) {
                memcpy(sOverlayDataCopy[id], ovl->data, dataSize);
            }
        } else {
            memcpy(ovl->data, sOverlayDataCopy[id], dataSize);
        }
    }
    if (ovl->bssEnd != ovl->bss) {
        memset(ovl->bss, 0, ovl->bssEnd - ovl->bss);
    }
}

void ActorOverlayTable_UnloadStatics(s32 id) {
    sOverlayLoaded[id] = false;
}
#endif

void ActorOverlayTable_Init(void) {
    gMaxActorId = ACTOR_ID_MAX;
    Fault_AddClient(&sFaultClient, ActorOverlayTable_FaultPrint, NULL, NULL);
}

void ActorOverlayTable_Cleanup(void) {
#ifdef LINUX
    memset(sOverlayLoaded, 0, sizeof(sOverlayLoaded)); // the play state's arena goes, every overlay with it
#endif
    Fault_RemoveClient(&sFaultClient);
    gMaxActorId = 0;
}
