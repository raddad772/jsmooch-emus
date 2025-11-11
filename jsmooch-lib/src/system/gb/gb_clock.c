#include "stdio.h"
#include "gb_clock.h"

void GB_clock_init(GB_clock* this) {
    this->ppu_mode = OAM_search;
    this->frames_since_restart = this->master_frame = this->trace_cycles = this->master_clock = this->ppu_master_clock = this->cpu_master_clock = 0;
    this->ly = this->lx = this->wly = this->cpu_frame_cycle = this->old_OAM_can = this->CPU_can_OAM = 0;

    this->cycles_left_this_frame = GB_CYCLES_PER_FRAME;

    this->CPU_can_VRAM = 1;
    this->bootROM_enabled = TRUE;

    this->timing.ppu_divisor = 1;
    this->timing.cpu_divisor = 4;
    this->timing.apu_divisor = 4;

    this->cgb_enable = this->turbo = FALSE;
}

void GB_clock_reset(GB_clock* this) {
    this->ppu_mode = 2;
    this->frames_since_restart = 0;
    this->master_clock = 0;
    this->cpu_master_clock = 0;
    this->ppu_master_clock = 0;
    this->lx = 0;
    this->ly = 0;
    this->timing.ppu_divisor = 1;
    this->timing.cpu_divisor = 4;
    this->cpu_frame_cycle = 0;
    this->CPU_can_VRAM = 1;
    this->CPU_can_OAM = 0;
    this->bootROM_enabled = TRUE;
    this->turbo = FALSE;
}

void GB_clock_setCPU_can_OAM(GB_clock* this, u32 to) {
    this->CPU_can_OAM = to;
    this->old_OAM_can = to;
}
