// OS function stubs


#include "padmgr.h"
#include "controller.h"
#include "ultra64.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "dma.h"

#include "s30/vrom_table.h"
#include "s30_input.h"
#include "s30_platform.h"
#define NORETURN




// Sound stub threading shit
u64 aspMainDataEnd[1] = {0};
u64 aspMainDataStart[1] = {0};

// Sequence-to-soundfont lookup table (extracted from sequence_font_table.o, u16 offsets swapped to LE)
// First 220 bytes: u16 offsets for 110 sequences, remaining bytes: font count + font IDs
u8 gSequenceFontTable[448] = {
    0xDC, 0x00, 0xDF, 0x00, 0xE1, 0x00, 0xE3, 0x00, 0xE5, 0x00, 0xE7, 0x00, 0xE9, 0x00, 0xEB, 0x00,
    0xED, 0x00, 0xEF, 0x00, 0xF1, 0x00, 0xF3, 0x00, 0xF5, 0x00, 0xF7, 0x00, 0xF9, 0x00, 0xFB, 0x00,
    0xFD, 0x00, 0xFF, 0x00, 0x01, 0x01, 0x03, 0x01, 0x05, 0x01, 0x07, 0x01, 0x09, 0x01, 0x0B, 0x01,
    0x0D, 0x01, 0x0F, 0x01, 0x11, 0x01, 0x13, 0x01, 0x15, 0x01, 0x17, 0x01, 0x19, 0x01, 0x1B, 0x01,
    0x1D, 0x01, 0x1F, 0x01, 0x21, 0x01, 0x23, 0x01, 0x25, 0x01, 0x27, 0x01, 0x29, 0x01, 0x2B, 0x01,
    0x2D, 0x01, 0x2F, 0x01, 0x31, 0x01, 0x33, 0x01, 0x35, 0x01, 0x37, 0x01, 0x39, 0x01, 0x3B, 0x01,
    0x3D, 0x01, 0x3F, 0x01, 0x41, 0x01, 0x43, 0x01, 0x45, 0x01, 0x47, 0x01, 0x49, 0x01, 0x4B, 0x01,
    0x4D, 0x01, 0x4F, 0x01, 0x51, 0x01, 0x53, 0x01, 0x55, 0x01, 0x57, 0x01, 0x59, 0x01, 0x5B, 0x01,
    0x5D, 0x01, 0x5F, 0x01, 0x61, 0x01, 0x63, 0x01, 0x65, 0x01, 0x67, 0x01, 0x69, 0x01, 0x6B, 0x01,
    0x6D, 0x01, 0x6F, 0x01, 0x71, 0x01, 0x73, 0x01, 0x75, 0x01, 0x77, 0x01, 0x79, 0x01, 0x7B, 0x01,
    0x7D, 0x01, 0x7F, 0x01, 0x81, 0x01, 0x83, 0x01, 0x85, 0x01, 0x87, 0x01, 0x89, 0x01, 0x8B, 0x01,
    0x8D, 0x01, 0x8F, 0x01, 0x91, 0x01, 0x93, 0x01, 0x95, 0x01, 0x97, 0x01, 0x99, 0x01, 0x9B, 0x01,
    0x9D, 0x01, 0x9F, 0x01, 0xA1, 0x01, 0xA3, 0x01, 0xA5, 0x01, 0xA7, 0x01, 0xA9, 0x01, 0xAB, 0x01,
    0xAD, 0x01, 0xAF, 0x01, 0xB1, 0x01, 0xB3, 0x01, 0xB5, 0x01, 0xB7, 0x01, 0x02, 0x01, 0x00, 0x01,
    0x02, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01,
    0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01,
    0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x0B, 0x01,
    0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x04, 0x01, 0x05, 0x01, 0x06, 0x01, 0x03, 0x01, 0x23, 0x01,
    0x03, 0x01, 0x23, 0x01, 0x03, 0x01, 0x23, 0x01, 0x12, 0x01, 0x07, 0x01, 0x08, 0x01, 0x09, 0x01,
    0x09, 0x01, 0x0A, 0x01, 0x03, 0x01, 0x0C, 0x01, 0x03, 0x01, 0x1E, 0x01, 0x0D, 0x01, 0x0E, 0x01,
    0x03, 0x01, 0x03, 0x01, 0x12, 0x01, 0x12, 0x01, 0x12, 0x01, 0x12, 0x01, 0x12, 0x01, 0x03, 0x01,
    0x23, 0x01, 0x09, 0x01, 0x03, 0x01, 0x0F, 0x01, 0x09, 0x01, 0x05, 0x01, 0x10, 0x01, 0x11, 0x01,
    0x11, 0x01, 0x11, 0x01, 0x03, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x01,
    0x00, 0x01, 0x03, 0x01, 0x09, 0x01, 0x08, 0x01, 0x13, 0x01, 0x14, 0x01, 0x09, 0x01, 0x15, 0x01,
    0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x03, 0x01, 0x16, 0x01, 0x13, 0x01, 0x09, 0x01, 0x17, 0x01,
    0x12, 0x01, 0x24, 0x01, 0x18, 0x01, 0x19, 0x01, 0x13, 0x01, 0x20, 0x01, 0x1B, 0x01, 0x1C, 0x01,
    0x1D, 0x01, 0x03, 0x01, 0x1F, 0x01, 0x20, 0x01, 0x20, 0x01, 0x09, 0x01, 0x21, 0x01, 0x22, 0x01,
    0x21, 0x01, 0x09, 0x01, 0x20, 0x01, 0x03, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


u64 aspMainTextStart[1] = {0};

//u8 Sequence_109_Size[1] = {0}; // TODO: Determine proper type
//u8 Sequence_109_Start[1] = {0}; // TODO: Determine proper type

extern void flush_texture_cache_external(void);

// NOT Needed should verify. 
u64 gspF3DZEX2_NoN_PosLight_fifoTextStart[1] = {0};
u64 gspS2DEX2d_fifoTextStart[1] = {0};
u64 gspS2DEX2d_fifoDataStart[1] = {0};
s8 D_80009430 = 0;
vu8 gViConfigBlack = 0;
u8 gViConfigAdditionalScanLines = 0;
u32 gViConfigFeatures = 0;
f32 gViConfigXScale = 1.0f;
f32 gViConfigYScale = 1.0f;
u32 gViConfigMode = 0;
u8 gViConfigModeType = 0;
void ViConfig_UpdateVi(u32 black) {}
void ViConfig_UpdateBlack(void) {}

u64 rspbootTextStart[1] = {0};
u64 rspbootTextEnd[1] = {0};
u64 gspF3DZEX2_NoN_PosLight_fifoDataStart[1] = {0};

// RSP JPEG shit.
u64 njpgdspMainTextStart[1] = {0};
u64 njpgdspMainTextEnd[1] = {0};
u64 njpgdspMainDataStart[1] = {0};
u64 njpgdspMainDataEnd[1] = {0};

// Likely important  and need to sort out.
//Mtx D_01000000 = {0};  // billboardMtx

//u16 D_0F000000[1] = {0};
//u16 D_0E000000[1] = {0};
// Debug font - moji
u64 gMojiFontTLUTs[4][4] = {0};
u64 gMojiFontTex[1] = {0};


// build 
const char gBuildCreator[] = "PC Port";
const char gBuildDate[] = "2025";

// this and other dma shit is in z_std_dma.c which im not compiling need to properly sort it later to correctly reimplement here. 
u32 gDmaMgrVerbose = 0;
size_t gDmaMgrDmaBuffSize = 0x2000;


// Math library
float __libm_qnan_f = 0.0f;
float qNaN0x10000 = NAN;

// OS variables
void* osRomBase = (void*)0x10000000;
s32 osResetType = 0;
s32 osAppNMIBuffer[16] = {0};
s32 osTvType = 1;
u32 osMemSize = 0x800000;


// Cache management
void osWritebackDCache(void* vaddr, s32 size) {}
void osWritebackDCacheAll(void) {}
void osInvalDCache(void* vaddr, s32 size) {}
void osInvalICache(void* vaddr, s32 size) {}

// Interrupt management  
/* "Interrupts off" = one global recursive lock. The game and audio threads
   run on separate cores; libultra's critical sections (message queues,
   timers) rely on this for mutual exclusion. */
static pthread_mutex_t sOsIntLock;
__attribute__((constructor)) static void OsIntLock_Init(void) {
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&sOsIntLock, &attr);
    pthread_mutexattr_destroy(&attr);
}
s32 __osDisableInt(void) {
    pthread_mutex_lock(&sOsIntLock);
    return 0;
}
void __osRestoreInt(s32 mask) {
    pthread_mutex_unlock(&sOsIntLock);
}

