//
// Created by . on 12/4/24.
//

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
#include "gba_bus.h"

#include "gba_ppu.h"

void GBA_PPU_init(struct GBA *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));
}

void GBA_PPU_delete(struct GBA *this)
{

}

void GBA_PPU_reset(struct GBA *this)
{

}

static void vblank(struct GBA *this, u32 val)
{

}

static void hblank(struct GBA *this, u32 val)
{

}

static void new_frame(struct GBA *this)
{
    this->clock.ppu.vcount = 0;
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->clock.master_frame++;
    vblank(this, 0);
}

void GBA_PPU_start_scanline(struct GBA*this)
{
    hblank(this, 0);
    this->ppu.cur_pixel = 0;
    this->clock.ppu.scanline_start = this->clock.master_cycle_count;
}

void GBA_PPU_hblank(struct GBA*this)
{
    hblank(this, 1);
}

void GBA_PPU_finish_scanline(struct GBA*this)
{
    // do stuff, then

    this->clock.ppu.vcount++;
    if (this->clock.ppu.vcount == 160) {
        vblank(this, 1);
    }
    if (this->clock.ppu.vcount == 228) {
        new_frame(this);
    }
}