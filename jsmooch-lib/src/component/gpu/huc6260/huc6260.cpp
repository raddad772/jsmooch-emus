//
// Created by . on 6/19/25.
//

#include <cstring>
#include <cstdio>

#include "component/gpu/huc6270/huc6270.h"
#include "huc6260.h"
#include "helpers/physical_io.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

/* HUC6260 creates an NTSC-ish frame.
 * It drives VDC to get pixel info
 * VDC responds to some signals like HSYNC etc.
 */

namespace HUC6260 {
chip::chip(scheduler_t *scheduler_in, HUC6270::chip *vdc0_in, HUC6270::chip *vdc1_in) :
    scheduler{scheduler_in}, vdc0{vdc0_in}, vdc1{vdc1_in} {
    regs.frame_height = 262;
    regs.next_frame_height = 262;
    regs.cycles_per_frame = CYCLE_PER_LINE * regs.frame_height;
}

void chip::reset()
{
    vdc0->regs.divisor = regs.clock_div;

}

void chip::hsync(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<HUC6260::chip *>(ptr);

    if (key) {
        DBG_EVENT_TH(th->dbg.events.HSYNC_UP);
    }
    th->regs.hsync = key;
    th->vdc0->hsync(key);
}

void chip::vsync(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<HUC6260::chip *>(ptr);
    if (key) {
        DBG_EVENT_TH(th->dbg.events.VSYNC_UP);
    }
    else {

    }
    th->regs.vsync = key;
    th->vdc0->vsync(key);
}

void chip::new_frame()
{
    //printf("\n6260 NEW FRAME!");
    debugger_report_frame(dbg.interface);
    regs.y = 0;
    master_frame++;
    cur_output = static_cast<u16 *>(display->output[display->last_written]);
    memset(cur_output, 0, 1024*240*2);
    display->last_written ^= 1;
    //vsync(th, 1, 0, 0);
}

void chip::frame_end(void *ptr, u64 key, u64 clock, u32 jitter)
{

}

void chip::schedule_scanline(void *ptr, u64 key, u64 cclock, u32 jitter)
{
    auto *th = static_cast<HUC6260::chip *>(ptr);
    //printf("\nNEW LINE %lld", key);
    th->regs.y = key;
    u64 clock = cclock - jitter;

    // Schedule next line
    u32 next_line = key + 1;
    if (next_line == th->regs.frame_height) next_line = 0;
    th->scheduler->only_add_abs(clock + HSYNC_DOWN, 0, th, &hsync, nullptr);
    th->scheduler->only_add_abs(clock + HSYNC_UP, 1, th, &hsync, nullptr);

    th->scheduler->only_add_abs_w_tag(clock + CYCLE_PER_LINE, next_line, th, &schedule_scanline, nullptr, 1);
    th->regs.line_start = clock;

    // New frame or vsync begin or end scheduling
    if (th->regs.y == 0) { // new frame stuff
        th->new_frame();
        th->scheduler->only_add_abs_w_tag(clock + (CYCLE_PER_LINE * th->regs.frame_height), 0, th, &frame_end, nullptr, 2);
    }
    else if (th->regs.y == LINE_VSYNC_START) {
        th->scheduler->only_add_abs(clock + LINE_VSYNC_POS, 0, th, &vsync, nullptr);
        th->regs.frame_height = th->regs.next_frame_height;
        th->regs.cycles_per_frame = CYCLE_PER_LINE * th->regs.frame_height;
    }
    else if (th->regs.y == LINE_VSYNC_END)
        th->scheduler->only_add_abs(clock + LINE_VSYNC_POS, 1, th, &vsync, nullptr);

    // Report th line to debugger interface
    debugger_report_line(th->dbg.interface, key);

    th->cur_line = th->cur_output + (th->regs.y * CYCLE_PER_LINE);
}

void chip::schedule_first()
{
    schedule_scanline(this, 0, 0, 0);
    scheduler->only_add_abs(regs.clock_div, 0, this, &pixel_clock, nullptr);
}

void chip::write(u32 maddr, u8 val)
{
    u32 addr = maddr & 7;
#ifdef TG16_LYCODER2
    dbg_printf("VCE WRITE %04X: %02X\n", maddr & 0x1FFF, val);
#endif
    switch(addr) {
        case 0: //CR
            io.DCC = val;
            regs.clock_div = 4 - (val & 3);
            if ((val & 3) == 3) {
                printf("\nWARN ILLEGAL DIVISOR");
                regs.clock_div = 2;
            }
            vdc0->regs.divisor = regs.clock_div;
            // Bit 2 = Frame height (0= 262, 1= 263 scanlines)
            regs.next_frame_height = ((val >> 2) & 1) ? 263 : 262;

            //Bit 7 = Color subcarrier enable (0= enabled, 1= disabled (B&W video))
            regs.bw = ((val >> 7) & 1) << 15;
            return;
        case 1: //CTA
            return;
        case 2:
            io.CTA.lo = val;
            return;
        case 3:
            io.CTA.hi = val & 1;
            return;
        case 4: //CDW
            DBG_EVENT(dbg.events.WRITE_CRAM);
            CRAM[io.CTA.u] = (val & 0xFF) | (CRAM[io.CTA.u] & 0x100);
            return;
        case 5:
            io.CTW.hi = val & 1;
            DBG_EVENT(dbg.events.WRITE_CRAM);
            CRAM[io.CTA.u] = ((val & 1) << 8) | (CRAM[io.CTA.u] & 0xFF);
            io.CTA.u = (io.CTA.u + 1) & 0x1FF;
            return;
        case 6:
        case 7:
            return;

    }
}

u8 chip::read(u32 maddr, u8 old)
{
    u32 addr = maddr & 7;
    u32 lo = (maddr & 1) ^ 1;

    switch(addr) {
        case 4: {// CDR
            return CRAM[io.CTA.u] & 0xFF;
        }
        case 5: {
            u32 la = io.CTA.u;
            io.CTA.u = (io.CTA.u + 1) & 0x1FF;
            return 0xFE | ((CRAM[la] >> 8) & 1);
        }
        default:
    }

    return 0xFF;
}

void chip::schedule_next_pixel_clock(u64 cur)
{
    // If /4 or /2 and we are at start of line inside hsync,
    // And we are on an uneven cycle,
    // We lengthen from 4-5 or 2-3 in order to sync pixel clock up.
    // Otherwise it would drift out of phase 1/4 or 1/2 each line
    u32 sdiv = regs.clock_div;
    static constexpr int stretch_cycle = 1;
    u32 line_pos = cur - regs.line_start;

    if (line_pos == stretch_cycle)
        sdiv++;
    scheduler->only_add_abs(cur + sdiv, 0, this, &pixel_clock, nullptr);
}

void chip::pixel_clock(void *ptr, u64 key, u64 clock, u32 jitter)
{
    auto *th = static_cast<chip *>(ptr);
    th->vdc0->cycle();

    u64 cur = clock - jitter;
    u64 line_pos = cur - th->regs.line_start;
    u32 pc = th->vdc0->regs.px_out;
    u16 c = th->CRAM[pc];
    // GRB hi-lo, 9-bit
    // so blue is low 3 bits
    if (pc == 0xFFFF) c = 0x1FF;
    /*if (regs.y == 4) {
        c = 0x1FF;
    }*/
    for (i32 i = 0; i < th->regs.clock_div; i++) {
        if ((line_pos+i) >= CYCLE_PER_LINE) break;
        th->cur_line[line_pos+i] = c | th->regs.bw;
    }

    th->schedule_next_pixel_clock(cur);
}
}