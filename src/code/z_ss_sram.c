#include "ultra64.h"
#include "printf.h"
#include "ss_sram.h"

typedef struct SsSramContext {
    /* 0x00 */ OSPiHandle piHandle;
    /* 0x74 */ OSIoMesg ioMesg;
    /* 0x8C */ OSMesgQueue msgQueue;
} SsSramContext; // size = 0xA4

SsSramContext sSsSramContext = { 0 };

void SsSram_Init(s32 addr, u8 handleType, u8 handleDomain, u8 handleLatency, u8 handlePageSize, u8 handleRelDuration,
                 u8 handlePulse, u32 handleSpeed) {
    u32 prevInt;
    OSPiHandle* handle = &sSsSramContext.piHandle;

    if ((u32)OS_PHYSICAL_TO_K1(addr) != (*handle).baseAddress) {
        sSsSramContext.piHandle.type = handleType;
        (*handle).baseAddress = (u32)OS_PHYSICAL_TO_K1(addr);
        sSsSramContext.piHandle.latency = handleLatency;
        sSsSramContext.piHandle.pulse = handlePulse;
        sSsSramContext.piHandle.pageSize = handlePageSize;
        sSsSramContext.piHandle.relDuration = handleRelDuration;
        sSsSramContext.piHandle.domain = handleDomain;
        sSsSramContext.piHandle.speed = handleSpeed;

        bzero(&sSsSramContext.piHandle.transferInfo, sizeof(__OSTranxInfo));

        prevInt = __osDisableInt();
        sSsSramContext.piHandle.next = __osPiTable;
        __osPiTable = &sSsSramContext.piHandle;
        __osRestoreInt(prevInt);

        sSsSramContext.ioMesg.hdr.pri = OS_MESG_PRI_NORMAL;
        sSsSramContext.ioMesg.hdr.retQueue = &sSsSramContext.msgQueue;
        sSsSramContext.ioMesg.devAddr = addr;
    }
}

void SsSram_Dma(void* dramAddr, size_t size, s32 direction) {
    OSMesg msg;

    osCreateMesgQueue(&sSsSramContext.msgQueue, &msg, 1);
    sSsSramContext.ioMesg.dramAddr = dramAddr;
    sSsSramContext.ioMesg.size = size;
    osWritebackDCache(dramAddr, size);
    osEPiStartDma(&sSsSramContext.piHandle, &sSsSramContext.ioMesg, direction);
    osRecvMesg(&sSsSramContext.msgQueue, &msg, OS_MESG_BLOCK);
    osInvalDCache(dramAddr, size);
}

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sram.h"
#include "s30_platform.h"

/*
 * S30 SRAM: a 32 KiB RAM mirror of the N64 save SRAM, backed by one file
 * (gS30SaveFile). Loaded once; every write that changes the mirror rewrites
 * the whole file through a temporary + fsync + rename, so a power cut during
 * a save leaves the previous save intact.
 */
static u8 sSram[SRAM_SIZE];
static u8 sSramLoaded = 0;

static void S30Sram_Load(void) {
    FILE* f = fopen(gS30SaveFile, "rb");
    size_t n = 0;

    memset(sSram, 0, SRAM_SIZE);
    if (f == NULL) {
        printf("SRAM: no %s yet, starting blank\n", gS30SaveFile);
        return;
    }
    n = fread(sSram, 1, SRAM_SIZE, f);
    fclose(f);
    printf("SRAM: loaded %zu bytes from %s\n", n, gS30SaveFile);
}

static void S30Sram_Store(void) {
    char tmp[512];
    int fd;
    ssize_t n;

    snprintf(tmp, sizeof(tmp), "%s.tmp", gS30SaveFile);
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        printf("SRAM: cannot open %s for writing\n", tmp);
        return;
    }
    n = write(fd, sSram, SRAM_SIZE);
    if (n != SRAM_SIZE || fsync(fd) != 0) {
        printf("SRAM: write to %s failed\n", tmp);
        close(fd);
        unlink(tmp);
        return;
    }
    close(fd);
    if (rename(tmp, gS30SaveFile) != 0) {
        printf("SRAM: rename to %s failed\n", gS30SaveFile);
        unlink(tmp);
        return;
    }
    printf("SRAM: saved to %s\n", gS30SaveFile);
}

void SsSram_ReadWrite(s32 addr, void* dramAddr, size_t size, s32 direction) {
    PRINTF("ssSRAMReadWrite:%08x %08x %08x %d\n", addr, dramAddr, size, direction);
    /* Callers pass OS_K1_TO_PHYSICAL(0xA8000000) + offset; undo it the same way
     * (the macro is the identity on this build, a real translation on N64). */
    u32 offset = (u32)addr - (u32)OS_K1_TO_PHYSICAL(0xA8000000);

    if (offset >= SRAM_SIZE || size > SRAM_SIZE - offset) {
        printf("SRAM: out-of-range access offset 0x%X size 0x%X\n", (unsigned)offset, (unsigned)size);
        return;
    }

    if (!sSramLoaded) {
        S30Sram_Load();
        sSramLoaded = 1;
    }

    if (direction == OS_READ) {
        memcpy(dramAddr, sSram + offset, size);
    } else if (direction == OS_WRITE) {
        if (memcmp(sSram + offset, dramAddr, size) == 0) {
            return; /* nothing changed (e.g. file-select header rewrites) */
        }
        memcpy(sSram + offset, dramAddr, size);
        S30Sram_Store();
    }
}
