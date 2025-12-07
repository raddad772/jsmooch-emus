//
// Created by . on 6/19/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"
#include "helpers/debugger/debugger.h"
#include "helpers/debugger/debuggerdefs.h"
#include "helpers/physical_io.h"

namespace HUC6270 {
    struct chip;
}

namespace HUC6260 {
struct chip {
    explicit chip(scheduler_t *scheduler, HUC6270::chip *vdc0, HUC6270::chip *vdc1);
    void reset();
    static void hsync(void *ptr, u64 key, u64 clock, u32 jitter);
    static void vsync(void *ptr, u64 key, u64 clock, u32 jitter);
    static void frame_end(void *ptr, u64 key, u64 clock, u32 jitter);
    static void schedule_scanline(void *ptr, u64 key, u64 cclock, u32 jitter);
    static void pixel_clock(void *ptr, u64 key, u64 clock, u32 jitter);
    void schedule_first();
    void new_frame();
    void write(u32 maddr, u8 val);
    void schedule_next_pixel_clock(u64 cur);
    u8 read(u32 maddr, u8 old);
    u16 *cur_output{};
    u16 *cur_line{};
    u64 *master_clock{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};
    scheduler_t *scheduler{};

    HUC6270::chip *vdc0, *vdc1;
    u16 CRAM[512]{}; // 256 bg, 256 sprite
    u64 master_frame{};

    struct {
        u32 clock_div{4};
        u32 num_lines{}; // 262
        u32 y{};

        u64 line_start{};
        u32 hsync{}, vsync{};
        u32 frame_height{262}, next_frame_height{262}, bw{};
        u32 cycles_per_frame{};
    } regs{};

    struct {
        u32 DCC{};
        UN16 CTA{}, CTW{};
    } io{};

    DBG_START
        DBG_EVENT_VIEW_START
        HSYNC_UP{}, VSYNC_UP{}, WRITE_CRAM{}
        DBG_EVENT_VIEW_END

    DBG_END
};

static constexpr int CYCLE_PER_LINE = 1365;
/*
 * 237 master cycles hblank
 * 1128 draw cycles
 */

static constexpr int DRAW_CYCLES = 1128;
static constexpr int DRAW_START = 0;
static constexpr int HSYNC_DOWN = 1224;
static constexpr int HSYNC_UP = 96;
static constexpr int DRAW_END = 1128;
static constexpr int LINE_VSYNC_POS = (HSYNC_DOWN + 30) % CYCLE_PER_LINE;
static constexpr int LINE_VSYNC_START = 248; // 245/8
static constexpr int LINE_VSYNC_END = 251; // line to term on, so 246 and 249 = 3 lines
}