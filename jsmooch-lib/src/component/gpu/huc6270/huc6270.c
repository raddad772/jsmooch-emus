//
// Created by . on 6/18/25.
//

#include <string.h>
#include <printf.h>

#include "huc6270.h"
#include "component/gpu/huc6260/huc6260.h"

// collision, over, scanline, vertical blank
#define IRQ_COLLISION 1
#define IRQ_OVER 2
#define IRQ_SCANLINE 4
#define IRQ_VBLANK 8

/*
 * OK SO
 * this is "driven" by the VCE.
 * each clock just draws a pixel, no worry about timing.
 * BUT! it's a state machine that expects things to happen at a time.
 * HSW is horixontal sync window, it expects hsync within this time. if it doesn't get it it just moves on
 * if it gets it, the window ends, it moves on
 * if it gets it OUTSIDE THIS WINDOW, the whole line is aborted and we move to next
 * vsw works the same way
 * VCE gives these at fixed times, so...yeah.
 */

void HUC6270_init(struct HUC6270 *this)
{
    memset(this, 0, sizeof(*this));
}


void HUC6270_delete(struct HUC6270 *this)
{
}

void HUC6270_reset(struct HUC6270 *this)
{

}

static void update_irqs(struct HUC6270 *this);

static void update_RCR(struct HUC6270 *this)
{
    this->irq.IR &= ~IRQ_SCANLINE;
    if (this->regs.y == this->io.RCR.u) this->irq.IR |= IRQ_SCANLINE;
    update_irqs(this);
}

static void new_line(struct HUC6270 *this)
{
    this->regs.yscroll = this->regs.next_yscroll;
    this->regs.next_yscroll = this->regs.yscroll + 1;
    this->latch.sprites_on = this->io.CR.SB;
    this->latch.bg_on = this->io.CR.BB;
    this->regs.y++;

    update_RCR(this);

    // calculate line timing stuff
    
}

static void vblank(struct HUC6270 *this, u32 val)
{
    if (val) {
        this->irq.IR |= IRQ_VBLANK;
        update_irqs(this);
        this->bg.x_tiles = this->io.bg.x_tiles;
        this->bg.y_tiles = this->io.bg.y_tiles;
        this->bg.x_tiles_mask = this->bg.x_tiles - 1;
        this->bg.y_tiles_mask = this->bg.y_tiles - 1;
    }
    else {
        this->irq.IR &= ~IRQ_VBLANK;
        update_irqs(this);
    }
}

static void new_frame(struct HUC6270 *this)
{
    this->regs.y = -1; // return this +64
    this->regs.next_yscroll = this->io.BYR.u;
    // // on write BYR, set next_scroll also
}

static void run_cycle(void *ptr, u64 key, u64 clock, u32 jitter);



static void write_addr(struct HUC6270 *this, u32 val)
{
    printf("\nWRITE ADDR NOT IMPL!");
    this->io.ADDR = val & 0x1F;
}

static void update_irqs(struct HUC6270 *this)
{
    u32 cie = this->io.CR.IE;
    u32 old_line = this->irq.line;
    this->irq.line = (cie & this->irq.IR) != 0;
    if (old_line != this->irq.line)
        this->irq.update_func(this->irq.update_func_ptr, this->irq.line);
}

static void write_lsb(struct HUC6270 *this, u32 val)
{
    switch(this->io.ADDR) {
        case 0x00: // MAWR
            this->io.MAWR.lo = val;
            return;
        case 0x01: // MARR
            this->io.MARR.lo = val;
            return;
        case 0x02: // VWR
            this->io.VWR.lo = val;
            return;
        case 0x03:
            //this->io.VRR.lo = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", this->io.ADDR);
            return;
        case 0x05: // CR
            this->io.CR.lo = val;
            // TODO: do more stuff with this
            update_irqs(this);
            return;
        case 0x06:
            this->io.RCR.lo = val;
            update_RCR(this);
            return;
        case 0x07: // BGX
            this->io.BXR.lo = val;
            return;
        case 0x08: // BGY
            this->io.BYR.lo = val;
            this->regs.next_yscroll = this->io.BYR.u;
            return;
        case 0x09: {
            static const u32 screen_sizes[8][2] = { {32, 32}, {64, 32},
                                                    {128, 32}, {128, 32},
                                                    {32, 64}, {64, 64},
                                                    {128, 64}, {128,64}
                                                    };
            u32 sr = (val >> 4) & 7;
            this->io.bg.x_tiles = screen_sizes[sr][0];
            this->io.bg.y_tiles = screen_sizes[sr][1];
            return; }
        case 0x0A:
            this->io.HSW = val & 0x1F;
            return;
        case 0x0B:
            this->io.HDW = val & 0x7F;
            return;
        case 0x0C:
            this->io.VSW = val & 0x1F;
            return;
        case 0x0D:
            this->io.VDW.lo = val;
            return;
        case 0x0E:
            this->io.VCR = val;
            return;
        case 0x0F:
            // TODO: this!
            return;
        case 0x10:
            this->io.SOUR.lo = val;
            return;
        case 0x11:
            this->io.DESR.lo = val;
        case 0x12:
            this->io.LENR.lo = val;
            return;
        case 0x13:
            this->io.DVSSR.lo = val;
            return;
    }
    printf("\nWRITE LSB NOT IMPL: %02x", this->io.ADDR);
}

