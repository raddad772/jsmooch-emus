#pragma once

#include "helpers/int.h"

namespace mac {
struct core;
struct via {
    explicit via(core *parent) : bus(parent) {}
    u16 read(u32 addr, u16 mask, u16 old, bool has_effect);
    void write(u32 addr, u16 mask, u16 val);
    core *bus;
    void reset();
    void step();
    void irq_sample();

    struct {
        u8 IRA{}, ORA{}; // Input and Output Register A
        u8 IRB{}, ORB{}; // Input and Output Register B
        u8 dirA{}, dirB{}; // Direction for A and B. 1 = output{}, 0 = input
        u16 T1C{}; // timer 1 count
        u16 T1L{}; // timer 1 latch
        u16 T2C{}; // timer 2 count
        u16 T2L{}; // write-only timer 2 latch
        u8 ACR{}, PCR{}; // Auxilliary and Peripheral control registers
        u8 IER{}, IFR{}; // Interrupt enable and flag registers
        u8 SR{}; // Keyboard shfit register

        u8 num_shifts{};
    } regs{};

    struct {
        u32 t1_active{}, t2_active{};
    } state{};
};

}