// Thread management (low-level only)
void __osEnqueueThread(OSThread** queue, OSThread* thread) {}
void __osEnqueueAndYield(OSThread** queue) {}
OSThread* __osPopThread(OSThread** queue) { return NULL; }
void __osDispatchThread(void) {}
void __osCleanupThread(void) {}

// Timer/counter
u32 osGetCount(void) {
    /* N64 CPU count register: 46.875 MHz */
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32)((u64)ts.tv_sec * 46875000u + (u64)ts.tv_nsec * 375 / 8000);
}
void __osSetCompare(u32 value) {}

// FPU control
u32 __osGetFpcCsr(void) { return 0; }
void __osSetFpcCsr(u32 value) {}

// Status register
u32 __osGetSR(void) { return 0; }
void __osSetSR(u32 value) {}

// Watch register
void __osSetWatchLo(u32 value) {}

// Cause register
u32 __osGetCause(void) { return 0; }

// Exception handling
u32 __osExceptionPreamble[1] = {0};


// Hardware interrupt table
__osHwInt __osHwIntTable[32] = {0};

// TLB
u32 __osProbeTLB(void* vaddr) { return 0; }
void osUnmapTLBAll(void) {}
void osMapTLBRdb(void) {}

// PI globals
OSPiHandle* gCartHandle = NULL;
OSMesgQueue gPiMgrCmdQueue = {0};

