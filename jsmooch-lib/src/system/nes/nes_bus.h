//
// Created by Dave on 2/5/2024.
//

#ifndef JSMOOCH_EMUS_NES_BUS_H
#define JSMOOCH_EMUS_NES_BUS_H

struct NES_bus {
    struct NES_clock* clock;
    struct NES* nes;

    // generic read/write etc. stuff goes here
};

struct NES;
void NES_bus_init(struct NES_bus* this, struct NES* nes, struct NES_clock* clock);

#endif //JSMOOCH_EMUS_NES_BUS_H
