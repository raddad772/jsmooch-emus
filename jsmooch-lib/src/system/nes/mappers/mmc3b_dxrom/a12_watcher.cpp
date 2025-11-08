//
// Created by Dave on 2/6/2024.
//

#include "a12_watcher.h"
#include "../../nes_clock.h"

a12_r NES_a12_watcher::update(u32 addr)
{
    a12_r result = A12_NOTHING;
    if (cycles_down > 0) {
        if (last_cycle > clock->ppu_frame_cycle) {
            cycles_down += (89342 - last_cycle) + (i64)clock->ppu_frame_cycle;
        } else {
            cycles_down += (i64)clock->ppu_frame_cycle - last_cycle;
        }
    }

    if ((addr & 0x1000) == 0) {
        if (cycles_down == 0) {
            cycles_down = 1;
            result = A12_FALL;
        }
    }
    else {
        if (cycles_down > delay) {
            result = A12_RISE;
        }
        cycles_down = 0;
    }
    last_cycle = (i64)clock->ppu_frame_cycle;

    return result;
}