// N64 interrupt mask control - controls which hardware interrupts are enabled
u32 osSetIntMask(u32 mask) { return 0; }



// Debug printing stub to avoid spam. 
void osSyncPrintf(const char* fmt, ...) {
#if 1
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
#endif
}




// gVromTable is auto-generated in ascending vrom_start order with no
// overlapping ranges, so the segment containing `vrom` is the rightmost
// entry whose vrom_start <= vrom (then bounds-checked against vrom_end).
// Binary search: O(log n) over the ~1500 entries, run on every simulated DMA.
static int gVromCount = -1;

static const VromEntry* find_segment(uintptr_t vrom) {
    if (gVromCount < 0) {
        int n = 0;
        while (gVromTable[n].name != NULL) {
            n++;
        }
        gVromCount = n;
    }

    // Upper-bound: lo lands on the first entry with vrom_start > vrom.
    int lo = 0;
    int hi = gVromCount;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (gVromTable[mid].vrom_start <= vrom) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }

    if (lo == 0) {
        return NULL;
    }
    const VromEntry* cand = &gVromTable[lo - 1];
    if (vrom < cand->vrom_end) {
        return cand;
    }
    /* VROM falls in the romalign padding between cand's (as-built size) end and the
       next segment's start. N64 hardware just DMAs those ROM bytes (e.g.
       z_message.c textboxBackgroundIdx*0x900 indexing past the two textures
       in message_texture_static). Charge the read to cand; the reader short-
       reads the .bin and zero-fills the rest. */
    if (lo < gVromCount && vrom < gVromTable[lo].vrom_start) {
        return cand;
    }
    return NULL;
}

/* Segment files (data/<name>.bin) are loaded whole on first access and kept
   in RAM; every ROM DMA is then a copy out of the cached image. */
typedef struct {
    u8* data;
    size_t size;
} SegmentImage;

static SegmentImage sSegmentImages[sizeof(gVromTable) / sizeof(gVromTable[0])];

