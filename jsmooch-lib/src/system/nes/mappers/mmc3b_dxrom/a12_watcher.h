//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_A12_WATCHER_H
#define JSMOOCH_EMUS_A12_WATCHER_H

#include "helpers/int.h"
#include "../../nes_clock.h"

enum a12_r {
    A12_NOTHING,
    A12_RISE,
    A12_FALL
};

struct NES_clock;
struct NES_a12_watcher {
    i64 cycles_down{};
    i64 last_cycle{};
    u32 delay;

    NES_clock* clock;

    NES_a12_watcher(NES_clock* clock) : clock(clock)
    { delay = clock->timing.ppu_divisor * 3; }

    a12_r update(u32 addr);
};


#endif //JSMOOCH_EMUS_A12_WATCHER_H
