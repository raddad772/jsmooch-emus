//
// Created by . on 6/19/25.
//

#include <string.h>
#include <stdio.h>

#include "component/gpu/huc6270/huc6270.h"
#include "huc6260.h"
#include "helpers/physical_io.h"


/* HUC6260 creates an NTSC-ish frame.
 * It drives VDC to get pixel info
 * VDC responds to some signals like HSYNC etc.
 */

void HUC6260_init(struct HUC6260 *this, struct scheduler_t *scheduler, struct HUC6270 *vdc0, struct HUC6270 *vdc1) {
    memset(this, 0, sizeof(*this));
    this->regs.clock_div = 4;
    //this->scheduler = scheduler;
    this->scheduler = scheduler;
    this->vdc0 = vdc0;
    this->vdc1 = vdc1;
    this->regs.frame_height = 262;
}

void HUC6260_delete(struct HUC6260 *this)
{

}

void HUC6260_reset(struct HUC6260 *this)
{

}


static void hsync(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6260 *this = (struct HUC6260 *)ptr;
    this->regs.hsync = key;
    HUC6270_hsync(this->vdc0, key);
}

static void vsync(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6260 *this = (struct HUC6260 *)ptr;
    this->regs.vsync = key;
    HUC6270_vsync(this->vdc0, key);
    //if (key) printf("\nVSYNC ON ON LINE %d", this->regs.y);
}

static void new_frame(struct HUC6260 *this)
{
    //printf("\nNEW FRAME!");
    this->regs.y = 0;
    this->master_frame++;
    this->cur_output = this->display->output[this->display->last_written];
    memset(this->cur_output, 0, 1024*240*2);
    this->display->last_written ^= 1;
    //vsync(this, 1, 0, 0);
}

static void frame_end(void *ptr, u64 key, u64 clock, u32 jitter)
{

}

static void schedule_scanline(void *ptr, u64 key, u64 cclock, u32 jitter)
{
    struct HUC6260 *this = (struct HUC6260 *)ptr;
    //printf("\nNEW LINE %lld", key);
    this->regs.y = key;
    u64 clock = cclock - jitter;

    hsync(this, 0, 0, 0);
    scheduler_only_add_abs(this->scheduler, clock + HUC6260_HSYNC_END, 1, this, &hsync, NULL);
    u32 next_line = key + 1;
    if (next_line == 262) next_line = 0;
    scheduler_only_add_abs_w_tag(this->scheduler, clock + HUC6260_CYCLE_PER_LINE, next_line, this, &schedule_scanline, NULL, 1);
    this->regs.line_start = clock;
    if (this->regs.y == 0) { // new frame stuff
        new_frame(this);
        scheduler_only_add_abs_w_tag(this->scheduler, clock + (HUC6260_CYCLE_PER_LINE * 262), 0, this, &frame_end, NULL, 2);
    }
    else if (this->regs.y == HUC6260_LINE_VSYNC_START)
        vsync(this, 0, 0, 0);
    else if (this->regs.y == HUC6260_LINE_VSYNC_END)
        vsync(this, 1, 0, 0);

    this->regs.line_start = clock;
    this->cur_line = this->cur_output + (this->regs.y * HUC6260_DRAW_CYCLES);
}

void HUC6260_schedule_first(struct HUC6260 *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs(this->scheduler, this->regs.clock_div, 0, this, &HUC6260_cycle, NULL);
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
                if (this->io.CTW.u != 0) printf("\nCGRAM WRITE %03x: %04x", this->io.CTA.u, this->io.CTW.u);
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

void HUC6260_cycle(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct HUC6260 *this = (struct HUC6260 *)ptr;
    HUC6270_cycle(this->vdc0);

    u64 cur = clock - jitter;
    u64 line_pos = cur - this->regs.line_start;
    if (line_pos < HUC6260_DRAW_END) {
        u32 pc = this->vdc0->regs.px_out;
        u16 c = this->CRAM[pc];
        // GRB hi-lo, 9-bit
        // so blue is low 3 bits
        u32 dp = line_pos - HUC6260_DRAW_START;
        for (u32 i = 0; i < this->regs.clock_div; i++) {
            if ((dp+i) >= HUC6260_DRAW_END) break;
            this->cur_line[dp+i] = c;
        }
    }
    scheduler_only_add_abs(this->scheduler, cur + this->regs.clock_div, 0, this, &HUC6260_cycle, NULL);
}
