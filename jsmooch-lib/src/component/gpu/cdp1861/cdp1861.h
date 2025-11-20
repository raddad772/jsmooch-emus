#pragma once

#include "helpers/int.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"

namespace CDP1861 {

struct bus {
    u8 DMA_OUT;
    u8 D;
    u8 IRQ;
    u8 EF1;
};

struct core {
    explicit core() = default;
    void reset();
    void cycle();

    bus bus{};
    u16 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};

    u8 x{}, y{};

private:
    struct {
        u8 pos{};
        u8 data{};
    } shifter{};
};
};