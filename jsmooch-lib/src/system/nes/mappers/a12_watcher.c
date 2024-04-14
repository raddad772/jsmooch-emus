//
// Created by Dave on 2/6/2024.
//

#include "a12_watcher.h"
#include "../nes_clock.h"

void a12_watcher_init(struct NES_a12_watcher* this, struct NES_clock* clock)
{
    this->cycles_down = 0;
    this->last_cycle = 0;
    this->clock = clock;
    this->delay = clock->timing.ppu_divisor * 3;
}

enum a12_r a12_watcher_update(struct NES_a12_watcher* this, u32 addr)
{
    enum a12_r result = A12_NOTHING;
    if (this->cycles_down > 0) {
        if (this->last_cycle > this->clock->ppu_frame_cycle) {
            this->cycles_down += (89342 - this->last_cycle) + (i64)this->clock->ppu_frame_cycle;
        } else {
            this->cycles_down += (i64)this->clock->ppu_frame_cycle - this->last_cycle;
        }
    }

    if ((addr & 0x1000) == 0) {
        if (this->cycles_down == 0) {
            this->cycles_down = 1;
            result = A12_FALL;
        }
    }
    else {
        if (this->cycles_down > this->delay) {
            result = A12_RISE;
        }
        this->cycles_down = 0;
    }
    this->last_cycle = (i64)this->clock->ppu_frame_cycle;

    return result;
}
