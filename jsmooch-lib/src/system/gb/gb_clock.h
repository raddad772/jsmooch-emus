#ifndef _GB_CLOCK_H
#define _GB_CLOCK_H

#include "helpers/int.h"
#include "gb_enums.h"

#define GB_CYCLES_PER_FRAME 70224

struct GB_clock {
    enum GB_PPU_modes ppu_mode; // = GB_PPU_modes.OAM_search
    u32 frames_since_restart;
    u32 master_frame;

    i32 cycles_left_this_frame; // = GB_CYCLES_PER_FRAME;

    u32 trace_cycles;

    u32 master_clock;
    u32 ppu_master_clock;
    u32 cpu_master_clock;

    u32 cgb_enable;
    u32 turbo;


    u32 ly;
    u32 lx;

    u32 wly;

    u32 cpu_frame_cycle;
    u32 CPU_can_VRAM; // = 1
    u32 old_OAM_can;
    u32 CPU_can_OAM;
    u32 bootROM_enabled; // = TRUE;

    u32 SYSCLK;

    struct {
        u32 ppu_divisor;// : u32 = 1
        u32 cpu_divisor;// : u32 = 4
        u32 apu_divisor; // : u32 = 4
    } timing;
};


void GB_clock_init(struct GB_clock*);
void GB_clock_setCPU_can_OAM(struct GB_clock*, u32 to);
void GB_clock_reset(struct GB_clock*);

#endif
