//
// Created by RadDad772 on 3/8/24.
//

#ifndef JSMOOCH_EMUS_TMU_H
#define JSMOOCH_EMUS_TMU_H

#include "helpers/int.h"

// TMUs count down at 1/4 speed from the CPU


struct SH4_TMU {
    u64 base_clock;
};

struct SH4;
void TMU_init(SH4*);
void TMU_reset(SH4*);
void TMU_write(SH4*, u32 addr, u64 val, u32 sz, u32* success);
u64 TMU_read(SH4*, u32 addr, u32 sz, u32* success);

#endif //JSMOOCH_EMUS_TMU_H
