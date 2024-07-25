//
// Created by . on 7/24/24.
//

#ifndef JSMOOCH_EMUS_MAC_INTERNAL_H
#define JSMOOCH_EMUS_MAC_INTERNAL_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "helpers/debug.h"
#include "helpers/physical_io.h"

#include "component/cpu/m68000/m68000.h"

#include "mac.h"

struct mac_clock {
    u64 master_cycle_count;
    u64 master_frame;
};

struct mac {
    enum mac_variants kind;
    struct M68k cpu;
    struct mac_clock clock;


    struct {
        struct cvec* IOs;
        u32 described_inputs;
        i64 cycles_left;
    } jsm;

    struct {
        struct {
            u16 last_read_data;
        } cpu;
    } io;
};

u16 mac_mainbus_read(struct mac* this, u32 addr, u32 UDS, u32 LDS, u16 old, u32 has_effect);

#endif //JSMOOCH_EMUS_MAC_INTERNAL_H
