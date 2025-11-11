
// Created by . on 4/21/25.
//

#include <string.h>
#include "fail"
#include "snes_ppu.h"
#include "snes_bus.h"
#include "snes_debugger.h"

#include "helpers/multisize_memaccess.c"

#define PPU_src_BG1 0
#define PPU_src_BG2 1
#define PPU_src_BG3 2
#define PPU_src_BG4 3
#define PPU_src_OBJ1 4
#define PPU_src_OBJ2 5
#define PPU_src_COL 6

void SNES_PPU_init(SNES *snes)
{
}

void SNES_PPU_delete(SNES *this)
{
    
}

void SNES_PPU_reset(SNES *snes)
{
    struct SNES_PPU *this = &snes->ppu;
    this->mode7.a = this->mode7.d = 0x100;
    this->mode7.b = this->mode7.c = 0;
}

static u32 read_oam(SNES_PPU *this, u32 addr)
{
    u32 n;
    if (!(addr & 0x200)) {
        n = addr >> 2;
        addr &= 3;
        switch(addr) {
            case 0:
                return this->obj.items[n].x & 0xFF;
            case 1:
                return this->obj.items[n].y & 0xFF;
            case 2:
                return this->obj.items[n].tile_num;
        }
        return (this->obj.items[n].name_select) | (this->obj.items[n].palette << 1) | (this->obj.items[n].priority << 4) | (this->obj.items[n].hflip << 6) | (this->obj.items[n].vflip << 7);
    } else {
        n = (addr & 0x1F) << 2;
        return (this->obj.items[n].x >> 8) |
        ((this->obj.items[n+1].x >> 8) << 2) |
        ((this->obj.items[n+2].x >> 8) << 4) |
        ((this->obj.items[n+3].x >> 8) << 6) |
        (this->obj.items[n].size << 1) |
        (this->obj.items[n+1].size << 3) |
        (this->obj.items[n+2].size << 5) |
        (this->obj.items[n+3].size << 7);
    }
}

static void write_oam(SNES *snes, SNES_PPU *this, u32 addr, u32 val) {
    if (!snes->clock.ppu.vblank_active && !this->io.force_blank) {
        printf("\nWARN OAM BAD WRITE TIME");
        return;
    }
    if (addr < 0x200) { // 0-0x1FF
        u32 n = addr >> 2;
        if (n < 128) {
            struct SNES_PPU_sprite *sp = &snes->ppu.obj.items[n];
            switch (addr & 3) {
                case 0:
                    sp->x = (sp->x & 0x100) | val;
                    return;
                case 1:
                    sp->y = val + 1;
                    return;
                case 2:
                    sp->tile_num = val;
                    return;
                default:
                    sp->name_select = val & 1;
                    sp->name_select_add = 1 + ((val & 1) << 12);
                    sp->palette = (val >> 1) & 7;
                    sp->pal_offset = 128 + (sp->palette << 4);
                    sp->priority = (val >> 4) & 3;
                    sp->hflip = (val >> 6) & 1;
                    sp->vflip = (val >> 7) & 1;
                    return;
            }
        }
    }
    else {
        if (addr >= 544) {
            printf("\nOVER 544 OAM!? %d", addr);
            return;
        }
        u32 n = (addr & 0x1F) << 2;
        snes->ppu.obj.items[n].x = (snes->ppu.obj.items[n].x & 0xFF) | ((val << 8) & 0x100);
        snes->ppu.obj.items[n+1].x = (snes->ppu.obj.items[n+1].x & 0xFF) | ((val << 6) & 0x100);
        snes->ppu.obj.items[n+2].x = (snes->ppu.obj.items[n+2].x & 0xFF) | ((val << 4) & 0x100);
        snes->ppu.obj.items[n+3].x = (snes->ppu.obj.items[n+3].x & 0xFF) | ((val << 2) & 0x100);
        snes->ppu.obj.items[n].size = (val >> 1) & 1;
        snes->ppu.obj.items[n+1].size = (val >> 3) & 1;
        snes->ppu.obj.items[n+2].size = (val >> 5) & 1;
        snes->ppu.obj.items[n+3].size = (val >> 7) & 1;
    }
}

static u32 get_addr_by_map(SNES_PPU *this)
{
    u32 addr = this->io.vram.addr;
    switch(this->io.vram.mapping) {
        case 0: return addr;
        case 1:
            return (addr & 0x7F00) | ((addr << 3) & 0x00F8) | ((addr >> 5) & 7);
        case 2:
            return (addr & 0x7E00) | ((addr << 3) & 0x01F8) | ((addr >> 6) & 7);
        case 3:
            return (addr & 0x7C00) | ((addr << 3) & 0x03F8) | ((addr >> 7) & 7);
    }
    return 0x8000;
}

static void update_video_mode(SNES_PPU *this)
{
		//snes->clock.scanline.bottom_scanline = this->overscan ? 240 : 225;
#define BGP(num, lo, hi) this->bg[num-1].priority[0] = lo; this->bg[num-1].priority[1] = hi
#define OBP(n0, n1, n2, n3) this->obj.priority[0] = n0; this->obj.priority[1] = n1; this->obj.priority[2] = n2; this->obj.priority[3] = n3
		switch(this->io.bg_mode) {
			case 0:
				this->bg[0].tile_mode = SPTM_BPP2;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_BPP2;
				this->bg[3].tile_mode = SPTM_BPP2;
				BGP(1, 5, 2);
				BGP(2, 6, 3);
				BGP(3, 11, 8);
				BGP(4, 12, 9);
				OBP(10, 7, 4, 1);
				break;
			case 1:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_BPP2;
				this->bg[3].tile_mode = SPTM_inactive;
                BGP(1, 5, 2);
                BGP(2, 6, 3);
                OBP(10, 7, 4, 1);
				if (this->io.bg_priority) {
					BGP(3, 12, 8);
				} else {
					BGP(3, 11, 0);
				}
				break;
			case 2:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(1, 8, 2);
				BGP(2, 11, 5);
				OBP(10, 7, 4, 1);
				break;
			case 3:
				this->bg[0].tile_mode = SPTM_BPP8;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(1, 8, 2);
				BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 4:
				this->bg[0].tile_mode = SPTM_BPP8;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
                BGP(1, 8, 2);
                BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 5:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
                BGP(1, 8, 2);
                BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 6:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_inactive;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
                BGP(1, 8, 2);
                OBP(10, 7, 4, 1);
				break;
			case 7:
                this->bg[0].tile_mode = SPTM_mode7;
                this->bg[1].tile_mode = this->io.extbg ? SPTM_mode7 : SPTM_inactive;
                this->bg[2].tile_mode = SPTM_inactive;
                this->bg[3].tile_mode = SPTM_inactive;
                BGP(1, 8, 8);
                BGP(2, 5, 11);
                OBP(10, 7, 4, 1);
				break;
		}
        static const u32 twidth[4] = { 32, 64, 32, 64};
        static const u32 theight[4] = { 32, 32, 64, 64 };
        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            struct SNES_PPU_BG *bg = &this->bg[bgnum];
            bg->cols = twidth[bg->io.screen_size];
            bg->rows = theight[bg->io.screen_size];
            bg->col_mask = bg->cols - 1;
            bg->row_mask = bg->rows - 1;
            bg->tile_px = bg->io.tile_size ? 16 : 8;
            bg->scroll_shift = bg->io.tile_size ? 4 : 3;
            bg->tile_px_mask = bg->tile_px - 1;

            bg->pixels_h = bg->cols * bg->tile_px;
            bg->pixels_v = bg->rows * bg->tile_px;
            bg->pixels_h_mask = bg->pixels_h - 1;
            bg->pixels_v_mask = bg->pixels_v - 1;

            bg->palette_offset = 0;
            switch(bg->tile_mode) {
                case SPTM_BPP2:
                    bg->palette_offset = 0;
                    if (this->io.bg_mode == 0) {
                        bg->palette_offset = 0x20 * bgnum;
                    }
                    bg->palette_shift = 2; // * 4
                    bg->palette_mask = 7;
                    bg->num_planes = 1;
                    bg->tile_bytes = 0x10;
                    bg->tile_row_bytes = 2;
                    break;
                case SPTM_BPP4:
                    bg->palette_shift = 4; // * 16
                    bg->palette_mask = 7;
                    bg->num_planes = 2;
                    bg->tile_bytes = 0x20;
                    bg->tile_row_bytes = 4;
                    break;
                case SPTM_BPP8:
                    bg->palette_shift = 0; //
                    bg->palette_mask = 0xFF;
                    bg->num_planes = 4;
                    bg->tile_bytes = 0x40;
                    bg->tile_row_bytes = 8;
                    break;
                case SPTM_inactive:
                    break;
                case SPTM_mode7:
                    bg->palette_shift = 0;
                    bg->palette_mask = 0xFF;
                    bg->num_planes = 1;
                    break;
            }
        }
}

