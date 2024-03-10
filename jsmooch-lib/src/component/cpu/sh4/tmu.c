//
// Created by David Schneider on 3/8/24.
//

#include "tmu.h"
#include "sh4_interpreter.h"

/* A lot of the structure and logic here very closely follows Reicast */

static void UpdateTMUCounts(u32 ch)
{
    u32 irq_num = 10 + ch;

}

void TMU_reset(struct SH4* this)
{
    this->tmu.TOCR = this->tmu.TSTR = 0;
    this->tmu.TCOR[0] = this->tmu.TCOR[1] = this->tmu.TCOR[2] = 0xFFFFFFFF;
    this->tmu.TCR[0] = this->tmu.TCR[1] = this->tmu.TCR[2] = 0;

    UpdateTMUCounts(0);
    UpdateTMUCounts(1);
    UpdateTMUCounts(2);

    //write_TMU_TSTR(0, 0);

    //for (u32 i = 0; i < 3; i++)
        //write_TMU_TCNTch(i, 0xffffffff);
}

void TMU_write(struct SH4* this, u32 addr, u64 val, u32 sz, u32* success)
{

}

u64 TMU_read(struct SH4* this, u32 addr, u32 sz, u32* success)
{
    *success = 0;
    return 0;
}

