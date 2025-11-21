#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"

namespace CDP1861 {

struct bus {
    u8 DMA_OUT;
    u8 SC;
    u8 D;
    u8 IRQ;
    u8 EF1;
};

struct core {
    explicit core() = default;
    void reset();
    void cycle();

    bus bus{};
    u8 *cur_output{};
    u8 *line_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};
    u8 x{}, y{};
u64 master_frame;
private:
    void new_scanline();
    void new_frame();

    struct {
        u8 enabled{1};
    } io{};
};
};