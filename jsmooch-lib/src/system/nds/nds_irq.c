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
}

void NDS_update_IF9(struct NDS *this, u32 bitnum)
{
    u32 old_IF = this->io.arm9.IF;
    this->io.arm9.IF |= 1 << bitnum;
    if (old_IF != this->io.arm9.IF) {
        NDS_eval_irqs_9(this);
    }
}

void NDS_update_IFs(struct NDS *this, u32 bitnum)
{
    NDS_update_IF7(this, bitnum);
    NDS_update_IF9(this, bitnum);
}
