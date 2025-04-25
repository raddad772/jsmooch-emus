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
    printf("\nWARN SNES PPU WRITE NOT IN %04x %02x", addr, val);
    static int num = 0;
    num++;
    if (num == 10) {
        dbg_break("PPU WRITE TOOMANYBAD", snes->clock.master_cycle_count);
    }
}

u32 SNES_PPU_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
    struct SNES_PPU *this = &snes->ppu;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_read(snes, addr, old, has_effect); }
    printf("\nUNIMPLEMENTED PPU READ FROM %04x", addr);
    return 0;
}

// Two states: regular, or paused for RAM refresh.
// I think I'll just use a "ram_refresh" kinda function that just advances the clock!? Maybe?

static void new_scanline(struct SNES* this, u64 cur_clock)
{
    // TODO: this
    // TODO: hblank exit here too
    this->clock.ppu.scanline_start = cur_clock;
    this->clock.ppu.y++;
    this->r5a22.status.hirq_line = 0;
}

static void dram_refresh(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES *)ptr;
    scheduler_from_event_adjust_master_clock(&this->scheduler, 40);
}

static void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    snes->clock.ppu.hblank_active = key;

    R5A22_hblank(snes, key);
    // Draw layers and display

    // Do hblank DMA

    // etc.
    // TODO: all this
}

static inline u32 ch_hdma_is_active(struct R5A22_DMA_CHANNEL *ch)
{
    return ch->hdma_enable && !ch->hdma_completed;
}

static u32 hdma_is_finished(struct R5A22_DMA_CHANNEL *this)
{
    struct R5A22_DMA_CHANNEL *ch = this->next;
    while(ch != NULL) {
        if (ch_hdma_is_active(ch)) return 0;
        ch = ch->next;
    }
    return true;

}

static u32 hdma_reload_ch(struct SNES *snes, struct R5A22_DMA_CHANNEL *ch)
{
    u32 cn = 8;
    u32 data = SNES_wdc65816_read(snes, (ch->source_bank << 16 | ch->hdma_address), 0, 1);
    if ((ch->line_counter & 0x7F) == 0) {
        ch->line_counter = data;
        ch->hdma_address++;

        ch->hdma_completed = ch->line_counter == 0;
        ch->hdma_address = !ch->hdma_completed;

        if (ch->indirect) {
            cn += 8;
            data = SNES_wdc65816_read(snes, (ch->source_bank << 16) | ch->hdma_address, 0, 1);
            ch->hdma_address++;
            ch->indirect_address = data << 8;
            if (ch->hdma_completed && hdma_is_finished(ch)) return cn;

            cn += 8;
            data = SNES_wdc65816_read(snes, (ch->source_bank << 16) | ch->hdma_address, 0, 1);
            ch->hdma_address++;

            ch->indirect_address = (data << 8) | (ch->indirect_address >> 8);
        }
    }
    return cn;
}

static u32 hdma_setup_ch(struct SNES *snes, struct R5A22_DMA_CHANNEL *ch)
{
    ch->hdma_do_transfer = 1;
    if (!ch->hdma_enable) return 0;

    ch->dma_enable = 0; // Stomp on DMA if HDMA runs
    ch->hdma_enable = ch->source_address;
    ch->line_counter = 0;
    return hdma_reload_ch(snes, ch);
}

static void hdma_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    u32 cn = 8;
    for (u32 n = 0; n < 8; n++) {
        cn += hdma_setup_ch(snes, &snes->r5a22.dma.channels[n]);
    }
    scheduler_from_event_adjust_master_clock(&snes->scheduler, cn);
}

static void hdma(void *ptr, u64 key, u64 clock, u32 jitter)
{

}

static void assert_hirq(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    struct R5A22 *this = &snes->r5a22;
    this->status.hirq_line = key;
    R5A22_update_irq(snes);
}

static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (struct SNES*)ptr;
    u64 cur = clock - jitter;
    u32 vblank = this->clock.ppu.y >= 224;
    new_scanline(this, cur);

    // hblank DMA setup, 12-20ish
    if (!vblank) {
        scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hdma_setup_position, 0, this, &hdma_setup, NULL);
    }

    // hblank out - 108 ish
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hblank_stop, 0, this, &hblank, NULL);

    // DRAM refresh - ~510 ish
    this->clock.timing.line.dram_refresh = this->clock.rev == 1 ? 12 + 8 - (this->clock.master_cycle_count & 7) : 12 + (this->clock.master_cycle_count & 7);
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.dram_refresh, 0, this, &dram_refresh, NULL);


    // HIRQ
    if (this->r5a22.io.irq_enable) {
        if (!this->r5a22.io.hirq_enable) assert_hirq(this, 1, cur, 0);
        else
            this->ppu.hirq.sched_id = scheduler_only_add_abs(&this->scheduler, cur + ((this->r5a22.io.htime + 21) * 4), 1, this, &assert_hirq, &this->ppu.hirq.still_sched);
    }
    R5A22_update_irq(this);


    // HDMA - 1104 ish
    if (!vblank) {
        scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hdma_position, 0, this, &hdma, NULL);
    }

    // Hblank in - 1108 ish
    scheduler_only_add_abs(&this->scheduler, cur + this->clock.timing.line.hblank_start, 1, this, &hblank, NULL);

    // new line - 1364
    scheduler_only_add_abs_w_tag(&this->scheduler, cur + this->clock.timing.line.master_cycles, 0, this, &schedule_scanline, NULL, 1);
}

static void vblank(void *ptr, u64 key, u64 cur_clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    snes->clock.ppu.vblank_active = key;
    snes->r5a22.status.nmi_flag = key;
    if (!key) { // VBLANK off at start of frame
    }
    else { // VBLANK on partway through frame
    }
    R5A22_update_nmi(snes);
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
    vblank(this, 0, cur_clock, jitter);

    //set_clock_divisor(this);
    scheduler_only_add_abs(&this->scheduler, cur + (this->clock.timing.line.master_cycles * 224), 1, this, &vblank, NULL);
    scheduler_only_add_abs_w_tag(&this->scheduler, cur + this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}


void SNES_PPU_schedule_first(struct SNES *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs_w_tag(&this->scheduler, this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}