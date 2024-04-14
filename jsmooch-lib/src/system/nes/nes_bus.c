//
// Created by Dave on 2/5/2024.
//

#include "nes_bus.h"

void NES_bus_init(struct NES_bus* this, struct NES* nes, struct NES_clock* clock)
{
    this->nes = nes;
    this->clock = clock;
}