static void read_vram(struct HUC6270 *this)
{
    this->io.VRR.u = this->VRAM[this->io.MARR.u & 0x7FFF];
    this->io.MARR.u += this->regs.vram_inc;
    // TODO: timing
}

static void write_vram(struct HUC6270 *this)
{
    this->VRAM[this->io.MAWR.u & 0x7FFF] = this->io.VWR.u;
    this->io.MAWR.u += this->regs.vram_inc;
}

static void write_msb(struct HUC6270 *this, u32 val)
{
    switch(this->io.ADDR) {
        case 0x00: // MAWR
            this->io.MAWR.hi = val;
            return;
        case 0x01: // MARR
            this->io.MARR.hi = val;
            read_vram(this); // Read data into VRAM data transfer reg
            return;
        case 0x02: // VWR
            this->io.VWR.hi = val;
            write_vram(this);
            return;
        case 0x03:
            //this->io.VRR.hi = val;
            return;
        case 0x04:
            printf("\nWARN write reserved huc6270 register %d", this->io.ADDR);
            return;
        case 0x05:
            this->io.CR.hi = val;
            switch(this->io.CR.IW) {
                case 0:
                    this->regs.vram_inc = 1;
                    break;
                case 1:
                    this->regs.vram_inc = 0x20;
                    break;
                case 2:
                    this->regs.vram_inc = 0x40;
                    break;
                case 3:
                    this->regs.vram_inc = 0x80;
                    break;
            }
            return;
        case 0x06:
            this->io.RCR.hi = val & 3;
            update_RCR(this);
            return;
        case 0x07: // BGX
            this->io.BXR.hi = val & 3;
            return;
        case 0x08: // BGY
            this->io.BYR.hi = val & 1;
            this->regs.next_yscroll = this->io.BYR.u;
            return;
        case 0x09:
            return;
        case 0x0A:
            this->io.HDS = val & 0x7F;
            return;
        case 0x0B:
            this->io.HDE = val & 0x7F;
            return;
        case 0x0C:
            this->io.VDS = val;
            return;
        case 0x0D:
            this->io.VDW.hi = val & 1;
            return;
        case 0x0E:
            return;
        case 0x0F:
            return;
        case 0x10:
            this->io.SOUR.hi = val;
            return;
        case 0x11:
            this->io.DESR.hi = val;
            return;
        case 0x12:
            this->io.LENR.hi = val;
            return;
        case 0x13:
            this->io.DVSSR.hi = val;
            return;
    }
    printf("\nWRITE MSB NOT IMPL: %02x", this->io.ADDR);
}

void HUC6270_write(struct HUC6270 *this, u32 addr, u32 val)
{
    addr &= 3;
    switch(addr) {
        case 0:
            write_addr(this, val);
            return;
        case 1:
            printf("\nUnserviced HUC6270 (VDC) write %d: %02x", addr, val);
            return;
        case 2:
            write_lsb(this, val);
            return;
        case 3:
            write_msb(this, val);
            return;
    }
}

static u32 read_lsb(struct HUC6270 *this)
{
    printf("\nREAD LSB NOT IMPL!");
    return 0;
}

static u32 read_msb(struct HUC6270 *this)
{
    printf("\nREAD MSB NOT IMPL!");
    return 0;
}

static u32 read_status(struct HUC6270 *this)
{
    u32 v = this->io.STATUS.u;
    this->io.STATUS.u &= 0b1000000;
    return v;
}

u32 HUC6270_read(struct HUC6270 *this, u32 addr, u32 old)
{
    addr &= 3;
    switch(addr) {
        case 0:
            return read_status(this);
        case 1:
            printf("\nUnserviced HUC6270 (VDC) read %d", addr);
            return old;
        case 2:
            return read_lsb(this);
        case 3:
            return read_msb(this);
    }
    printf("\nUnserviced HUC6270 (VDC) read: %06x", addr);
    return old;
}