static const SegmentImage* load_segment(const VromEntry* seg) {
    SegmentImage* img = &sSegmentImages[seg - gVromTable];
    char path[512];
    FILE* f;
    long fileSize;

    if (img->data != NULL) {
        return img;
    }
    snprintf(path, sizeof(path), "%s/%s.bin", gS30DataDir, seg->name);
    f = fopen(path, "rb");
    if (f == NULL) {
        printf("DMA FAIL: Could not open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    img->data = malloc(fileSize > 0 ? fileSize : 1);
    img->size = fread(img->data, 1, fileSize, f);
    fclose(f);
    if (img->size != (size_t)fileSize) {
        printf("DMA WARN: %s read %zu of %ld bytes\n", path, img->size, fileSize);
    }
    return img;
}

/* Copy from a RAM-resident segment image, zero-filling any part of the request
   that lies past the image (VROM alignment-gap reads, see find_segment). */
static void dma_copy_clamped(void* ram, const void* buf, size_t bufsize, uintptr_t offset, size_t size) {
    if (offset >= bufsize) {
        memset(ram, 0, size);
        return;
    }
    if (offset + size > bufsize) {
        size_t avail = bufsize - offset;
        memcpy(ram, (const u8*)buf + offset, avail);
        memset((u8*)ram + avail, 0, size - avail);
        return;
    }
    memcpy(ram, (const u8*)buf + offset, size);
}

s32 DmaMgr_DmaRomToRam(uintptr_t rom, void* ram, size_t size) {
    const VromEntry* seg = find_segment(rom);
    const SegmentImage* img;

    if (!seg) {
        printf("DMA FAIL: No segment for VROM 0x%08X\n", (unsigned int)rom);
        memset(ram, 0, size);
        return -1;
    }
    img = load_segment(seg);
    if (img == NULL) {
        memset(ram, 0, size);
        return -1;
    }
    /* Past-the-.bin bytes are zero-filled (N64 reads ROM padding / next file) */
    dma_copy_clamped(ram, img->data, img->size, rom - seg->vrom_start, size);
    return 0;
}

/*
 * Audio ROM cache: all audio .bin data loaded into RAM at boot.
 * Eliminates per-DMA file I/O (fopen/fseek/fread/fclose) from the audio path.
 *
 * The three audio segments are contiguous in VROM space:
 *   Audiobank:  0x19020 - 0x44DE0  (~176 KB)
 *   Audioseq:   0x44DE0 - 0x94860  (~319 KB)
 *   Audiotable: 0x94860 - 0x4E5BF0 (~4.3 MB)
 * Total: ~4.67 MB (0x4CCBD0 bytes)
 */
extern u8 _AudiobankSegmentRomStart[];
extern u8 _AudiobankSegmentRomEnd[];
extern u8 _AudioseqSegmentRomStart[];
extern u8 _AudioseqSegmentRomEnd[];
extern u8 _AudiotableSegmentRomStart[];
extern u8 _AudiotableSegmentRomEnd[];

#define AUDIO_ROM_BASE  ((uintptr_t)_AudiobankSegmentRomStart)
#define AUDIO_ROM_END   ((uintptr_t)_AudiotableSegmentRomEnd)

static u8* sAudioRomData;
static int sAudioRomLoaded = 0;

static int AudioRom_LoadSegment(const char* name, uintptr_t vromStart, uintptr_t vromEnd) {
    uintptr_t offset = vromStart - AUDIO_ROM_BASE;
    size_t size = vromEnd - vromStart;
    char path[256];

    snprintf(path, sizeof(path), "%s/%s.bin", gS30DataDir, name);

    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("AudioRom: Failed to open %s\n", path);
        return -1;
    }

    size_t nread = fread(sAudioRomData + offset, 1, size, f);
    fclose(f);

    if (nread != size) {
        printf("AudioRom: %s: read %zu bytes, expected %zu\n", name, nread, size);
        return -1;
    }

    printf("AudioRom: Loaded %s (%zu bytes) at cache offset 0x%X\n", name, size, (unsigned)offset);
    return 0;
}

void AudioRom_LoadAll(void) {
    uintptr_t needed = AUDIO_ROM_END - AUDIO_ROM_BASE;

    sAudioRomData = aligned_alloc(64, ALIGN64(needed));
    if (sAudioRomData == NULL) {
        printf("AudioRom: ERROR: cannot allocate 0x%X bytes\n", (unsigned)needed);
        return;
    }

    if (AudioRom_LoadSegment("Audiobank",
            (uintptr_t)_AudiobankSegmentRomStart, (uintptr_t)_AudiobankSegmentRomEnd) != 0)
        return;
    if (AudioRom_LoadSegment("Audioseq",
            (uintptr_t)_AudioseqSegmentRomStart, (uintptr_t)_AudioseqSegmentRomEnd) != 0)
        return;
    if (AudioRom_LoadSegment("Audiotable",
            (uintptr_t)_AudiotableSegmentRomStart, (uintptr_t)_AudiotableSegmentRomEnd) != 0)
        return;

    sAudioRomLoaded = 1;
    printf("AudioRom: All audio data resident in RAM (%u bytes)\n", (unsigned)needed);
}

/* Return a direct pointer into the audio ROM cache, or NULL if not resident.
   Avoids memcpy in the hot sample-data path. */
void* AudioRom_GetDirectPointer(u32 devAddr, u32 size) {
    if (sAudioRomLoaded && devAddr >= AUDIO_ROM_BASE && (devAddr + size) <= AUDIO_ROM_END) {
        return sAudioRomData + (devAddr - AUDIO_ROM_BASE);
    }
    return NULL;
}

s32 DmaMgr_AudioDmaHandler(OSPiHandle* pihandle, OSIoMesg* mb, s32 direction) {
    u32 devAddr = mb->devAddr;
    void* ramAddr = mb->dramAddr;
    u32 size = mb->size;

    if (sAudioRomLoaded && devAddr >= AUDIO_ROM_BASE && (devAddr + size) <= AUDIO_ROM_END) {
        memcpy(ramAddr, sAudioRomData + (devAddr - AUDIO_ROM_BASE), size);
    } else {
        DmaMgr_DmaRomToRam(devAddr, ramAddr, size);
    }

    if (mb->hdr.retQueue) {
        osSendMesg(mb->hdr.retQueue, NULL, OS_MESG_NOBLOCK);
    }

    return 0;
}



s32 DmaMgr_RequestSync(void* ram, uintptr_t vrom, size_t size) {
    return DmaMgr_DmaRomToRam(vrom, ram, size);
}


s32 DmaMgr_RequestAsync(DmaRequest* req, void* ram, uintptr_t vrom, size_t size, 
                        u32 unk, OSMesgQueue* queue, OSMesg msg) {
    s32 ret = DmaMgr_DmaRomToRam(vrom, ram, size);
    
    // Mark that DMA was started (PC does it synchronously)
    if (req != NULL) {
        req->vromAddr = vrom;
    }
    
    if (queue) {
        osSendMesg(queue, msg, OS_MESG_NOBLOCK);
    }
    return ret;
}



s32 DmaMgr_RequestSyncDebug(void* ram, uintptr_t vrom, size_t size, const char* file, int line) {
    if (vrom > 0x4000000) {
        printf("DMA: BAD VROM 0x%08X from %s:%d\n", (unsigned int)vrom, file, line);
        memset(ram, 0, size);
        return -1;
    }
    return DmaMgr_RequestSync(ram, vrom, size);
}


s32 DmaMgr_RequestAsyncDebug(DmaRequest* req, void* ram, uintptr_t vrom, size_t size, 
                             u32 unk, OSMesgQueue* queue, OSMesg msg, const char* file, int line) {
    if (vrom > 0x4000000) {
        printf("DMA: BAD VROM 0x%08X from %s:%d\n", (unsigned int)vrom, file, line);
        memset(ram, 0, size);
        if (queue) {
            osSendMesg(queue, msg, OS_MESG_NOBLOCK);
        }
        return -1;
    }
    return DmaMgr_RequestAsync(req, ram, vrom, size, unk, queue, msg);
}



void PC_UpdateInput(void) {
    Input* input = &gPadMgr.inputs[0];
    S30Pad pad;
    // Store previous state
    input->prev = input->cur;

    S30Input_Read(&pad);
    u16 buttons = pad.button;
    s8 stick_x = pad.stick_x;
    s8 stick_y = pad.stick_y;

    // Set current state
    input->cur.button = buttons;
    input->cur.stick_x = stick_x;
    input->cur.stick_y = stick_y;
    input->cur.errno = 0;


    // Calculate press/release 
    u16 buttonDiff = input->prev.button ^ input->cur.button;
    input->press.button |= (buttonDiff & input->cur.button);   // |= not =
    input->rel.button |= (buttonDiff & input->prev.button);    // |= not =
    input->press.stick_x += input->cur.stick_x - input->prev.stick_x;
    input->press.stick_y += input->cur.stick_y - input->prev.stick_y;
    PadUtils_UpdateRelXY(input);

}

