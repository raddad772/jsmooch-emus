//
// Created by . on 6/19/25.
//

#include <string.h>
#include <stdio.h>

#include "component/gpu/huc6270/huc6270.h"
#include "huc6260.h"
#include "helpers/physical_io.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

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
    debugger_report_frame(this->dbg.interface);
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

    // Hsync end

    // Schedule hsync start

    // Schedule next line
    u32 next_line = key + 1;
    if (next_line == 262) next_line = 0;
    scheduler_only_add_abs(this->scheduler, clock + HUC6260_HSYNC_DOWN, 0, this, &hsync, NULL);
    scheduler_only_add_abs(this->scheduler, clock + HUC6260_HSYNC_UP, 1, this, &hsync, NULL);

    scheduler_only_add_abs_w_tag(this->scheduler, clock + HUC6260_CYCLE_PER_LINE, next_line, this, &schedule_scanline, NULL, 1);
    this->regs.line_start = clock;

    // New frame or vsync begin or end scheduling
    if (this->regs.y == 0) { // new frame stuff
        new_frame(this);
        scheduler_only_add_abs_w_tag(this->scheduler, clock + (HUC6260_CYCLE_PER_LINE * 262), 0, this, &frame_end, NULL, 2);
    }
    else if (this->regs.y == HUC6260_LINE_VSYNC_START)
        scheduler_only_add_abs(this->scheduler, clock + 30, 0, this, &vsync, NULL);
    else if (this->regs.y == HUC6260_LINE_VSYNC_END)
        scheduler_only_add_abs(this->scheduler, clock + 30, 1, this, &vsync, NULL);

    // Report this line to debugger interface
    debugger_report_line(this->dbg.interface, key);

    this->cur_line = this->cur_output + (this->regs.y * HUC6260_DRAW_CYCLES);
}

void HUC6260_schedule_first(struct HUC6260 *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs(this->scheduler, this->regs.clock_div, 0, this, &HUC6260_pixel_clock, NULL);
}

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
                if (this->io.CTW.u != 0) {
                    //dbg_break("YOHA", 0);
                }
                this->io.CTA.u = (this->io.CTA.u + 1) & 0x1FF;
            }
            return;
        case 3:
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
            if (lo) return this->CRAM[la] & 0xFF;
            this->io.CTA.u = (this->io.CTA.u + 1) & 0x1FF;
            return 0xFE | ((this->CRAM[la] >> 8) & 1);
        }
    }

    //printf("\nUnserviced HUC6260 (VCE) read: %06x", maddr);
    return 0xFF;
}

static void schedule_next_pixel_clock(struct HUC6260 *this, u64 cur)
{
    // If /4 or /2 and we are at start of line inside hsync,
    // And we are on an uneven cycle,
    // We lengthen from 4-5 or 2-3 in order to sync pixel clock up.
    // Otherwise it would drift out of phase 1/4 or 1/2 each line
    u32 sdiv = this->regs.clock_div;
    static const int stretch_cycle = 1;
    u32 line_pos = cur - this->regs.line_start;

    if (line_pos == stretch_cycle)
        sdiv++;
    scheduler_only_add_abs(this->scheduler, cur + sdiv, 0, this, &HUC6260_pixel_clock, NULL);
}

void HUC6260_pixel_clock(void *ptr, u64 key, u64 clock, u32 jitter)
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

    schedule_next_pixel_clock(this, cur);
}
