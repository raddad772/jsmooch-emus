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
#include "helpers/multisize_memaccess.c"

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

static void pprint_palette_ram(struct GBA *this)
{
    for (u32 i = 0; i < 512; i++) {
        u16 c = this->ppu.palette_RAM[i];
    }
}

static void vblank(struct GBA *this, u32 val)
{
    //pprint_palette_ram(this);
    this->clock.ppu.vblank_active = val;
    if (val == 1) {
        this->io.IF |= 1;
        GBA_eval_irqs(this);
    }
}

static void hblank(struct GBA *this, u32 val)
{
    this->clock.ppu.hblank_active = 1;
    if (val == 0) {
        if (this->ppu.io.vcount_at == this->clock.ppu.y) {
            this->io.IF |= 4;
            GBA_eval_irqs(this);
        }
    }
    if (val == 1) {
        this->io.IF |= 2;
        GBA_eval_irqs(this);
    }
}

static void new_frame(struct GBA *this)
{
    this->clock.ppu.y = 0;
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->ppu.display->last_written ^= 1;
    this->clock.master_frame++;
    vblank(this, 0);
}

void GBA_PPU_start_scanline(struct GBA*this)
{
    hblank(this, 0);
    this->ppu.cur_pixel = 0;
    this->clock.ppu.scanline_start = this->clock.master_cycle_count;
}

#define OUT_WIDTH 240

static void draw_line0(struct GBA *this, u16 *line_output)
{
    ///printf("\nno line0...")
}

static void draw_line3(struct GBA *this, u16 *line_output)
{
    u16 *line_input = ((u16 *)this->ppu.VRAM) + (this->clock.ppu.y * 240);
    memcpy(line_output, line_input, 480);
}

static void draw_line4(struct GBA *this, u16 *line_output)
{
    u32 base_addr = 0xA000 * this->ppu.io.frame;
    u8 *line_input = ((u8 *)this->ppu.VRAM) + base_addr + (this->clock.ppu.y * 240);
    for (u32 x = 0; x < 240; x++) {
        u8 palcol = line_input[x];
        line_output[x] = this->ppu.palette_RAM[palcol];
    }
}

void GBA_PPU_hblank(struct GBA*this)
{
    // It's cleared at cycle 0 and set at cycle 1007
    hblank(this, 1);

    // Now draw line!
    if (this->clock.ppu.y < 160) {
        u16 *line_output = this->ppu.cur_output + (this->clock.ppu.y * OUT_WIDTH);
        if (this->ppu.io.force_blank) {
            memset(line_output, 0xFF, 480);
            return;
        }
        switch (this->ppu.io.bg_mode) {
            case 0:
                draw_line0(this, line_output);
                break;
            case 3:
                draw_line3(this, line_output);
                break;
            case 4:
                draw_line4(this, line_output);
                break;
            default:
                assert(1 == 2);
        }
    }
}

void GBA_PPU_finish_scanline(struct GBA*this)
{
    // do stuff, then
    this->clock.ppu.hblank_active = 0;
    this->clock.ppu.y++;
    if (this->clock.ppu.y == 160) {
        vblank(this, 1);
    }
    if (this->clock.ppu.y == 227) {
        this->clock.ppu.vblank_active = 0;
    }
    if (this->clock.ppu.y == 228) {
        new_frame(this);
    }
}

static u32 GBA_PPU_read_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    printf("\nREAD UNKNOWN PPU ADDR:%08x sz:%d", addr, sz);
    return 0;
}

static void GBA_PPU_write_invalid(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    printf("\nWRITE UNKNOWN PPU ADDR:%08x sz:%d DATA:%08x", addr, sz, val);
}

u32 GBA_PPU_mainbus_read_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x05000400)
        return cR[sz](this->ppu.palette_RAM, addr & 0x3FF);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

u32 GBA_PPU_mainbus_read_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    if (addr < 0x06018000)
        return cR[sz](this->ppu.VRAM, addr - 0x06000000);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);

}

u32 GBA_PPU_mainbus_read_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect) {
    if (addr < 0x07000400)
        return cR[sz](this->ppu.OAM, addr & 0x3FF);

    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

void GBA_PPU_mainbus_write_palette(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x05000400) {
        return cW[sz](this->ppu.palette_RAM, addr & 0x3FF, val);
    }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

void GBA_PPU_mainbus_write_VRAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x06018000)
        return cW[sz](this->ppu.VRAM, addr - 0x06000000, val);

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

void GBA_PPU_mainbus_write_OAM(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val)
{
    if (addr < 0x07000400)
        return cW[sz](this->ppu.OAM, addr & 0x3FF, val);

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

static u32 vcount(struct GBA *this)
{
    return (this->clock.ppu.y == this->ppu.io.vcount_at);
}

u32 GBA_PPU_mainbus_read_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
    switch(addr) {
        case 0x04000004: {// DISPSTAT
            v = this->clock.ppu.vblank_active;
            v |= this->clock.ppu.hblank_active << 1;
            v |= vcount(this);
            v |= this->ppu.io.vblank_irq_enable << 3;
            v |= this->ppu.io.hblank_irq_enable << 4;
            v |= this->ppu.io.vcount_irq_enable << 5;
            v |= (this->clock.ppu.y << 8);
            return v; }
        case 0x04000006: { // VCNT
            return this->clock.ppu.y;
        }
    }
    return GBA_PPU_read_invalid(this, addr, sz, access, has_effect);
}

void GBA_PPU_mainbus_write_IO(struct GBA *this, u32 addr, u32 sz, u32 access, u32 val) {
    struct GBA_PPU *ppu = &this->ppu;
    printf("\nWRITE %08x", addr);
    switch(addr) {
        case 0x04000000: {// DISPCNT
            u32 new_mode = val & 7;
            if (new_mode >= 6) {
                printf("\nILLEGAL BG MODE:%d", new_mode);
                dbg_break("ILLEGAL BG MODE", this->clock.master_cycle_count);
            }
            else {
                if (new_mode != ppu->io.bg_mode) printf("\nBG MODE:%d", val & 7);
            }

            ppu->io.bg_mode = new_mode;

            ppu->io.frame = (val >> 4) & 1;
            ppu->io.force_blank = (val >> 7) & 1;
            ppu->bg[0].enable = (val >> 8) & 1;
            ppu->bg[1].enable = (val >> 9) & 1;
            ppu->bg[2].enable = (val >> 10) & 1;
            ppu->bg[3].enable = (val >> 11) & 1;
            ppu->obj.enable = (val >> 12) & 1;
            ppu->window[0].enable = (val >> 13) & 1;
            ppu->window[1].enable = (val >> 14) & 1;
            ppu->obj.window_enable = (val >> 15) & 1;
            printf("\nBGs 0:%d 1:%d 2:%d 3:%d obj:%d window0:%d window1:%d force_hblank:%d",
                   ppu->bg[0].enable, ppu->bg[1].enable, ppu->bg[2].enable, ppu->bg[3].enable,
                   ppu->obj.enable, ppu->window[0].enable, ppu->window[1].enable, ppu->io.force_blank
                   );
            return; }
        case 0x04000004: {// DISPSTAT
            this->ppu.io.vblank_irq_enable = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable = (val >> 5) & 1;
            return; }
    }

    GBA_PPU_write_invalid(this, addr, sz, access, val);
}

