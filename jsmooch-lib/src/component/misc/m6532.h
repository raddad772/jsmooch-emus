//
// Created by RadDad772 on 4/14/24.
//

#ifndef JSMOOCH_EMUS_M6532_H
#define JSMOOCH_EMUS_M6532_H

#include "helpers/int.h"

struct M6532 {
    u8 RAM[128];
};

void M6532_init(struct M6532* this);
void M6532_reset(struct M6532* this);
void M6532_bus_cycle(struct M6532* this, u32 addr, u32 *data, u32 rw);

#endif //JSMOOCH_EMUS_M6532_H
