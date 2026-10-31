#include "ultra64.h"
#include <string.h>

// Mtx is a plain float matrix in this port (gbi.h), not N64 s15.16 fixed point
void guMtxF2L(f32 mf[4][4], Mtx* m) {
    memcpy(m->m, mf, sizeof(m->m));
}

void guMtxL2F(f32 mf[4][4], Mtx* m) {
    memcpy(mf, m->m, sizeof(m->m));
}

void guMtxIdentF(f32 mf[4][4]) {
    s32 i, j;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            if (i == j) {
                mf[i][j] = 1.0;
            } else {
                mf[i][j] = 0.0;
            }
        }
    }
}

void guMtxIdent(Mtx* m) {
    f32 mf[4][4];

    guMtxIdentF(mf);

    guMtxF2L(mf, m);
}