static void write_VRAM(SNES *snes, u32 addr, u32 val)
{
    addr &= 0x7FFF;
    DBG_EVENT(DBG_SNES_EVENT_WRITE_VRAM);
    dbgloglog(snes, SNES_CAT_PPU_VRAM_WRITE, DBGLS_INFO, "VRAM write %04x: %04x", addr, val);
    snes->ppu.VRAM[addr] = val;
}

void SNES_PPU_write(SNES *snes, u32 addr, u32 val, SNES_memmap_block *bl) {
    addr &= 0xFFFF;
    u32 addre;
    struct SNES_PPU *this = &snes->ppu;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_write(snes, addr, val); }
    switch (addr) {
        case 0x2100: // INIDISP
            this->io.force_blank = (val >> 7) & 1;
            this->io.screen_brightness = val & 15;
            return;
        case 0x2101: // OBSEL
            this->io.obj.sz = (val >> 5) & 7;
            this->io.obj.name_select = (val >> 3) & 3;
            this->io.obj.tile_addr = (val << 13) & 0x6000;
            return;
        case 0x2102: // OAMADDL
            this->io.oam.addr = (this->io.oam.addr & 0xFE00) | (val << 1);
            this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            return;
        case 0x2103: // OAMADDH
            this->io.oam.addr = (this->io.oam.addr & 0x1FE) | ((val & 1) << 9);
            this->io.oam.priority = (val >> 7) & 1;
            this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            return;
        case 0x2104: {// OAMDATA
            u32 latchbit = this->io.oam.addr & 1;
            addre = this->io.oam.addr;
            this->io.oam.addr = (this->io.oam.addr + 1) & 0x3FF;
            if (!latchbit) this->latch.oam = val;
            if (addre & 0x200) {
                write_oam(snes, this, addre, val);
            } else if (latchbit) {
                write_oam(snes, this, addre & 0xFFFE, this->latch.oam);
                write_oam(snes, this, (addre & 0xFFFE) + 1, val);
            }
            this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            return;
        }
        case 0x2105: { // BGMODE
            u32 old_bg_mode = this->io.bg_mode;
            this->io.bg_mode = val & 7;
            if (old_bg_mode != this->io.bg_mode) {
                printf("\nNEW BG MODE %d", this->io.bg_mode);
            }
            this->io.bg_priority = (val >> 4) & 1;
            this->bg[0].io.tile_size = (val >> 4) & 1;
            this->bg[1].io.tile_size = (val >> 5) & 1;
            this->bg[2].io.tile_size = (val >> 6) & 1;
            this->bg[3].io.tile_size = (val >> 7) & 1;
            update_video_mode(this);
            return;
        }
        case 0x2106: {// MOSAIC
            u32 latchbit = this->bg[0].mosaic.enable | this->bg[1].mosaic.enable | this->bg[2].mosaic.enable |
                           this->bg[3].mosaic.enable;
            /*this->bg[0].mosaic.enable = (val >> 0) & 1;
            this->bg[1].mosaic.enable = (val >> 1) & 1;
            this->bg[2].mosaic.enable = (val >> 2) & 1;
            this->bg[3].mosaic.enable = (val >> 3) & 1;*/
            this->io.mosaic.size = ((val >> 4) & 15) + 1;
            if (!latchbit && (val & 15)) {
                this->mosaic.counter = this->io.mosaic.size + 1;
            }
            return;
        }
        case 0x2107: // BG1SC
        case 0x2108: // BG2SC
        case 0x2109: // BG3SC
        case 0x210A: { // BG4SC
            u32 bgnum = addr - 0x2107;
            struct SNES_PPU_BG *bg = &this->bg[bgnum];
            bg->io.screen_size = val & 3;
            bg->io.screen_addr = (val << 8) & 0x7C00;
            update_video_mode(this);
            return;
        }
        case 0x210B: // BG12NBA
            this->bg[0].io.tiledata_addr = (val << 12) & 0x7000;
            this->bg[1].io.tiledata_addr = (val << 8) & 0x7000;
            return;
        case 0x210C: // BG34NBA
            this->bg[2].io.tiledata_addr = (val << 12) & 0x7000;
            this->bg[3].io.tiledata_addr = (val << 8) & 0x7000;
            return;
        case 0x210D: // BG1HOFS
            this->bg[0].io.hoffset = (val << 8) | (this->latch.ppu1.bgofs & 0xF8) | (this->latch.ppu2.bgofs & 7);
            this->latch.ppu1.bgofs = this->latch.ppu2.bgofs = val;

            this->mode7.hoffset = (val << 8) | (this->latch.mode7);
            this->latch.mode7 = val;
            this->mode7.rhoffset = SIGNe13to32(this->mode7.hoffset);
            return;
        case 0x210E: // BG1VOFS
            this->bg[0].io.voffset = (val << 8) | (this->latch.ppu1.bgofs);
            this->latch.ppu1.bgofs = val;

            this->mode7.voffset = (val << 8) | (this->latch.mode7);
            this->latch.mode7 = val;
            this->mode7.rvoffset = SIGNe13to32(this->mode7.voffset);
            return;
        case 0x210F: // BG2HOFS
            this->bg[1].io.hoffset = (val << 8) | (this->latch.ppu1.bgofs & 0xF8) | (this->latch.ppu2.bgofs & 7);
            this->latch.ppu1.bgofs = this->latch.ppu2.bgofs = val;
            return;
        case 0x2110: // BG2VOFS
            this->bg[1].io.voffset = (val << 8) | (this->latch.ppu1.bgofs);
            this->latch.ppu1.bgofs = val;
            return;
        case 0x2111: // BG3HOFS
            this->bg[2].io.hoffset = (val << 8) | (this->latch.ppu1.bgofs & 0xF8) | (this->latch.ppu2.bgofs & 7);
            this->latch.ppu1.bgofs = this->latch.ppu2.bgofs = val;
            return;
        case 0x2112: // BG3VOFS
            this->bg[2].io.voffset = (val << 8) | (this->latch.ppu1.bgofs);
            this->latch.ppu1.bgofs = val;
            return;
        case 0x2113: // BG4HOFS
            this->bg[3].io.hoffset = (val << 8) | (this->latch.ppu1.bgofs & 0xF8) | (this->latch.ppu2.bgofs & 7);
            this->latch.ppu1.bgofs = this->latch.ppu2.bgofs = val;
            return;
        case 0x2114: // BG4VOFS
            this->bg[3].io.voffset = (val << 8) | (this->latch.ppu1.bgofs);
            this->latch.ppu1.bgofs = val;
            return;
        case 0x2115: {// VRAM increment
            static const int steps[4] = {1, 32, 128, 128};
            this->io.vram.increment_step = steps[val & 3];
            this->io.vram.mapping = (val >> 2) & 3;
            this->io.vram.increment_mode = (val >> 7) & 1;
            return; }
        case 0x2116: // VRAM address lo
            this->io.vram.addr = (this->io.vram.addr & 0xFF00) | val;
            this->latch.vram = this->VRAM[get_addr_by_map(this)];
            return;
        case 0x2117: // VRAM address hi
            this->io.vram.addr = (val << 8) | (this->io.vram.addr & 0xFF);
            this->latch.vram = this->VRAM[get_addr_by_map(this)];
            return;
        case 0x2118: // VRAM data lo
            /*if (!snes->clock.ppu.vblank_active && !this->io.force_blank) {
                printf("\nDISCARD WRITE ON VBLANK OR FORCE BLANK");
                return;
            }*/
            addre = get_addr_by_map(this);
            write_VRAM(snes, addre, (this->VRAM[addre] & 0xFF00) | (val & 0xFF));
            if (this->io.vram.increment_mode == 0) this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            return;
        case 0x2119: // VRAM data hi
            //if (!snes->clock.ppu.vblank_active && !this->io.force_blank) return;
            addre = get_addr_by_map(this);
            write_VRAM(snes, addre, (val << 8) | (this->VRAM[addre] & 0xFF));
            if (this->io.vram.increment_mode == 1) this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            return;
        case 0x211A: // M7SEL
            this->mode7.hflip = val & 1;
            this->mode7.vflip = (val >> 1) & 1;
            this->mode7.repeat = (val >> 6) & 3;
            return;
        case 0x211B: // M7A
            this->mode7.a = (i16)((val << 8) | this->latch.mode7);
            this->latch.mode7 = val;
            return;
        case 0x211C: // M7B
            this->mode7.b = (i16)((val << 8) | this->latch.mode7);
            this->latch.mode7 = val;
            return;
        case 0x211D: // M7C
            this->mode7.c = (i16)((val << 8) | this->latch.mode7);
            this->latch.mode7 = val;
            return;
        case 0x211E: // M7D
            this->mode7.d = (i16)((val << 8) | this->latch.mode7);
            this->latch.mode7 = val;
            return;
        case 0x211F: // M7E
            this->mode7.x = (val << 8) | this->latch.mode7;
            this->mode7.rx = SIGNe13to32(this->mode7.x);
            this->latch.mode7 = val;
            return;
        case 0x2120: // M7F
            this->mode7.y = (val << 8) | this->latch.mode7;
            this->mode7.ry = SIGNe13to32(this->mode7.y);
            this->latch.mode7 = val;
            return;
        case 0x2121: // Color RAM address
            this->io.cgram_addr = val;
            this->latch.cgram_addr = 0;
            return;
        case 0x2122: // Color RAM data
            if (this->latch.cgram_addr == 0) {
                this->latch.cgram_addr = 1;
                this->latch.cgram = val;
            }
            else {
                this->latch.cgram_addr = 0;
                this->CGRAM[this->io.cgram_addr] = ((val & 0x7F) << 8) | this->latch.cgram;
                this->io.cgram_addr = (this->io.cgram_addr + 1) & 0xFF;
            }
            return;
        case 0x2123: // W12SEL
            this->bg[0].window.invert[0] = val  & 1;
            this->bg[0].window.enable[0] = (val >> 1) & 1;
            this->bg[0].window.invert[1] = (val >> 2) & 1;
            this->bg[0].window.enable[1] = (val >> 3) & 1;
            this->bg[1].window.invert[0] = (val >> 4)  & 1;
            this->bg[1].window.enable[0] = (val >> 5) & 1;
            this->bg[1].window.invert[1] = (val >> 6) & 1;
            this->bg[1].window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2124: // W34SEL
            this->bg[2].window.invert[0] = val  & 1;
            this->bg[2].window.enable[0] = (val >> 1) & 1;
            this->bg[2].window.invert[1] = (val >> 2) & 1;
            this->bg[2].window.enable[1] = (val >> 3) & 1;
            this->bg[3].window.invert[0] = (val >> 4)  & 1;
            this->bg[3].window.enable[0] = (val >> 5) & 1;
            this->bg[3].window.invert[1] = (val >> 6) & 1;
            this->bg[3].window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2125: // WOBJSEL
            this->obj.window.invert[0] = val & 1;
            this->obj.window.enable[0] = (val >> 1) & 1;
            this->obj.window.invert[1] = (val >> 2) & 1;
            this->obj.window.enable[1] = (val >> 3) & 1;
            this->color_math.window.invert[0] = (val >> 4) & 1;
            this->color_math.window.enable[0] = (val >> 5) & 1;
            this->color_math.window.invert[1] = (val >> 6) & 1;
            this->color_math.window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2126: // WH0
            //if (dbg.log_windows) console.log('WINDOW ONE LEFT WRITE', val);
            //this->window.one_left = 0;
            this->window[0].left = val;
            return;
        case 0x2127: // WH1...
            //if (val == 128) debugger;
            //if (dbg.log_windows) console.log('WINDOW ONE RIGHT WRITE', val);
            this->window[0].right = val;
            //this->window.one_right = 256;
            return;
        case 0x2128: // WH2
            this->window[1].left = val;
            return;
        case 0x2129: // WH3
            this->window[1].right = val;
            return;
        case 0x212A: // WBGLOG
            this->bg[0].window.mask = val & 3;
            this->bg[1].window.mask = (val >> 2) & 3;
            this->bg[2].window.mask = (val >> 4) & 3;
            this->bg[3].window.mask = (val >> 6) & 3;
            return;
        case 0x212B: // WOBJLOG
            this->obj.window.mask = val & 3;
            this->color_math.window.mask = (val >> 2) & 3;
            return;
        case 0x212C: // TM
            this->bg[0].main_enable = val & 1;
            this->bg[1].main_enable = (val >> 1) & 1;
            this->bg[2].main_enable = (val >> 2) & 1;
            this->bg[3].main_enable = (val >> 3) & 1;
            this->obj.main_enable = (val >> 4) & 1;
            return;
        case 0x212D: // TS
            this->bg[0].sub_enable = val & 1;
            this->bg[1].sub_enable = (val >> 1) & 1;
            this->bg[2].sub_enable = (val >> 2) & 1;
            this->bg[3].sub_enable = (val >> 3) & 1;
            this->obj.sub_enable = (val >> 4) & 1;
            return;
        case 0x212E: // TMW
            this->bg[0].window.main_enable = val & 1;
            this->bg[1].window.main_enable = (val >> 1) & 1;
            this->bg[2].window.main_enable = (val >> 2) & 1;
            this->bg[3].window.main_enable = (val >> 3) & 1;
            this->obj.window.main_enable = (val >> 4) & 1;
            return;
        case 0x212F: // TSW
            this->bg[0].window.sub_enable = val & 1;
            this->bg[1].window.sub_enable = (val >> 1) & 1;
            this->bg[2].window.sub_enable = (val >> 2) & 1;
            this->bg[3].window.sub_enable = (val >> 3) & 1;
            this->obj.window.sub_enable = (val >> 4) & 1;
            return;
        case 0x2130: // CGWSEL
            this->color_math.direct_color = val & 1;
            this->color_math.blend_mode = (val >> 1) & 1;
            this->color_math.window.sub_mask = (val >> 4) & 3;
            this->color_math.window.main_mask = (val >> 6) & 3;
            return;
        case 0x2131: // CGADDSUB
            this->color_math.enable[PPU_src_BG1] = val & 1;
            this->color_math.enable[PPU_src_BG2] = (val >> 1) & 1;
            this->color_math.enable[PPU_src_BG3] = (val >> 2) & 1;
            this->color_math.enable[PPU_src_BG4] = (val >> 3) & 1;
            this->color_math.enable[PPU_src_OBJ1] = 0;
            this->color_math.enable[PPU_src_OBJ2] = (val >> 4) & 1;
            this->color_math.enable[PPU_src_COL] = (val >> 5) & 1;
            this->color_math.halve = (val >> 6) & 1;
            this->color_math.math_mode = (val >> 7) & 1;
            return;
        case 0x2132: // COLDATA weirdness
            if (val & 0x20) this->color_math.fixed_color = (this->color_math.fixed_color & 0x7FE0) | (val & 31);
            if (val & 0x40) this->color_math.fixed_color = (this->color_math.fixed_color & 0x7C1F) | ((val & 31) << 5);
            if (val & 0x80) this->color_math.fixed_color = (this->color_math.fixed_color & 0x3FF) | ((val & 31) << 10);
            DBG_EVENT(DBG_SNES_EVENT_WRITE_COLDATA);
            return;
        case 0x2133: // SETINI
            this->io.interlace = val & 1;
            this->io.obj.interlace = (val >> 1) & 1;
            this->io.overscan = (val >> 2) & 1;
            this->io.pseudo_hires = (val >> 3) & 1;
            this->io.extbg = (val >> 6) & 1;
            update_video_mode(this);
            return;

        case 0x2180: // WRAM access port
            SNES_wdc65816_write(snes, 0x7E0000 | this->io.wram_addr, val);
            this->io.wram_addr = (this->io.wram_addr + 1) & 0x1FFFF;
            return;
        case 0x2181: // WRAM addr low
            this->io.wram_addr = (this->io.wram_addr & 0x1FF00) + val;
            return;
        case 0x2182: // WRAM addr med
            this->io.wram_addr = (val << 8) + (this->io.wram_addr & 0x100FF);
            return;
        case 0x2183: // WRAM bank
            this->io.wram_addr = ((val & 1) << 16) | (this->io.wram_addr & 0xFFFF);
            return;
        case 0x2134:
            return;
    }
    printf("\nWARN SNES PPU WRITE NOT IN %04x %02x", addr, val);
    static int num = 0;
    num++;
    if (num == 10) {
        //dbg_break("PPU WRITE TOOMANYBAD", snes->clock.master_cycle_count);
    }
}

