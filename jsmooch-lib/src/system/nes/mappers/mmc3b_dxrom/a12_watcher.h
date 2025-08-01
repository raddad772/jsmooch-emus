//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_A12_WATCHER_H
#define JSMOOCH_EMUS_A12_WATCHER_H

#include "helpers/int.h"

enum a12_r {
    A12_NOTHING,
    A12_RISE,
    A12_FALL
};

struct NES_clock;
struct NES_a12_watcher {
    i64 cycles_down;
    i64 last_cycle;
    u32 delay;

    struct NES_clock* clock;
};

void a12_watcher_init(struct NES_a12_watcher*, struct NES_clock* clock);
enum a12_r a12_watcher_update(struct NES_a12_watcher*, u32 addr);


#endif //JSMOOCH_EMUS_A12_WATCHER_H
