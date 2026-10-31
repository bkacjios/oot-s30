#include "ultra64.h"
#include "memory_utils.h"

/**
 * Copy `size` bytes from `src` to `dest`.
 * The memory regions must not overlap.
 */
void* MemCopy(void* dest, void* src, s32 size) {
    return memcpy(dest, src, size);
}

/**
 * Set `size` bytes starting at `dest` to value `val`.
 */
void* MemSet(void* dest, s32 val, s32 size) {
    return memset(dest, (u8)val, size);
}