static u32 mode7_mul(SNES_PPU *this)
{
    return (u32)((i32)(this->mode7.a) * (i32)(i8)(this->mode7.b >> 8));
}

u32 SNES_PPU_read(SNES *snes, u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl)
{
    addr &= 0xFFFF;
    struct SNES_PPU *this = &snes->ppu;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_read(snes, addr, old, has_effect); }
    u32 result;
    switch(addr) {
        case 0x2134: // MPYL
            result = mode7_mul(this);
            return result & 0xFF;
        case 0x2135: // MPYM
            result = mode7_mul(this);
            return (result >> 8) & 0xFF;
        case 0x2136: // MPYH
            result = mode7_mul(this);
            return (result >> 16) & 0xFF;
        case 0x2137: // SLHV?
            if (has_effect && (snes->r5a22.io.pio & 0x80)) SNES_latch_ppu_counters(snes);
            return old;
        case 0x2138: {// OAMDATAREAD
            u32 data = read_oam(this, this->io.oam.addr);
            if (has_effect) {
                this->io.oam.addr = (this->io.oam.addr + 1) & 0x3FF;
                this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            }
            return data; }
        case 0x2139: // VMDATAREADL
            result = this->latch.vram & 0xFF;
            if (has_effect && this->io.vram.increment_mode == 0) {
                this->latch.vram = this->VRAM[get_addr_by_map(this)];
                this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213A: // VMDATAREADH
            result = (this->latch.vram >> 8) & 0xFF;
            if (has_effect && (this->io.vram.increment_mode == 1)) {
                this->latch.vram = this->VRAM[get_addr_by_map(this)];
                this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213D: // OPVCT
            if (has_effect) {
                if (this->latch.vcounter == 0) {
                    this->latch.vcounter = 1;
                    this->latch.ppu2.mdr = snes->clock.ppu.y;
                } else {
                    this->latch.vcounter = 0;
                    this->latch.ppu2.mdr = (snes->clock.ppu.y >> 8) | (this->latch.ppu2.mdr & 0xFE);
                }
            }
            return this->latch.ppu2.mdr;
        case 0x213E: // STAT77
            if (has_effect)
                this->latch.ppu1.mdr = 1 | (this->obj.range_overflow << 6) | (this->obj.time_overflow << 7);
            return this->latch.ppu1.mdr;
        case 0x213F:
            if (has_effect) {
                this->latch.hcounter = 0;
                this->latch.vcounter = 0;
                this->latch.ppu2.mdr &= 32;
                this->latch.ppu2.mdr |= 0x03 | (snes->clock.ppu.field << 7);
                if (!(snes->r5a22.io.pio & 0x80)) {
                    this->latch.ppu2.mdr |= 1 << 6;
                } else {
                    this->latch.ppu2.mdr |= this->latch.counters << 6;
                    this->latch.counters = 0;
                }
            }
            return this->latch.ppu2.mdr;
        case 0x2180: {// WRAM access port
            u32 r = SNES_wdc65816_read(snes, 0x7E0000 | this->io.wram_addr, 0, has_effect);
            if (has_effect) {
                this->io.wram_addr++;
                if (this->io.wram_addr > 0x1FFFF) this->io.wram_addr = 0;
            }
            return r; }
    }
    if (has_effect) {
        printf("\nUNIMPLEMENTED PPU READ FROM %04x", addr);
        if (addr == 0x2000) {
            dbg_break("PPU2k!", snes->clock.master_cycle_count);
        }
        //dbg_break("PPU", snes->clock.master_cycle_count);
    }
    return 0;
}

// Two states: regular, or paused for RAM refresh.
// I think I'll just use a "ram_refresh" kinda function that just advances the clock!? Maybe?

static void new_scanline(SNES* this, u64 cur_clock)
{
    // TODO: this
    // TODO: hblank exit here too
    this->clock.ppu.scanline_start = cur_clock;
    this->clock.ppu.y++;
    if (this->dbg.events.view.vec) {
        debugger_report_line(this->dbg.interface, this->clock.ppu.y);
    }
    this->r5a22.status.hirq_line = 0;
}

static void dram_refresh(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *this = (SNES *)ptr;
    scheduler_from_event_adjust_master_clock(&this->scheduler, 40);
}

struct slice {
    union SNES_PPU_px px[16];
    u32 priority;
};

static inline u32 direct_color(u32 palette_index, u32 palette_color)
{
    return ((palette_color << 2) & 0x001C) + ((palette_index << 1) & 2) +
           ((palette_color << 4) & 0x380) + ((palette_index << 5) & 0x40) +
           ((palette_color << 7) & 0x6000) + ((palette_index << 10) & 0x1000);
}

static inline void setpx(union SNES_PPU_px *px, u32 source, u32 priority, u32 color, u32 bpp, u32 dbg_priority)
{
    px->has = 1;
    px->source = source;
    px->priority = priority;
    px->color = color;
    px->bpp = bpp;
    px->dbg_priority = dbg_priority;
}

static void draw_bg_line_mode7(SNES *snes, u32 source, i32 y)
{
    struct SNES_PPU *this = &snes->ppu;
    struct SNES_PPU_BG *bg = &this->bg[source];

    y = this->mode7.vflip ? 255 - y : y;
    u32 dbp;

    u16 m_color = 0;
    u32 m_counter = 1;
    u32 m_palette = 0;
    u8  m_priority = 0;

    i32 hohc = (this->mode7.rhoffset - this->mode7.rx);
    i32 vovc = (this->mode7.rvoffset - this->mode7.ry);

    // sign extend hohc and vovc
    hohc = (hohc & 0x2000) ? (hohc | ~1023) : (hohc & 1023);
    vovc = (vovc & 0x2000) ? (vovc | ~1023) : (vovc & 1023);

    i32 origin_x = (this->mode7.a * hohc & ~63) + (this->mode7.b * vovc & ~63) + (this->mode7.b * y & ~63) + (this->mode7.rx << 8);
    i32 origin_y = (this->mode7.c * hohc & ~63) + (this->mode7.d * vovc & ~63) + (this->mode7.d * y & ~63) + (this->mode7.ry << 8);

    for (i32 sx = 0; sx < 256; sx++) {
        i32 x = this->mode7.hflip ? 255 - sx : sx;
        i32 pixel_x = (origin_x + this->mode7.a * x) >> 8;
        i32 pixel_y = (origin_y + this->mode7.c * x) >> 8;
        i32 tile_x = (pixel_x >> 3) & 127;
        i32 tile_y = (pixel_y >> 3) & 127;
        i32 out_of_bounds = (pixel_x | pixel_y) & ~1023;
        i32 tile_addr = (tile_y << 7) | tile_x;
        i32 pal_addr = ((pixel_y & 7) << 3) + (pixel_x & 7);
        u32 tile = (((this->mode7.repeat == 3) && out_of_bounds) ? 0 : this->VRAM[tile_addr & 0x7FFF]) & 0xFF;
        if ((sx == 12) && (y == 96)) {
            printf("\npx:%d  py:%d  tile_addr:%04x  tile_val:%02x", pixel_x, pixel_y, tile_addr, tile);
        }
        u32 palette = (((this->mode7.repeat == 2) && out_of_bounds) ? 0 : this->VRAM[(tile << 6 | pal_addr) & 0x7FFF] >> 8) & 0xFF;

        u32 priority;
        if (source == 0) {
            priority = bg->priority[0];
            dbp = 0;
        }
        else {
            priority = bg->priority[palette >> 7];
            dbp = palette >> 7;
            palette &= 0x7F;
        }

        if (--m_counter == 0) {
            m_counter = bg->mosaic.enable ? bg->mosaic.size : 1;
            m_palette = palette;
            m_priority = priority;
            if (this->color_math.direct_color && source == 0) {
                m_color = direct_color(0, palette);
            }
            else {
                m_color = this->CGRAM[palette];
            }
        }

        if (!m_palette) continue;
        setpx(&bg->line[sx], source, m_priority, m_color, BPP_8, dbp);
    }
}

static u32 get_tile(SNES *snes, SNES_PPU_BG *bg, i32 hoffset, i32 voffset)
{
    u32 hires = snes->ppu.io.bg_mode == 5 || snes->ppu.io.bg_mode == 6;
    u32 tile_height = 3 + bg->io.tile_size;
    u32 tile_width = !hires ? tile_height : 4;
    u32 screen_x = bg->io.screen_size & 1 ? 0x400 : 0;
    u32 screen_y = bg->io.screen_size & 2 ? 32 << (5 + (bg->io.screen_size & 1)) : 0;
    u32 tile_x = hoffset >> tile_width;
    u32 tile_y = voffset >> tile_height;
    u32 offset = ((tile_y & 0x1F) << 5) | (tile_x & 0x1F);
    if (tile_x & 0x20) offset += screen_x;
    if (tile_y & 0x20) offset += screen_y;
    u32 addr = (bg->io.screen_addr + offset) & 0x7FFF;
    return snes->ppu.VRAM[addr];
}

static void draw_bg_line(SNES *snes, u32 source, u32 y)
{
    struct SNES_PPU *this = &snes->ppu;
    struct SNES_PPU_BG *bg = &this->bg[source];
    memset(bg->line, 0, sizeof(bg->line));
    if ((bg->tile_mode == SPTM_inactive) || (!bg->main_enable && !bg->sub_enable)) {
        return;
    }
    if (bg->tile_mode == SPTM_mode7) {
        return draw_bg_line_mode7(snes, source, y);
    }
    u32 bpp = BPP_4;
    if (bg->tile_mode == SPTM_BPP2) bpp = BPP_2;
    if (bg->tile_mode == SPTM_BPP8) bpp = BPP_8;

    u32 hires = this->io.bg_mode == 5 || this->io.bg_mode == 6;
    u32 offset_per_tile_mode = this->io.bg_mode == 2 || this->io.bg_mode == 4 || this->io.bg_mode == 6;

    u32 direct_color_mode = this->color_math.direct_color && source == 0 && (this->io.bg_mode == 3 || this->io.bg_mode == 4);
    u32 color_shift = 3 + bg->tile_mode;
    i32 width = 256 << hires;
    //console.log('RENDERING PPU y:', y, 'BG MODE', this->io.bg_mode);

    u32 tile_height = 3 + bg->io.tile_size;
    u32 tile_width = hires ? 4 : tile_height;
    u32 tile_mask = 0xFFF >> bg->tile_mode;
    bg->tiledata_index = bg->io.tiledata_addr >> (3 + bg->tile_mode);

    bg->palette_base = this->io.bg_mode == 0 ? source << 5 : 0;
    bg->palette_shift = 2 << bg->tile_mode;

    u32 hscroll = bg->io.hoffset;
    u32 vscroll = bg->io.voffset;
    u32 hmask = (width << bg->io.tile_size << (bg->io.screen_size & 1)) - 1;
    u32 vmask = (width << bg->io.tile_size << ((bg->io.screen_size & 2) >> 1)) - 1;
    //if (snes->clock.ppu.y == 40 && source == 0) printf("\nHSCROLL:%d VSCROLL:%d", hscroll, vscroll);

    if (hires) {
        hscroll <<= 1;
        // TODO: this?
        //if (this->io.interlace) y = y << 1 | +(cache.control.field && !bg.mosaic_enable);
    }
    if (bg->mosaic.enable) {
        //printf("\nWHAT ENABLED?");
        y -= (this->io.mosaic.size - bg->mosaic.counter) << (hires && this->io.interlace);
    }

    u32 mosaic_counter = 1;
    u32 mosaic_palette = 0;
    u32 mosaic_priority = 0;
    u32 mosaic_color = 0;

    i32 x = 0 - (hscroll & 7);
    while(x < width) {
        i32 hoffset = x + hscroll;
        i32 voffset = y + vscroll;
        if (offset_per_tile_mode) {
            i32 valid_bit = 0x2000 << source;
            i32 offset_x = x + (hscroll & 7);
            if (offset_x >= 8) {
                i32 hlookup = get_tile(snes, &snes->ppu.bg[2], (offset_x - 8) + (this->bg[2].io.hoffset & 0xFFF8), this->bg[2].io.voffset);
                if (this->io.bg_mode == 4) {
                    if (hlookup & valid_bit) {
                        if (!(hlookup & 0x8000)) {
                            hoffset = offset_x + (hlookup & 0xFFF8);
                        } else {
                            voffset = y + hlookup;
                        }
                    }
                } else {
                    i32 vlookup = get_tile(snes, &this->bg[2], (offset_x - 8) + (this->bg[2].io.hoffset & 0xFFF8), this->bg[2].io.voffset + 8);
                    if (hlookup & valid_bit) {
                        hoffset = offset_x + (hlookup & 0xFFF8);
                    }
                    if (vlookup & valid_bit) {
                        voffset = y + vlookup;
                    }
                }
            }
        }
        hoffset &= hmask;
        voffset &= vmask;

        u32 tile_number = get_tile(snes, bg, hoffset, voffset);

        u32 mirror_x = tile_number & 0x4000 ? 7 : 0;
        u32 mirror_y = tile_number & 0x8000 ? 7 : 0;
        u32 dbg_prio = ((tile_number & 0x2000) >> 13) & 1;
        u32 tile_priority = bg->priority[dbg_prio];
        u32 palette_number = (tile_number >> 10) & 7;
        u32 palette_index = (bg->palette_base + (palette_number << bg->palette_shift)) & 0xFF;

        if (tile_width == 4 && (((hoffset & 8) >> 3) ^ (mirror_x != 0))) tile_number += 1;
        if (tile_height == 4 && (((voffset & 8) >> 3) ^ (mirror_y != 0))) tile_number += 16;
        tile_number = ((tile_number & 0x3FF) + bg->tiledata_index) & tile_mask;

        u32 address = (tile_number << color_shift) + ((voffset & 7) ^ mirror_y) & 0x7FFF;

        u32 datalo = (this->VRAM[(address + 8) & 0x7FFF] << 16) | (this->VRAM[address]);
        u32 datahi = (this->VRAM[(address + 24) & 0x7FFF] << 16) | (this->VRAM[(address + 16) & 0x7FFF]);
        u32 datamid = ((datalo >> 16) & 0xFFFF) | ((datahi << 16) & 0xFFFF0000); // upper 16 bits of data lo or lower 16 bits of data high
        for (i32 tile_x = 0; tile_x < 8; tile_x++, x++) {
            if (x < 0 || x >= width) continue; // x < 0 || x >= width
            u32 color;
            if (--mosaic_counter == 0) {
                i32 shift = mirror_x ? tile_x : 7 - tile_x;
                {
                    color = (datalo >> shift) & 1;
                    color += (datalo >> (shift + 7)) & 2; // 0-2 + 7-9
                }
                if (bg->tile_mode >= SPTM_BPP4) {
                    color += (datalo >> (shift + 14)) & 4; // bits 16-24
                    color += (datalo >> (shift + 21)) & 8; // bits 24-31
                }
                if (bg->tile_mode >= SPTM_BPP8) {
                    color += (datamid >> (shift + 12)) & 16;
                    color += (datamid >> (shift + 19)) & 32;
                    color += (datahi >> (shift + 10)) & 64;
                    color += (datahi >> (shift + 17)) & 128;
                }

                mosaic_counter = bg->mosaic.enable ? bg->mosaic.size << hires : 1;
                mosaic_palette = color;
                mosaic_priority = tile_priority;

                if (direct_color_mode) {
                    mosaic_color = direct_color(palette_number, mosaic_palette);
                } else {
                    mosaic_color = this->CGRAM[palette_index + mosaic_palette];
                }
            }
            if (!mosaic_palette) continue;

            if (!hires) {
                if (mosaic_priority > bg->line[x].priority) // && !PPUF_window_above[x]
                    setpx(&bg->line[x], source, mosaic_priority, mosaic_color, bpp, dbg_prio);
            } else {
                if (x & 1) {
                    if (mosaic_priority > bg->line[x].priority) // && !PPUF_window_above[bx]
                        setpx(&bg->line[x], source, mosaic_priority, mosaic_color, bpp, dbg_prio);
                } else {
                    if (bg->sub_enable && mosaic_priority > bg->line[x].priority) // !PPUF_window_below[bx] &&
                        setpx(&bg->line[x], source, mosaic_priority, mosaic_color, bpp, dbg_prio);
                }
            }
        }
    }
}

static void color_math(SNES_PPU *this, union SNES_PPU_px *main, union SNES_PPU_px *sub)
{
    main->color_math_flags = 1;
    i32 mr = main->color & 0x1F;
    i32 mg = (main->color >> 5) & 0x1F;
    i32 mb = (main->color >> 10) & 0x1F;
    i32 sr = sub->color & 0x1F;
    i32 sg = (sub->color >> 5) & 0x1F;
    i32 sb = (sub->color >> 10) & 0x1F;

    if (this->color_math.math_mode) {
        // 1 = sub
        mr -= sr;
        mg -= sg;
        mb -= sb;
        main->color_math_flags |= 2;
    }
    else {
        mr += sr;
        mg += sg;
        mb += sb;
    }
    if (mr < 0) mr = 0;
    if (mg < 0) mg = 0;
    if (mb < 0) mb = 0;
    if (this->color_math.halve) {
        main->color_math_flags |= 4;
        mr >>= 1;
        mg >>= 1;
        mb >>= 1;
    }
    if (mr > 0x1F) mr = 0x1F;
    if (mg > 0x1F) mg = 0x1F;
    if (mb > 0x1F) mb = 0x1F;
    main->color = mr | (mg << 5) | (mb << 10);
}

static void draw_sprite_line(SNES *snes, i32 ppu_y)
{
    struct SNES_PPU *this = &snes->ppu;
    memset(this->obj.line, 0, sizeof(this->obj.line));

    if (!this->obj.main_enable && !this->obj.sub_enable) return;
    static const i32 spwidth[8][2] = { // [this->io.obj.sz][sprite_sz] }
            {8, 16},
            {8, 32},
            {8, 64},
            {16, 32},
            {16, 64},
            {32, 64},
            {16, 32},
            {16, 32}
    };
    static const i32 spheight[8][2] = {
            {8, 16},
            {8, 32},
            {8, 64},
            {16, 32},
            {16, 64},
            {32, 64},
            {32, 64},
            {32, 32}
    };
    u32 item_limit = 32;
    u32 tile_limit = 34;
    for (u32 mbn = 0; mbn < 0x80; mbn++) {
        if ((item_limit < 1) || (tile_limit < 1)) return;
        u32 sn = (mbn + this->obj.first) & 0x7F;

        struct SNES_PPU_sprite *sp = &this->obj.items[sn];

        i32 height = spheight[this->io.obj.sz][sp->size];
        i32 width = spwidth[this->io.obj.sz][sp->size];

        if ((sp->x > 256) && ((sp->x + width - 1) < 512)) continue;
        if (!(((ppu_y >= sp->y) && (ppu_y < (sp->y + height))) ||
                (((sp->y + height) >= 256) && (ppu_y < ((sp->y + height) & 255)))
                )) {
            continue;
        }

        i32 sp_line = (ppu_y - sp->y) & 0xFF;
        item_limit--;

        u32 num_h_tiles = width >> 3;

        if (sp->vflip) {
            if (width == height)
                sp_line = (height - 1) - sp_line;
            else if (sp_line < width)
                sp_line = (width - 1) - sp_line;
            else
                sp_line = width + (width - 1) - (sp_line - width);
        }
        //sp->x &= 511;
        sp_line &= 255;

        i32 tile_xxor = sp->hflip ? ((width >> 3) - 1) : 0;
        i32 in_sp_x = 0;
        u32 tile_y = sp_line & 7;

        u32 tile_addr = this->io.obj.tile_addr + sp->name_select_add;
        u32 character_x = sp->tile_num & 15;
        u32 character_y = (((sp->tile_num >> 4) + (sp_line >> 3)) & 15) << 4;

        for (u32 n = 0; n < num_h_tiles; n++) {
            if (tile_limit < 1) return;
            u32 block_x = n ^ tile_xxor;
            u32 addr = tile_addr + ((character_y + (character_x + block_x & 15)) << 4);
            addr = (addr & 0x7FF0) + tile_y;
            u32 data = this->VRAM[addr] | (this->VRAM[(addr + 8) & 0x7FFF]) << 16;
            for (u32 pxn = 0; pxn < 8; pxn++) {
                i32 rx = sp->x + in_sp_x;
                if ((rx >= 0) && (rx < 256)) {
                    u32 mpx = sp->hflip ? pxn : 7 - pxn;
                    u32 color = (data >> mpx) & 1;
                    color += (data >> (mpx + 7)) & 2;
                    color += (data >> (mpx + 14)) & 4;
                    color += (data >> (mpx + 21)) & 8;
                    if (color != 0) {
                        union SNES_PPU_px *px = &this->obj.line[rx];
                        px->has = 1;
                        px->source = 4; // OBJ = 4
                        px->priority = this->obj.priority[sp->priority];
                        px->dbg_priority = sp->priority;
                        px->color = this->CGRAM[sp->pal_offset + color];
                        px->palette = sp->palette;
                    }
                }
                in_sp_x++;
            }
            tile_limit--;
        }
    }
}

#define SRC_BG1 0
#define SRC_BG2 1
#define SRC_BG3 2
#define SRC_BG4 3
#define SRC_OBJ_PAL03 4
#define SRC_OBJ_PAL47 5
#define SRC_BACK 6

static void draw_line(SNES *snes)
{
    // Draw sprite line
    struct SNES_PPU *this = &snes->ppu;
    //printf("\nDraw line %d", snes->clock.ppu.y);
    if (snes->clock.ppu.y == 0) memset(&snes->dbg_info.line, 0, sizeof(snes->dbg_info.line));
    draw_sprite_line(snes, snes->clock.ppu.y);
    draw_bg_line(snes, 0, snes->clock.ppu.y);
    draw_bg_line(snes, 1, snes->clock.ppu.y);
    draw_bg_line(snes, 2, snes->clock.ppu.y);
    draw_bg_line(snes, 3, snes->clock.ppu.y);
    for (u32 i = 0; i < 4; i++) {
        memcpy(snes->dbg_info.line[snes->clock.ppu.y].bg[i].px, this->bg[i].line, sizeof(this->bg[i].line));
    }
    memcpy(snes->dbg_info.line[snes->clock.ppu.y].sprite_px, this->obj.line, sizeof(this->obj.line));
    u16 *line_output = this->cur_output + (snes->clock.ppu.y * 512);
    for (u32 x = 0; x < 256; x++) {
        union SNES_PPU_px main_px = {.source=SRC_BACK};
        union SNES_PPU_px sub_px = {.source=SRC_BACK};
        sub_px.priority = 15;
        sub_px.color = this->color_math.fixed_color;
        sub_px.has = 0;
        main_px.priority = 15;
        main_px.color = this->CGRAM[0];
        main_px.has = 0;
#define MAINPRIO 0
#define SUBPRIO 1
        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            struct SNES_PPU_BG *bg = &this->bg[bgnum];
            union SNES_PPU_px *cmp = &bg->line[x];
            if (bg->main_enable && cmp->has && (main_px.priority > cmp->priority)) {
                main_px.color = cmp->color;
                main_px.has = 1;
                main_px.priority = cmp->priority;
                main_px.source = bgnum;
                main_px.dbg_priority = bg->priority[MAINPRIO];
            }
            if (this->color_math.blend_mode && bg->sub_enable && cmp->has && (sub_px.priority > cmp->priority)) {
                sub_px.color = cmp->color;
                sub_px.has = 1;
                sub_px.priority = cmp->priority;
                sub_px.source = bgnum;
            }
        }
        union SNES_PPU_px *cmp = &this->obj.line[x];
        if (this->obj.main_enable && this->obj.main_enable && cmp->has && (main_px.priority > cmp->priority)) {
            main_px.color = cmp->color;
            main_px.has = 1;
            main_px.priority = cmp->priority;
            main_px.source = cmp->palette < 4 ? SRC_OBJ_PAL03 : SRC_OBJ_PAL47;
        }
        if (this->color_math.blend_mode && this->obj.sub_enable && cmp->has && (sub_px.priority > cmp->priority)) {
            sub_px.color = cmp->color;
            sub_px.has = 1;
            sub_px.priority = cmp->priority;
            sub_px.source = cmp->palette < 4 ? SRC_OBJ_PAL03 : SRC_OBJ_PAL47;
        }

        main_px.color_math_flags = 0;
        if (this->color_math.enable[main_px.source]) color_math(this, &main_px, &sub_px);

        u16 c = main_px.color;
        // this->io.screen_brightness
        u32 b = (c >> 10) & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 r = c & 0x1F;
        b *= this->io.screen_brightness;
        g *= this->io.screen_brightness;
        r *= this->io.screen_brightness;
        b /= 15;
        g /= 15;
        r /= 15;
        if (r > 0x1F) r = 0x1F;
        if (g > 0x1F) g = 0x1F;
        if (b > 0x1F) b = 0x1F;
        c = (b << 10) | (g << 5) | r;
        line_output[x*2] = c;
        line_output[(x*2)+1] = c;
    }
}


