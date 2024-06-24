//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_BUS_H
#define JSMOOCH_EMUS_GENESIS_BUS_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"
#include "helpers/cvec.h"
#include "component/cpu/z80/z80.h"
#include "component/cpu/m68000/m68000.h"

#include "genesis_vdp.h"
#include "genesis_clock.h"
#include "genesis_cart.h"

struct genesis {
    struct Z80 z80;
    struct M68k m68k;
    struct genesis_vdp vdp;
    struct genesis_clock clock;
    struct genesis_cart cart;

    u16 RAM[32768];
    u8 ARAM[8192];

    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        struct {
            u32 last_dtack;
        } m68k;
    } bus;
};

void genesis_cycle_m68k(struct genesis* this);
void genesis_cycle_z80(struct genesis* this);
void genesis_cycle_vdp(struct genesis* this);

#endif //JSMOOCH_EMUS_GENESIS_BUS_H
