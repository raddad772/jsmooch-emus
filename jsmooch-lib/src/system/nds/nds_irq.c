//
// Created by . on 1/20/25.
//

#include "nds_irq.h"
#include "nds_bus.h"

void NDS_eval_irqs_7(struct NDS *this)
{
    this->arm7.regs.IRQ_line = (!!(this->io.arm7.IE & this->io.arm7.IF)) & this->io.arm7.IME;
    ARM7TDMI_IRQcheck(&this->arm7, this->arm7_ins);
}

void NDS_eval_irqs_9(struct NDS *this)
{
    // Bit 21 can't go down while certain conditions are true
    //printf("\neval_irqs_9 IF:%08x IE:%08x IME:%d", this->io.arm9.IF, this->io.arm9.IE, this->io.arm9.IME);
    if (!(this->io.arm9.IF & (1 << 21)))
        this->io.arm9.IF |= NDS_GE_check_irq(this) << 21;
    this->arm9.regs.IRQ_line = (!!(this->io.arm9.IE & this->io.arm9.IF)) & this->io.arm9.IME;
    //printf("\nARM9 IRQ line:%d", this->arm9.regs.IRQ_line);
    ARM946ES_IRQcheck(&this->arm9, this->arm9_ins);
}

void NDS_eval_irqs(struct NDS *this)
{
    /*
0     LCD V-Blank
  1     LCD H-Blank
  2     LCD V-Counter Match
  3     Timer 0 Overflow
  4     Timer 1 Overflow
  5     Timer 2 Overflow
  6     Timer 3 Overflow
  7     NDS7 only: SIO/RCNT/RTC (Real Time Clock)
  8     DMA 0
  9     DMA 1
  10    DMA 2
  11    DMA 3
  12    Keypad
  13    GBA-Slot (external IRQ source) / DSi: None such
  14    Not used                       / DSi9: NDS-Slot Card change?
  15    Not used                       / DSi: dito for 2nd NDS-Slot?
  16    IPC Sync
  17    IPC Send FIFO Empty
  18    IPC Recv FIFO Not Empty
  19    NDS-Slot Game Card Data Transfer Completion
  20    NDS-Slot Game Card IREQ_MC
  21    NDS9 only: Geometry Command FIFO
  22    NDS7 only: Screens unfolding
  23    NDS7 only: SPI bus
  24    NDS7 only: Wifi    / DSi9: XpertTeak DSP
  25    Not used           / DSi9: Camera
  26    Not used           / DSi9: Undoc, IF.26 set on FFh-filling 40021Axh
  27    Not used           / DSi:  Maybe IREQ_MC for 2nd gamecard?
  28    Not used           / DSi: NewDMA0
  29    Not used           / DSi: NewDMA1
  30    Not used           / DSi: NewDMA2
  31    Not used           / DSi: NewDMA3     */
    this->arm7.regs.IRQ_line = (!!(this->io.arm7.IE & this->io.arm7.IF & 0x3FFF)) & this->io.arm7.IME;
    this->arm9.regs.IRQ_line = (!!(this->io.arm9.IE & this->io.arm9.IF & 0x3FFF)) & this->io.arm9.IME;
}

void NDS_update_IF7(struct NDS *this, u32 bitnum)
{
    u32 old_IF = this->io.arm7.IF;
    this->io.arm7.IF |= 1 << bitnum;
    if (old_IF != this->io.arm7.IF) {
        NDS_eval_irqs_7(this);
    }
    ARM7TDMI_IRQcheck(&this->arm7, this->arm7_ins);
}

void NDS_update_IF9(struct NDS *this, u32 bitnum)
{
    u32 old_IF = this->io.arm9.IF;
    this->io.arm9.IF |= 1 << bitnum;
    NDS_eval_irqs_9(this);
}

void NDS_update_IFs(struct NDS *this, u32 bitnum)
{
    NDS_update_IF7(this, bitnum);
    NDS_update_IF9(this, bitnum);
}