static void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    snes->clock.ppu.hblank_active = key;

    R5A22_hblank(snes, key);
    // Draw layers and display

    if (key == 0) {
        // Draw bg
        // Output bg &  sprite
        if (snes->clock.ppu.y < 224)
            draw_line(snes);
    }
    else { // HBLANK before end of line.
    }

    // Do hblank DMA? no this is scheduled

}

static inline u32 ch_hdma_is_active(R5A22_DMA_CHANNEL *ch)
{
    return ch->hdma_enable && !ch->hdma_completed;
}

static u32 hdma_is_finished(R5A22_DMA_CHANNEL *this)
{
    struct R5A22_DMA_CHANNEL *ch = this->next;
    while(ch != NULL) {
        if (ch_hdma_is_active(ch)) return 0;
        ch = ch->next;
    }
    return true;

}

u32 SNES_hdma_reload_ch(SNES *snes, R5A22_DMA_CHANNEL *ch)
{
    u32 cn = 8;
    u32 data = SNES_wdc65816_read(snes, (ch->source_bank << 16 | ch->hdma_address), 0, 1);
    if ((ch->line_counter & 0x7F) == 0) {
        ch->line_counter = data;
        ch->hdma_address++;

        ch->hdma_completed = ch->line_counter == 0;
        ch->hdma_do_transfer = !ch->hdma_completed;

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

static u32 hdma_setup_ch(SNES *snes, R5A22_DMA_CHANNEL *ch)
{
    ch->hdma_do_transfer = 1;
    if (!ch->hdma_enable) return 0;

    ch->dma_enable = 0; // Stomp on DMA if HDMA runs
    ch->hdma_address = ch->source_address;
    //ch->line_counter = 0;
    return SNES_hdma_reload_ch(snes, ch);
}

static void hdma_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    u32 cn = 8;
    for (u32 n = 0; n < 8; n++) {
        cn += hdma_setup_ch(snes, &snes->r5a22.dma.channels[n]);
    }
    scheduler_from_event_adjust_master_clock(&snes->scheduler, cn);
}

static u32 hdma_is_enabled(SNES *snes)
{
    for (u32 n = 0; n < 8; n++) {
        if (snes->r5a22.dma.channels[n].hdma_enable) return 1;
    }
    return 0;
}

static void hdma(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    if (hdma_is_enabled(snes)) {
        snes->r5a22.status.dma_running = 1;
    }
}

static void assert_hirq(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    struct R5A22 *this = &snes->r5a22;
    this->status.hirq_line = key;
    R5A22_update_irq(snes);
    if (key) DBG_EVENT(DBG_SNES_EVENT_HIRQ);
}

static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (SNES*)ptr;
    u64 cur = clock - jitter;
    u32 vblank = snes->clock.ppu.y >= 224;
    new_scanline(snes, cur);

    // hblank DMA setup, 12-20ish
    if (!vblank) {
        snes->clock.timing.line.hdma_setup_position = snes->clock.rev == 1 ? 12 + 8 - (snes->clock.master_cycle_count & 7) : 12 + (snes->clock.master_cycle_count & 7);
        scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hdma_setup_position, 0, snes, &hdma_setup, NULL);
    }

    // hblank out - 108 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hblank_stop, 0, snes, &hblank, NULL);

    // DRAM refresh - ~510 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.dram_refresh, 0, snes, &dram_refresh, NULL);


    // HIRQ
    if (snes->r5a22.io.irq_enable) {
        if (!snes->r5a22.io.hirq_enable) assert_hirq(snes, 1, cur, 0);
        else
            snes->ppu.hirq.sched_id = scheduler_only_add_abs(&snes->scheduler, cur + ((snes->r5a22.io.htime + 21) * 4), 1, snes, &assert_hirq, &snes->ppu.hirq.still_sched);
    }
    R5A22_update_irq(snes);

    // HDMA - 1104 ish
    if (!vblank) {
        scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hdma_position, 0, snes, &hdma, NULL);
    }

    // Hblank in - 1108 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hblank_start, 1, snes, &hblank, NULL);

    // new line - 1364
    scheduler_only_add_abs_w_tag(&snes->scheduler, cur + snes->clock.timing.line.master_cycles, 0, snes, &schedule_scanline, NULL, 1);
}

