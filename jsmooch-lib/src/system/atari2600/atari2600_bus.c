//
// Created by . on 6/11/25.
//

#include "atari2600_bus.h"


void atari2600_CPU_run_cycle(atari2600* this)
{
    if (this->tia.cpu_RDY) return; // CPU is halted until next scanline

    M6502_cycle(&this->cpu);

    this->CPU_bus.Addr.u = this->cpu.pins.Addr & 0x1FFF;
    this->CPU_bus.RW = this->cpu.pins.RW;
    this->CPU_bus.D = this->cpu.pins.D;

    if (this->CPU_bus.Addr.a12) // cart. a12=1
        atari2600_cart_bus_cycle(&this->cart, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    else if ((this->CPU_bus.Addr.a9 && this->CPU_bus.Addr.a7) || this->CPU_bus.Addr.a7) // RIOT, RIOT RAM
        M6532_bus_cycle(&this->riot, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    else if (this->CPU_bus.Addr.a9 == 0) { // TIA
        TIA_bus_cycle(&this->tia, this->CPU_bus.Addr.u, &this->CPU_bus.D, this->CPU_bus.RW);
    }
    else {
        printf("\nMISSED ADDR2 %04x %d %d", this->CPU_bus.Addr.u, this->CPU_bus.Addr.a7, this->CPU_bus.Addr.a9);
    }
    this->cpu.pins.D = this->CPU_bus.D;
}
