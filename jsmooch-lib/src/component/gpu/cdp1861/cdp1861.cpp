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
}

void core::new_scanline() {
    x = 0;
    y++;
    switch (y) {
        // Configure EF, INT signals here...
        case 262:
            new_frame();
            break;
        default:
    }

    line_output = cur_output + (y * 112);
}

void core::cycle() {
    x++;
    if (x >= 14) {
        new_scanline();
    }
    // Beginning in third cycle, assert DMAO
    bool display_field = (y >= 0) && (y < 128);
    if (display_field && (x >= 3) && (x < 12)) {
        bus.DMA_OUT = io.enabled;;
    }
    else {
        bus.DMA_OUT = 0;
    }

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
};