static void vblank(void *ptr, u64 key, u64 cur_clock, u32 jitter)
{
    struct SNES *snes = (SNES *)ptr;
    snes->clock.ppu.vblank_active = key;
    snes->r5a22.status.nmi_flag = key;
    if (!key) { // VBLANK off at start of frame
        //printf("\nNMI OFF");
        snes->ppu.obj.range_overflow = snes->ppu.obj.time_overflow = 0;
    }
    else { // VBLANK on partway through frame
        //printf("\nNMI ON");
    }
    R5A22_update_nmi(snes);
}

static void new_frame(void* ptr, u64 key, u64 cur_clock, u32 jitter)
{
    u64 cur = cur_clock - jitter;
    //printf("\nNEW FRAME @%lld", cur);
    struct SNES* snes = (SNES*)ptr;

    if (snes->dbg.events.view.vec) {
        debugger_report_frame(snes->dbg.interface);
    }
    snes->ppu.cur_output = ((u16 *)snes->ppu.display->output[snes->ppu.display->last_written ^ 1]);
    snes->clock.master_frame++;

    snes->clock.ppu.field ^= 1;
    snes->clock.ppu.y = -1;

    /*snes->clock.ppu.scanline_start = cur_clock;
    if (snes->dbg.events.view.vec) {
        debugger_report_line(snes->dbg.interface, 0);
    }*/

    snes->clock.ppu.vblank_active = 0;
    snes->ppu.cur_output = snes->ppu.display->output[snes->ppu.display->last_written];
    memset(snes->ppu.cur_output, 0, 256*224*2);
    snes->ppu.display->last_written ^= 1;

    snes->ppu.display->scan_y = 0;
    snes->ppu.display->scan_x = 0;

    // vblank 1->0
    vblank(snes, 0, cur_clock, jitter);

    //set_clock_divisor(this);
    scheduler_only_add_abs(&snes->scheduler, cur + (snes->clock.timing.line.master_cycles * 225), 1, snes, &vblank, NULL);
    scheduler_only_add_abs_w_tag(&snes->scheduler, cur + snes->clock.timing.frame.master_cycles, 0, snes, &new_frame, NULL, 2);
}


void SNES_PPU_schedule_first(SNES *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs_w_tag(&this->scheduler, this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}