//
// Created by . on 6/19/25.
//

#include <string.h>
#include <printf.h>
#include "component/gpu/huc6270/huc6270.h"
#include "huc6260.h"
#include "helpers/physical_io.h"

/* HUC6260 creates an NTSC-ish frame.
 * It drives VDC to get pixel info
 * VDC responds to some signals like HSYNC etc.
 */

void HUC6260_init(struct HUC6260 *this, u64 *master_clock, struct HUC6270 *vdc0, struct HUC6270 *vdc1) {
    memset(this, 0, sizeof(*this));
    this->regs.clock_div = 4;
    //this->scheduler = scheduler;
    this->master_clock = master_clock;
    this->vdc0 = vdc0;
    this->vdc1 = vdc1;
}

void HUC6260_delete(struct HUC6260 *this)
{

}

void HUC6260_reset(struct HUC6260 *this)
{

}

/*static void schedule_self(struct HUC6260 *this, u64 clock)
{
    scheduler_add_or_run_abs(this->scheduler, clock + this->regs.clock_div, 0, this, &run_cycle, NULL);
}*/

void HUC6260_write(struct HUC6260 *this, u32 maddr, u32 val)
{
    u32 addr = (maddr >> 1) & 3;
    u32 lo = (maddr & 1) ^ 1;
    switch(addr) {
        case 0: //CR
            if (lo) {
                this->io.DCC = val;
                this->regs.clock_div = 4 - (val & 3);
                if ((val & 3) == 3) {
                    printf("\nWARN ILLEGAL DIVISOR");
                    this->regs.clock_div = 2;
                }
            }
            return;
        case 1: //CTA
            if (lo)
                this->io.CTA.lo = val;
            else
                this->io.CTA.hi = val & 1;
            return;
        case 2: //CDW
            if (lo)
                this->io.CTW.lo = val;
            else {
                this->io.CTW.hi = val & 1;
                this->CRAM[this->io.CTA.u] = this->io.CTW.u;
                this->io.CTA.u = (this->io.CTA.u + 1) & 0x1FF;
            }
            return;
        case 3:
            printf("\nUnserviced HUC6260 write: %06x %02x", maddr, val);
            return;
    }
}

u32 HUC6260_read(struct HUC6260 *this, u32 maddr, u32 old)
{
    u32 addr = (maddr >> 1) & 3;
    u32 lo = (maddr & 1) ^ 1;

    switch(addr) {
        case 2: {// CDR
            u32 la = this->io.CTA.u;
            this->io.CTA.u = (this->io.CTA.u + 1) & 0x1FF;
            if (lo) return this->CRAM[la] & 0xFF;
            else return 0xFE | ((this->CRAM[la] >> 8) & 1);
        }
    }

    printf("\nUnserviced HUC6260 (VCE) read: %06x", maddr);
    return 0xFF;
}

static void hblank(struct HUC6260 *this, u32 val)
{
    this->regs.hblank = val;
    HUC6270_hsync(this->vdc0, val);
}

static void vblank(struct HUC6260 *this, u32 val)
{
    this->regs.vblank = val;
    HUC6270_vsync(this->vdc0, val);
}

static void new_frame(struct HUC6260 *this)
{
    this->regs.y = 0;
    this->master_frame++;
    this->cur_output = this->display->output[this->display->last_written];
    memset(this->cur_output, 0, 1024*240*2);
    this->display->last_written ^= 1;
    vblank(this, 0);

}

static void new_line(struct HUC6260 *this)
{
    if (this->regs.y >= 261)
        new_frame(this);
    else
        this->regs.y++;
    hblank(this, 0);
    if (this->regs.y == 242) vblank(this, 1);
    this->regs.line_start += 1368;
    this->cur_line = this->cur_output + (this->regs.y * 1024);
}

void HUC6260_cycle(struct HUC6260 *this, u64 clock) {
    u64 line_pos = clock - this->regs.line_start;
    if (line_pos < 1024) {
        // hblank(0) done by new_line()
    }
    else if (line_pos < 1368) {
        if (!this->regs.hblank) hblank(this, 1);
    }
    else {
        new_line(this);
    }
    // Output a pixel, 10 bits
    u32 pc = this->vdc0->regs.px_out;
    u16 c = this->CRAM[pc];
    // GRB hi-lo, 9-bit
    // so blue is low 3 bits
    for (u32 i = 0; i < this->regs.clock_div; i++) {
        if ((line_pos+i) > 1023) break;
        this->cur_line[line_pos+i] = c;
    }
}

void HUC6270_schedule_first(struct HUC6260 *this)
{
    //schedule_self(this, 0);
}
