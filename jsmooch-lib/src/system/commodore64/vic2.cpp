//
// Created by . on 11/25/25.
//
#include <cassert>

#include "vic2.h"
#include "c64_bus.h"

#include "c64_vic2_color.h"

#include "helpers/enums.h"

namespace VIC2 {

core::core(C64::core *parent) : bus(parent) {
    setup_timing();
    for (u32 i = 0; i < 16; i++) {
        palette[i] = color::calculate_color(i, 1, 50.0f, 100.0f, 1.0f);
    }
}

void core::reset() {
    hpos = vpos = 0;
}

static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter) {
    auto *v = static_cast<VIC2::core *>(ptr);
    clock -= jitter;
    v->new_scanline();

    v->bus->scheduler.only_add_abs_w_tag(clock + v->timing.frame.cycles_per, 0, v, &schedule_scanline, nullptr, 2);
}

void new_frame(void *ptr, u64 key, u64 cur_clock, u32 jitter) {
    cur_clock -= jitter;

    auto *v = static_cast<VIC2::core *>(ptr);
    v->master_frame++;

    if (v->bus->dbg.events.view.vec) {
        debugger_report_frame(v->bus->dbg.interface);
    }
    v->cur_output = static_cast<u8 *>(v->display->output[v->display->last_written ^ 1]);
    memset(v->cur_output, 0, v->timing.frame.sz);
    v->display->last_written ^= 1;

    v->field ^= 1;
    v->vpos = -1;

    //vblank(v, 0, cur_clock, 0);

    v->bus->scheduler.only_add_abs_w_tag(cur_clock + v->timing.line.cycles_per, 0, v, &new_frame, nullptr, 2);
}

void core::new_scanline() {
    //timing.scanline_start = cur_clock;
    vpos++;
    if (bus->dbg.events.view.vec) {
        debugger_report_line(bus->dbg.interface, vpos);
    }
}

void core::schedule_first() {
    schedule_scanline(this, 0, 0, 0);
    bus->scheduler.only_add_abs_w_tag(timing.frame.cycles_per, 0, this, &new_frame, nullptr, 2);
    // TODO: schedule drawing...

}

void core::setup_timing() {
    switch (bus->display_standard) {
        case jsm::NTSC: {
            timing.line.cycles_per = 65 * 8;
            timing.line.pixels_per = 520;
            timing.frame.lines_per = 263;
            timing.frame.cycles_per = timing.line.cycles_per * timing.frame.lines_per;
            break;
        }
        case jsm::PAL: {
            timing.line.cycles_per = 63 * 8;
            timing.line.pixels_per = 504;
            timing.frame.lines_per = 312;
            timing.frame.cycles_per = timing.line.cycles_per * timing.frame.lines_per;
            break;
        }
        default:
            assert(1==2);
            return;
    }
    timing.frame.sz = timing.frame.lines_per * timing.line.pixels_per;
}

}
