//
// Created by . on 6/1/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_CLOCK_H
#define JSMOOCH_EMUS_GENESIS_CLOCK_H

#include "helpers/int.h"

struct genesis_clock {
    u64 master_cycle_count;
    u64 master_frame;
    u64 frames_since_reset;

    u32 mem_break;

    struct {
        i32 hcount, vcount;
        u32 hblank, vblank;
        u32 hblank_fast;
        u32 field;
        i32 clock_divisor;
        i32 cycles_til_clock;
        u32 line_size;
        u32 line_mclock;
    } vdp;

    struct {
        i32 clock_divisor;
        i32 cycles_til_clock;
    } m68k;

    struct {
        i32 clock_divisor;
        i32 cycles_til_clock;
    } z80;

    struct {
        u32 cycles_per_second;
        u32 cycles_per_frame;
        u32 fps;
    } timing;
};

/*
  PAL  53.203Mhz, /7 = ~7.6004MHz M68k
                 /15 =  3.546894 MHz Z80
                 /4 =  ~13MHz       VDP
  NTSC 53.6931Mhz /7 = ~7.6704MHz M68k
                 /15 = 3.579545 MHz Z80,
                 / 4 = ~13MHz VDP
 */

void genesis_clock_init(struct genesis_clock* );
void genesis_clock_reset(struct genesis_clock*);

#endif //JSMOOCH_EMUS_GENESIS_CLOCK_H
