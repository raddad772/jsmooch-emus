//
// Created by . on 3/4/25.
//

#include <string.h>

#include "ps1_bus.h"
#include "ps1_clock.h"

#define PS1_CYCLES_PER_SECOND_NTSC 33868800
#define PS1_CYCLES_PER_SECOND PS1_CYCLES_PER_SECOND_NTSC
#define PS1_FPS_NTSC 60
#define PS1_FPS PS1_FPS_NTSC
#define PS1_CYCLES_PER_FRAME (PS1_CYCLES_PER_SECOND / PS1_FPS)
#define PS1_VBLANK_CYCLE_IN_FRAME ((PS1_CYCLES_PER_FRAME*85)/100)
#define PS1_CYCLES_INSIDE_VBLANK_IN_FRAME (PS1_CYCLES_PER_FRAME - PS1_VBLANK_CYCLE_IN_FRAME)
#define PS1_CYCLES_PER_FRAME_PAL 677376

#define CPU_HZ this->timing.cpu.hz
#define GPU_HZ this->timing.gpu.hz
#define FPS this->timing.fps
#define FRAME_LINES this->timing.frame.lines
static void finish_timing(PS1_clock *this)
{
    this->timing.frame.cycles = CPU_HZ / (u64)FPS; // 564480
    this->timing.scanline.cycles = this->timing.frame.cycles / FRAME_LINES; // 2154
    this->timing.scanline.hblank.cycles = (u64)(this->timing.scanline.hblank.ratio * (float)this->timing.scanline.cycles);
    this->timing.scanline.hblank.start_on_cycle = this->timing.scanline.cycles - this->timing.scanline.hblank.cycles;
    // 262-243
    this->timing.frame.vblank.cycles = (u64)(this->timing.frame.vblank.ratio * (float)this->timing.frame.cycles);
    this->timing.frame.vblank.start_on_line = (this->timing.frame.cycles - this->timing.frame.vblank.cycles) / this->timing.scanline.cycles;
}


static void setup_ntsc(PS1_clock *this)
{
    // 3413
    CPU_HZ = 33868800;
    GPU_HZ = 53693175;
    FPS = 60.0f;
    FRAME_LINES = 263;
    // In the NTSC television standard, horizontal blanking occupies 10.9 μs out of every 63.6 μs scan line (17.2%).
    this->timing.scanline.hblank.ratio = 10.9f/63.6f;
    this->timing.frame.vblank.ratio = 20.0f/263.0f;
    finish_timing(this);
}

static void setup_pal(PS1_clock *this)
{
    CPU_HZ = 33868800;
    GPU_HZ = 53203425;
    FPS = 50.0f;
    FRAME_LINES = 314;
    this->timing.scanline.hblank.ratio = 12.0f/64.0f;
    this->timing.frame.vblank.ratio = 70.0f/314.0f;
    finish_timing(this);
    // In PAL, it occupies 12 μs out of every 64 μs scan line (18.8%).
}

void PS1_clock_init(PS1_clock *this, u32 is_ntsc)
{
    memset(this, 0, sizeof(*this));
    this->is_ntsc = is_ntsc;
    if (is_ntsc) setup_ntsc(this);
    else setup_pal(this);

}

void PS1_clock_reset(PS1 *this)
{
    PS1_clock_init(&this->clock, this->clock.is_ntsc);
}