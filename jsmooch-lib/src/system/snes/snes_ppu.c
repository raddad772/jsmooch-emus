//
// Created by . on 4/21/25.
//

#include <string.h>
#include "snes_ppu.h"
#include "snes_bus.h"

void SNES_PPU_init(struct SNES *this)
{
    
}

void SNES_PPU_delete(struct SNES *this)
{
    
}

void SNES_PPU_reset(struct SNES *this)
{
    
}

void SNES_PPU_write(struct SNES *snes, u32 addr, u32 val, struct SNES_memmap_block *bl)
{
    struct SNES_PPU *this = &snes->ppu;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_write(snes, addr, val); }
    printf("\nWARN SNES PPU WRITE NOT IN");
}

u32 SNES_PPU_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    struct SNES_PPU *this = &snes->ppu;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_read(snes, addr, old, has_effect); }
    printf("UNIMPLEMENTED PPU READ FROM %04x", addr);
    return 0;
}

// Two states: regular, or paused for RAM refresh.
// I think I'll just use a "ram_refresh" kinda function that just advances the clock!? Maybe?

static void new_scanline(struct SNES* this, u64 cur_clock)
{
    // TODO: this
    // TODO: hblank exit here too
    this->clock.ppu.scanline_start = cur_clock;
}

static void dram_refresh(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES *)ptr;
    scheduler_from_event_adjust_master_clock(&this->scheduler, 40);
}

static void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    // Draw layers and display

    // Do hblank DMA

    // etc.
    // TODO: all this
}

static void hdma_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{

}

static void hdma(void *ptr, u64 key, u64 clock, u32 jitter)
{

}


static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES*)ptr;
    u64 cur = clock - jitter;
    u32 vblank = this->clock.ppu.y >= 224;

    // hblank DMA setup, 12-20ish
    if (!vblank) {
        scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hdma_setup_position, 0, this, &hdma_setup, NULL);
    }

    // hblank out - 108 ish
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hblank_stop, 0, this, &hblank, NULL);

    // DRAM refresh - ~510 ish
    this->clock.timing.line.dram_refresh = this->clock.rev == 1 ? 12 + 8 - (this->clock.master_cycle_count & 7) : 12 + (this->clock.master_cycle_count & 7);
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.dram_refresh, 0, this, &dram_refresh, NULL);

    // HDMA - 1104 ish
    if (!vblank) {
        scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hdma_position, 0, this, &hdma, NULL);
    }

    // Hblank in - 1108 ish
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hblank_start, 1, this, &hblank, NULL);

    // new line - 1364
    scheduler_only_add_abs_w_tag(&this->scheduler, cur + this->clock.timing.line.master_cycles, 0, this, &schedule_scanline, NULL, 1);
    new_scanline(this, cur);
}

static void new_frame(void* ptr, u64 key, u64 cur_clock, u32 jitter)
{
    u64 cur = cur_clock - jitter;
    //printf("\nNEW FRAME @%lld", cur);
    struct SNES* this = (struct SNES*)ptr;

    debugger_report_frame(this->dbg.interface);
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->clock.master_frame++;

    this->clock.ppu.field ^= 1;
    this->clock.ppu.y = 0;
    this->clock.ppu.vblank_active = 0;
    this->ppu.cur_output = this->ppu.display->output[this->ppu.display->last_written];
    memset(this->ppu.cur_output, 0, 256*224*2);
    this->ppu.display->last_written ^= 1;

    this->ppu.display->scan_y = 0;
    this->ppu.display->scan_x = 0;

    // vblank 1->0
    this->clock.ppu.vblank_active = 0;

    //set_clock_divisor(this);
    scheduler_only_add_abs_w_tag(&this->scheduler, cur + this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}


void SNES_PPU_schedule_first(struct SNES *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs_w_tag(&this->scheduler, this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}