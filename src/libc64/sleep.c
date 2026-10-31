#include "ultra64.h"

#ifdef LINUX
#include <time.h>

/* osRecvMesg cannot block here and nothing services the libultra timer list,
   so the timer + message wait would return at once and leave this stack
   OSTimer linked in __osTimerList (the next insert then crashes): sleep for
   real instead. */
void Sleep_Cycles(OSTime cycles) {
    u64 ns = cycles * 1000000000ull / OS_CPU_COUNTER;
    struct timespec ts = { (time_t)(ns / 1000000000ull), (long)(ns % 1000000000ull) };

    while (nanosleep(&ts, &ts) != 0) {
    }
}
#else
void Sleep_Cycles(OSTime cycles) {
    OSMesgQueue mq;
    OSMesg msg;
    OSTimer timer;

    osCreateMesgQueue(&mq, &msg, 1);
    osSetTimer(&timer, cycles, 0, &mq, NULL);
    osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
}
#endif

void Sleep_Nsec(u32 nsec) {
    Sleep_Cycles(OS_NSEC_TO_CYCLES(nsec));
}

void Sleep_Usec(u32 usec) {
    Sleep_Cycles(OS_USEC_TO_CYCLES(usec));
}

// originally "msleep"
void Sleep_Msec(u32 ms) {
    Sleep_Cycles((ms * OS_CPU_COUNTER) / 1000ull);
}

void Sleep_Sec(u32 sec) {
    Sleep_Cycles(sec * OS_CPU_COUNTER);
}
