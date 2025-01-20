//
// Created by . on 1/20/25.
//

#include "nds_irq.h"
#include "nds_bus.h"

void NDS_eval_irqs_7(struct NDS *this)
{
    this->arm7.regs.IRQ_line = (!!(this->io.arm7.IE & this->io.arm7.IF & 0x3FFF)) & this->io.arm7.IME;
}

void NDS_eval_irqs_9(struct NDS *this)
{
    this->arm9.regs.IRQ_line = (!!(this->io.arm9.IE & this->io.arm9.IF & 0x3FFF)) & this->io.arm9.IME;
}

void NDS_eval_irqs(struct NDS *this)
{
    this->arm7.regs.IRQ_line = (!!(this->io.arm7.IE & this->io.arm7.IF & 0x3FFF)) & this->io.arm7.IME;
    this->arm9.regs.IRQ_line = (!!(this->io.arm9.IE & this->io.arm9.IF & 0x3FFF)) & this->io.arm9.IME;
}

void NDS_update_IF7(struct NDS *this, u32 bitnum, u32 val)
{
    u32 old_IF = this->io.arm7.IF;
    this->io.arm7.IF |= val << bitnum;
    if (old_IF != this->io.arm7.IF) {
        NDS_eval_irqs_7(this);
    }
}

void NDS_update_IF9(struct NDS *this, u32 bitnum, u32 val)
{
    u32 old_IF = this->io.arm9.IF;
    this->io.arm9.IF |= val << bitnum;
    if (old_IF != this->io.arm9.IF) {
        NDS_eval_irqs_9(this);
    }
}

void NDS_update_IFs(struct NDS *this, u32 bitnum, u32 val)
{
    NDS_update_IF7(this, bitnum, val);
    NDS_update_IF9(this, bitnum, val);
}
