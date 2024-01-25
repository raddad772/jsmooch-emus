#ifndef _GB_H
#define _GB_H

#include "helpers/int.h"
#include "helpers/sys_interface.h"
#include "gb_enums.h"
#include "gb_clock.h"
#include "gb_bus.h"
#include "gb_ppu.h"
#include "gb_cpu.h"
#include "cart.h"

void GB_new(struct jsm_system* system, enum GB_variants variant, struct JSM_IOmap *iomap);
void GB_delete(struct jsm_system* system);

struct GB_inputs {
    u32 a;
    u32 b;
    u32 start;
    u32 select;
    u32 up;
    u32 down;
    u32 left;
    u32 right;
};


struct GB {
    struct GB_bus bus;
    struct GB_clock clock;
    struct GB_CPU cpu;
    struct GB_PPU ppu;
    enum GB_variants variant;

    struct GB_cart cart;
    struct GB_inputs controller_in;
    i32 cycles_left;

    u8* BIOS;
    u32 BIOS_size;
};

#endif
