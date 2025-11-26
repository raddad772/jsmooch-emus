//
// Created by . on 11/25/25.
//
#pragma once

#include "helpers/int.h"
#include "helpers/scheduler.h"
#include "helpers/enums.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"

namespace C64 {
    struct core;
}

namespace VIC2 {

struct core {
    explicit core(C64::core *parent);
    jsm::display_standards display_standard=jsm::display_standards::NTSC;
    void setup_timing();
    void reset();
    void new_scanline();
    void write_IO(u16 addr, u8 val);
    u8 read_IO(u16 addr, u8 old, bool has_effect);

    u32 palette[16]{};
    u8 SCREEN_MTX[40];

    void schedule_first();
    struct {
        struct {
            u32 cycles_per{};
            u32 lines_per{};
            size_t sz;
        } frame{};

        struct {
            u32 cycles_per{};
            u32 pixels_per{};
        } line{};
    } timing{};
    i32 hpos{}, vpos{};

    u64 master_frame{};
    u32 field{};
    C64::core *bus;

    u8 *cur_output{};
    cvec_ptr<physical_io_device> display_ptr{};
    JSM_DISPLAY *display{};
    u8 AEC{};
private:

};


}
