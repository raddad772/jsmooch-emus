//
// Created by . on 5/2/24.
//

#pragma once
#include <variant>

#include "zxspectrum.h"
#include "helpers/int.h"

namespace ZXSpectrum {

struct core;
struct ULA {
    explicit ULA(variants variant_in, core *parent);
    void reset();
    void scanline_vblank();
    void scanline_border_bottom();
    void scanline_border_top();
    void scanline_visible();
    void new_scanline();
    void cycle();

    void (ULA::*scanline_func)();
    JSM_DISPLAY* display{};
    cvec_ptr<physical_io_device> display_ptr{};
    cvec_ptr<physical_io_device> keyboard_ptr{};
    variants variant{};
    core *bus{};

    bool first_vblank{true};
    i32 screen_x{};
    i32 screen_y{};
    u8* cur_output{};

    u32 bg_shift{};
    u32 next_bg_shift{};
    struct {
        u32 colors[2]{};
        u32 flash{};
    } attr{};
    u32 next_attr{};

    struct {
        u32 border_color{};
    } io{};

public:
    u8 reg_read(u16 addr);
    void reg_write(u16 addr, u8 val);

private:
    u8 kb_scan_row(const u32 *row);
    u32 get_keypress(JKEYS key);
};


}
