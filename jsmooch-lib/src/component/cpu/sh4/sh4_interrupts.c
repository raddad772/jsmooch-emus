//
// Created by . on 5/1/24.
//

#include "sh4_interrupts.h"


static u32 INTEVT_TABLE[15] = {
        0x200,
        0x220,
        0x240,
        0x260,
        0x280,
        0x2A0,
        0x2C0,
        0x2E0,
        0x300,
        0x320,
        0x340,
        0x360,
        0x380,
        0x3A0,
        0x3C0
};


void SH4_exec_interrupt(struct SH4* this)
{
    this->regs.SPC = this->regs.PC;
    this->regs.SSR = SH4_regs_SR_get(&this->regs.SR);
    this->regs.SGR = this->regs.R[15];

    u32 old_SR = SH4_regs_SR_get(&this->regs.SR);

    /*this->regs.SR.MD = 1;
    this->regs.SR.RB = 1;
    this->regs.SR.BL = 1;*/
    SH4_SR_set(this, old_SR | (1 << 30) | (1 << 29) | (1 << 28));

    this->regs.PC = this->regs.VBR + 0x00000600;
}

void SH4_interrupt_IRL(struct SH4* this, u32 level) {
    this->regs.INTEVT = INTEVT_TABLE[level];
    SH4_exec_interrupt(this);
#ifdef SH4_DBG_SUPPORT
    if (dbg.trace_on) {
        dbg_printf("\nRaising interrupt %d cyc:%llu", level, this->clock.trace_cycles);
    }
#endif
}

// IPR0 = off
// IPR1 = lowest
// IPR16? = highest
// IRL0-15. priority is ~IRL

// A write to ICR, IPRA, IPRB or IPRC
void SH4_IPR_update(struct SH4* this)
{
    //TODO if (this->regs.ICR)
}