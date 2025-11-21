//
// Created by . on 11/20/25.
//

#include "cdp1861.h"

namespace CDP1861 {
/*
    assert:
    EFX four lines, INT two lines.


    */
void core::reset() {
    x = y = 0;
    line_output = cur_output;
}

void core::new_frame() {
    x = y = 0;
    cur_output = static_cast<u8 *>(display->output[display->last_written ^ 1]);
    master_frame++;
    line_output = cur_output;
    memset(cur_output, 0, 112*262);
    display->last_written ^= 1;

    io.enable = io.latch.enable;
}

void core::new_scanline() {
    x = 0;
    y++;
    switch (y) {
        // Configure EF, INT signals here...
        case 60:
            bus.EF1 = 1;
            break;
        case 62:
            bus.IRQ = 1;
            break;
        case 64:
            bus.IRQ = 0;
            bus.EF1 = 0;
            break;
        case 188:
            bus.EF1 = 1;
            break;
        case 192:
            bus.EF1 = 0;
            break;
        case 262:
            new_frame();
            break;
        default:
    }

    line_output = cur_output + (y * 112);
}

u8 core::INP(u8 old) {
    io.latch.enable = 1;
    return old;
}

void core::OUT(u8 val) {
    io.latch.enable = 0;
}

void core::cycle() {
    x++;
    if (x >= 14) {
        new_scanline();
    }
    // Beginning in third cycle, assert DMAO
    // line 60 start IRQ?
    // line 62 stop IRQ?
    // display start line 64, end line 192
    bool display_field = (y >= 64) && (y < 192);
    bus.DMA_OUT = io.enable && display_field && (x >= 3) && (x < 12);

    u8 data = 0;
    if (display_field && bus.SC == 2) {
        data = bus.D;
    }

    // Now draw 8 pixels out from data, MSB-first!
    u8 *outp = line_output + (x * 8);
    for (u32 i = 0; i < 8; i++) {
        u32 out = data >> 7;
        data >>= 1;
        *outp = out ? 1 : 0;
        outp++;
    }
}
}