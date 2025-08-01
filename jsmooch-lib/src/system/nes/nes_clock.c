//
// Created by Dave on 2/5/2024.
//

#include "nes_clock.h"

void NES_clock_init(struct NES_clock* this)
{
    this->master_clock = this->master_frame = 0;

    this->frames_since_restart = 0;

    this->cpu_master_clock = this->apu_master_clock = this->ppu_master_clock = 0;
    this->trace_cycles = 0;

    this->nmi = 0;
    this->cpu_frame_cycle = this->ppu_frame_cycle = 0;
    this->timing.fps = 60;
    this->timing.apu_counter_rate = 60;
    this->timing.cpu_divisor = 12;
    this->timing.ppu_divisor = 4;
    this->timing.bottom_rendered_line = 239;
    this->timing.pixels_per_scanline = 280;
    this->timing.post_render_ppu_idle = 240;
    this->timing.hblank_start = 280;
    this->timing.vblank_start = 241;
    this->timing.vblank_end = 261;
    this->timing.ppu_pre_render = 261;
    this->ppu_y = 0;
    this->frame_odd = 0;
}

void NES_clock_reset(struct NES_clock* this)
{
    this->nmi = 0;
    this->master_clock = 0;
    this->master_frame = 0;
    this->cpu_master_clock = 0;
    this->ppu_master_clock = 0;
    //this->sound_master_clock = 0;
    //this->vblank = 0;
    this->ppu_y = 0;
    this->frame_odd = 0;
    this->frames_since_restart = 0;
}