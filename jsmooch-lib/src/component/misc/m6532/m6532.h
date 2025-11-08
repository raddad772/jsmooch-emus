//
// Created by RadDad772 on 4/14/24.
//

#ifndef JSMOOCH_EMUS_M6532_H
#define JSMOOCH_EMUS_M6532_H

#include "helpers/int.h"

struct M6532 {
    u8 RAM[128];

    struct {
        u32 cycle_mask;
        u32 counter;
        u32 reload_val;
        u32 highspeed_mode;
        u32 cycle;
    } timer;

    struct {
        u32 underflow_since_instat;
        u32 underflow_since_timnnt;

        u32 SWACNT, SWBCNT; // Direction control for...
        u32 SWCHA, SWCHB; // These!

    } io;
};

void M6532_init(struct M6532*);
void M6532_reset(struct M6532*);
void M6532_cycle(struct M6532*);
void M6532_bus_cycle(struct M6532*, u32 addr, u32 *data, u32 rw);

#endif //JSMOOCH_EMUS_M6532_H
