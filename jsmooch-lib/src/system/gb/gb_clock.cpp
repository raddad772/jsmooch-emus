#include "stdio.h"
#include "gb_clock.h"

namespace GB {

void clock::reset() {
    ppu_mode = PPU::OAM_search;
    frames_since_restart = 0;
    master_clock = 0;
    cpu_master_clock = 0;
    ppu_master_clock = 0;
    lx = 0;
    ly = 0;
    timing.ppu_divisor = 1;
    timing.cpu_divisor = 4;
    cpu_frame_cycle = 0;
    CPU_can_VRAM = true;
    CPU_can_OAM = false;
    bootROM_enabled = true;
    turbo = false;
}

void clock::setCPU_can_OAM(bool to) {
    CPU_can_OAM = to;
    old_OAM_can = to;
}
}