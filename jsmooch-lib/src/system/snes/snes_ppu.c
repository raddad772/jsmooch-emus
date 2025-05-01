//
// Created by . on 4/21/25.
//

#include <string.h>
#include "snes_ppu.h"
#include "snes_bus.h"

#include "helpers/multisize_memaccess.c"

#define PPU_src_BG1 0
#define PPU_src_BG2 1
#define PPU_src_BG3 2
#define PPU_src_BG4 3
#define PPU_src_OBJ1 4
#define PPU_src_OBJ2 5
#define PPU_src_COL 6

static int did_math = 0;
static u64 gfxshift[256];

static void do_math()
{
    did_math = 1;
    u32 space;
    for (u32 rep = 0; rep < 3; rep++) {
        for (u64 i = 0; i < 256; i++) {
            u64 shift = 0;
            u64 v = 0;
            for (u64 bn = 0; bn < 8; bn++) {
                v = (v << 8) | ((i >> shift) & 1);
                shift++;
            }
            gfxshift[i] = v;
        }
    }
}

void SNES_PPU_init(struct SNES *this)
{
    if (!did_math) do_math();
}

void SNES_PPU_delete(struct SNES *this)
{
    
}

void SNES_PPU_reset(struct SNES *this)
{
    
}

static u32 read_oam(struct SNES_PPU *this, u32 addr)
{
    return this->OAM[addr];
}

static void write_oam(struct SNES *snes, struct SNES_PPU *this, u32 addr, u32 val) {
    if (!snes->clock.ppu.vblank_active && !this->io.force_blank) {
        printf("\nWARN OAM BAD WRITE TIME");
        return;
    }
    this->OAM[addr] = val;
}

static u32 get_addr_by_map(struct SNES_PPU *this)
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

static void update_video_mode(struct SNES_PPU *this)
{
		//snes->clock.scanline.bottom_scanline = this->overscan ? 240 : 225;
#define BGP(num, main, sub) this->bg[num].priority[0] = main; this->bg[num].priority[1] = sub
#define OBP(n0, n1, n2, n3) this->obj.priority[0] = n0; this->obj.priority[1] = n1; this->obj.priority[2] = n2; this->obj.priority[3] = n3
		switch(this->io.bg_mode) {
			case 0:
				this->bg[0].tile_mode = SPTM_BPP2;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_BPP2;
				this->bg[3].tile_mode = SPTM_BPP2;
				BGP(0, 8, 11);
				BGP(1, 7, 10);
				BGP(2, 2, 5);
				BGP(3, 1, 4);
				OBP(3, 6, 9, 12);
				break;
			case 1:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_BPP2;
				this->bg[3].tile_mode = SPTM_inactive;
				if (this->io.bg_priority) {
					BGP(0, 5, 8);
					BGP(1, 4, 7);
					BGP(2, 1, 10);
					OBP(2, 3, 6, 9);
				} else {
					BGP(0, 6, 9);
					BGP(1, 5, 8);
					BGP(2, 1, 3);
					OBP(2, 4, 7, 10);
				}
				break;
			case 2:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(0, 6, 9);
				BGP(1, 5, 8);
				OBP(2, 4, 7, 10);
				break;
			case 3:
				this->bg[0].tile_mode = SPTM_BPP8;
				this->bg[1].tile_mode = SPTM_BPP4;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(0, 3, 7);
				BGP(1, 1, 5);
				OBP(2, 4, 6, 8);
				break;
			case 4:
				this->bg[0].tile_mode = SPTM_BPP8;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(0, 3, 7);
				BGP(1, 1, 5);
				OBP(2, 4, 6, 8);
				break;
			case 5:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_BPP2;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(0, 3, 7);
				BGP(1, 1, 5);
				OBP(2, 4, 6, 8);
				break;
			case 6:
				this->bg[0].tile_mode = SPTM_BPP4;
				this->bg[1].tile_mode = SPTM_inactive;
				this->bg[2].tile_mode = SPTM_inactive;
				this->bg[3].tile_mode = SPTM_inactive;
				BGP(0, 2, 5);
				OBP(1, 3, 4, 6);
				break;
			case 7:
				if (!this->io.extbg) {
					this->bg[0].tile_mode = SPTM_mode7;
					this->bg[1].tile_mode = SPTM_inactive;
					this->bg[2].tile_mode = SPTM_inactive;
					this->bg[3].tile_mode = SPTM_inactive;
					BGP(0, 2, 2);
					OBP(1, 3, 4, 5);
				} else {
					this->bg[0].tile_mode = SPTM_mode7;
					this->bg[1].tile_mode = SPTM_mode7;
					this->bg[2].tile_mode = SPTM_inactive;
					this->bg[3].tile_mode = SPTM_inactive;
					BGP(0, 3, 3);
					BGP(1, 1, 5);
					OBP(2, 4, 6, 7);
				}
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
                    bg->palette_mask = 0;
                    bg->num_planes = 4;
                    bg->tile_bytes = 0x40;
                    bg->tile_row_bytes = 8;
                    break;
                case SPTM_inactive:
                    break;
                case SPTM_mode7:
                    printf("\nWARN MODE7 NOT IMPLEMENT");
                    break;
            }
        }
}

void SNES_PPU_write(struct SNES *snes, u32 addr, u32 val, struct SNES_memmap_block *bl) {
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
            this->io.oam.base_addr = (this->io.oam.base_addr & 0xFE00) | (val << 1);
            this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            return;
        case 0x2103: // OAMADDH
            this->io.oam.base_addr = (this->io.oam.base_addr & 0x1FE) | ((val & 1) << 9);
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
            printf("\nBG MODE %d", this->io.bg_mode);
            if (old_bg_mode != this->io.bg_mode) {
                printf("\nNEW BG MODE %d", this->io.bg_mode);
            }
            this->io.bg_priority = (val >> 4) & 1;
            this->bg[0].io.tile_size = (val >> 4) & 1;
            this->bg[1].io.tile_size = (val >> 5) & 1;
            this->bg[2].io.tile_size = (val >> 6) & 1;
            this->bg[3].io.tile_size = (val >> 7) & 1;
            printf("\nTILE SIZES: %d %d %d %d", this->bg[0].io.tile_size, this->bg[1].io.tile_size, this->bg[2].io.tile_size, this->bg[3].io.tile_size);
            update_video_mode(this);
            return;
        }
        case 0x2106: {// MOSAIC
            u32 latchbit = this->bg[0].mosaic.enable | this->bg[1].mosaic.enable | this->bg[2].mosaic.enable |
                           this->bg[3].mosaic.enable;
            this->bg[0].mosaic.enable = (val >> 0) & 1;
            this->bg[1].mosaic.enable = (val >> 1) & 1;
            this->bg[2].mosaic.enable = (val >> 2) & 1;
            this->bg[3].mosaic.enable = (val >> 3) & 1;
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
            this->io.vram.addr = (this->io.vram.addr & 0xFF00) + val;
            this->latch.vram = this->VRAM[get_addr_by_map(this)];
            return;
        case 0x2117: // VRAM address hi
            this->io.vram.addr = (val << 8) + (this->io.vram.addr & 0xFF);
            this->latch.vram = this->VRAM[get_addr_by_map(this)];
            return;
        case 0x2118: // VRAM data lo
            if (!snes->clock.ppu.vblank_active && !this->io.force_blank) return;
            addre = get_addr_by_map(this);
            this->VRAM[addre] = (this->VRAM[addre] & 0xFF00) | val;
            if (this->io.vram.increment_mode == 0) this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            return;
        case 0x2119: // VRAM data hi
            if (!snes->clock.ppu.vblank_active && !this->io.force_blank) return;
            addre = get_addr_by_map(this);
            this->VRAM[addre] = (val << 8) | (this->VRAM[addre] & 0xFF);
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
                //printf("\nCRAM WRITE %02x: %04x", this->io.cgram_addr, ((val & 0x7F) << 8) | this->latch.cgram);
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
            SNES_wdc65816_write(snes, 0x7E0000 | this->io.wram_addr++, val);
            if (this->io.wram_addr > 0x1FFFF) this->io.wram_addr = 0;
            return;
        case 0x2181: // WRAM addr low
            this->io.wram_addr = (this->io.wram_addr & 0xFFF00) + val;
            return;
        case 0x2182: // WRAM addr med
            this->io.wram_addr = (val << 8) + (this->io.wram_addr & 0xF00FF);
            return;
        case 0x2183: // WRAM bank
            this->io.wram_addr = ((val & 1) << 16) | (this->io.wram_addr & 0xFFFF);
            return;
    }
    printf("\nWARN SNES PPU WRITE NOT IN %04x %02x", addr, val);
    static int num = 0;
    num++;
    if (num == 10) {
        dbg_break("PPU WRITE TOOMANYBAD", snes->clock.master_cycle_count);
    }
}

static u32 mode7_mul(struct SNES_PPU *this)
{
    return (u32)((i32)(this->mode7.a) * (i32)(i8)(this->mode7.b >> 8));
}

u32 SNES_PPU_read(struct SNES *snes, u32 addr, u32 old, u32 has_effect, struct SNES_memmap_block *bl)
{
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
            if (snes->r5a22.io.pio & 0x80) SNES_latch_ppu_counters(snes);
            return old;
        case 0x2138: {// OAMDATAREAD
            u32 data = read_oam(this, this->io.oam.addr);
            this->io.oam.addr = (this->io.oam.addr + 1) & 0x3FF;
            this->obj.first = this->io.oam.priority ? (this->io.oam.addr >> 2) & 0x7F : 0;
            return data; }
        case 0x2139: // VMDATAREADL
            result = this->latch.vram & 0xFF;
            if (this->io.vram.increment_mode == 0) {
                this->latch.vram = this->VRAM[get_addr_by_map(this)];
                this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213A: // VMDATAREADH
            result = (this->latch.vram >> 8) &0xFF;
            if (this->io.vram.increment_mode == 1) {
                this->latch.vram = this->VRAM[get_addr_by_map(this)];
                this->io.vram.addr = (this->io.vram.addr + this->io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213D: // OPVCT
            if (this->latch.vcounter == 0) {
                this->latch.vcounter = 1;
                this->latch.ppu2.mdr = snes->clock.ppu.y;
            } else {
                this->latch.vcounter = 0;
                this->latch.ppu2.mdr = (snes->clock.ppu.y >> 8) | (this->latch.ppu2.mdr & 0xFE);
            }
            return this->latch.ppu2.mdr;
        case 0x213E: // STAT77
            this->latch.ppu1.mdr = 1 | (this->obj.range_overflow << 6) | (this->obj.time_overflow << 7);
            return this->latch.ppu1.mdr;
        case 0x213F:
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
            return this->latch.ppu2.mdr;
        case 0x2180: {// WRAM access port
            u32 r = SNES_wdc65816_read(snes, 0x7E0000 | this->io.wram_addr, 0, has_effect);
            if (has_effect) {
                this->io.wram_addr++;
                if (this->io.wram_addr > 0x1FFFF) this->io.wram_addr = 0;
            }
            return r; }
    }
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

struct slice {
    struct SNES_PPU_px px[16];
    u32 priority;
};


static void fetch_slice(struct SNES *snes, struct SNES_PPU_BG *bg, u32 coarse_col, u32 row, struct slice *slice)
{
    struct SNES_PPU *this = &snes->ppu;
    // fetch entry from nametable
    u32 coarse_row = row >> bg->scroll_shift;
    u32 fine_row = row & bg->tile_px_mask;
    u32 nt_addr = bg->io.screen_addr;

    u32 hires = this->io.bg_mode == 5 || this->io.bg_mode == 6;
    u32 tile_height = 3 + bg->io.tile_size;
    u32 tile_width = !hires ? tile_height : 4;

    u32 screen_x = bg->io.screen_size & 1 ? 0x400 : 0;
    u32 screen_y = bg->io.screen_size & 2 ? 32 << (5 + (bg->io.screen_size & 1)) : 0;

    u32 tx = coarse_col;
    u32 ty = coarse_row;

    u32 offset = ((ty & 0x1F) << 5) | (tx & 0x1F);
    if (tx & 0x20) offset += screen_x;
    if (ty & 0x20) offset += screen_y;

    // 64 bytes per line
    // but in 64-wide mode, there's
    /*nt_addr += (64 * coarse_row) + (2 * coarse_col);
    if ((bg->cols == 64) && (coarse_col & 1)) {
        // left and right-hand tables
        nt_addr += (64 * bg->rows);
    }*/

    u32 entry = snes->ppu.VRAM[bg->io.screen_addr + offset];
    u32 vflip = (entry >> 15) & 1;
    u32 hflip = (entry >> 14) & 1;
    u32 priority = (entry >> 13) & 1;
    u32 palette = (entry >> 10) & 7;
    u32 tile_num = entry & 1023;

    if (vflip) fine_row = bg->tile_px_mask - fine_row;
    slice->priority = priority;
    u32 pal_base = bg->palette_offset + ((palette & bg->palette_mask) << bg->palette_shift);

    // So we have to get the bitplanes of the correct row.
    // If tiles are 16x16 and we're on , we pick the "next" tile
    u32 block_x = 0, block_y = 0;
    if (bg->io.tile_size) {
        if (fine_row > 7) nt_addr += bg->tile_bytes * 0x10;
    }
    nt_addr += ((fine_row & 7) * bg->tile_row_bytes);
    i32 tile_x = hflip ? bg->tile_px_mask : 0;
    i32 tile_add = hflip ? -1 : 1;
    // Now grab 1-2 8-pixel slices
    u64 pixel_data = 0;
    for (u32 slice_num = 0; slice_num < bg->io.tile_size+1; slice_num++) {
        u32 my_addr = nt_addr;
        my_addr += (slice_num ^ hflip) * bg->tile_bytes;
        if (slice_num && bg->io.tile_size) pixel_data <<= 8;

        // Grab 1-4 2-planes of data
        for (u32 plane_num = 0; plane_num < bg->num_planes; plane_num++) {
            u16 pixels = snes->ppu.VRAM[my_addr >> 1];
            u64 data = gfxshift[pixels & 0xFF];
            data |= gfxshift[pixels >> 1] << 1;
            pixel_data |= data << (2 * plane_num);
            my_addr++;
        }
    }

    const u64 mask[3] = { 3, 15, 255};
    const u64 shift[3] = { 2, 4, 8 };
    for (u32 pnum = 0; pnum < bg->tile_px; pnum++) {
        u32 c = pixel_data & mask[bg->tile_mode];
        pixel_data >>= shift[bg->tile_mode];
        slice->px[tile_x].has = c != 0;
        slice->px[tile_x].color = pal_base + c;
        tile_x += tile_add;
    }
}

static inline void render_to_bgbuffer(struct SNES *snes, struct SNES_PPU_BG *bg, struct slice *slice, u32 start, u32 end)
{
    for (u32 i = start; i < end; i++) {
        bg->line[bg->screen_x].has = slice->px[i].has;
        bg->line[bg->screen_x].color = slice->px[i].color;
        bg->screen_x++;
        if (bg->screen_x >= 256) return;
    }
}

static void draw_bg_line(struct SNES *snes, u32 bgnum, u32 y)
{
    struct SNES_PPU *this = &snes->ppu;
    struct SNES_PPU_BG *bg = &this->bg[bgnum];
    memset(bg->line, 0, sizeof(bg->line));
    if (bg->tile_mode == SPTM_inactive) return;
    if (!bg->main_enable && !bg->sub_enable) return;

    // So for a 256-pixel screen, we must do 33 fetches. 1 for 0-7 pixels off the left side, the rest for normal stuff
    // Kinda like we do on the Sega Genesis, because we can.

    u32 voffset_per_tile = this->io.bg_mode == 2 || this->io.bg_mode == 4 || this->io.bg_mode == 6;
    u32 direct_color_mode = this->color_math.direct_color && bgnum == 0 && ((this->io.bg_mode == 3) || (this->io.bg_mode == 4));
    u32 color_shift = 3 + bg->io.tile_size;
    // if it's 16 wide or tall, then that scroll is going to go half as far.
    bg->screen_x = 0;
    u32 xscrl = bg->io.hoffset & bg->pixels_h_mask;

    u32 fine_x = xscrl & bg->tile_px_mask;
    u32 col_coarse = (xscrl >> bg->scroll_shift) & bg->pixels_h_mask;
    u32 row = (y + bg->io.voffset) & bg->pixels_v_mask;

    struct slice slice;

    // render left column
    fetch_slice(snes, bg, col_coarse, row, &slice);
    col_coarse = (col_coarse + 1) & (bg->col_mask);
    render_to_bgbuffer(snes, bg, &slice, bg->tile_px_mask - fine_x, bg->tile_px);

    // render the rest
    while (bg->screen_x < 256) {
        fetch_slice(snes, bg, col_coarse, row, &slice);
        render_to_bgbuffer(snes, bg, &slice, 0, 15);
        col_coarse = (col_coarse + 1) & (bg->col_mask);
    }
}

static void draw_sprite_line(struct SNES *snes)
{
    struct SNES_PPU *this = &snes->ppu;
}

static void draw_line(struct SNES *snes)
{
    // Draw sprite line
    struct SNES_PPU *this = &snes->ppu;
    draw_sprite_line(snes);
    draw_bg_line(snes, 0, snes->clock.ppu.y);
    draw_bg_line(snes, 1, snes->clock.ppu.y);
    draw_bg_line(snes, 2, snes->clock.ppu.y);
    draw_bg_line(snes, 3, snes->clock.ppu.y);
    u16 *line_output = this->cur_output + (snes->clock.ppu.y * 512);
    for (u32 x = 0; x < 256; x++) {
        u16 c = this->bg[2].line[x].color;
        if (x == 90) c = 0x7FFF;
        if (snes->clock.ppu.y == 100) c = 0x7FFF;
        line_output[x*2] = c;
        line_output[(x*2)+1] = c;
    }
}


static void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
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

u32 SNES_hdma_reload_ch(struct SNES *snes, struct R5A22_DMA_CHANNEL *ch)
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
    return SNES_hdma_reload_ch(snes, ch);
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

static u32 hdma_is_enabled(struct SNES *snes)
{
    for (u32 n = 0; n < 8; n++) {
        if (snes->r5a22.dma.channels[n].hdma_enable) return 1;
    }
    return 0;
}

static void hdma(void *ptr, u64 key, u64 clock, u32 jitter)
{
    struct SNES *snes = (struct SNES *)ptr;
    if (hdma_is_enabled(snes)) {
        snes->r5a22.status.dma_running = 1;
    }
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
    struct SNES *snes = (struct SNES*)ptr;
    u64 cur = clock - jitter;
    u32 vblank = snes->clock.ppu.y >= 224;
    new_scanline(snes, cur);

    // hblank DMA setup, 12-20ish
    if (!vblank) {
        scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hdma_setup_position, 0, snes, &hdma_setup, NULL);
    }

    // hblank out - 108 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hblank_stop, 0, snes, &hblank, NULL);

    // DRAM refresh - ~510 ish
    snes->clock.timing.line.dram_refresh = snes->clock.rev == 1 ? 12 + 8 - (snes->clock.master_cycle_count & 7) : 12 + (snes->clock.master_cycle_count & 7);
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
    struct SNES *snes = (struct SNES *)ptr;
    snes->clock.ppu.vblank_active = key;
    snes->r5a22.status.nmi_flag = key;
    if (!key) { // VBLANK off at start of frame
        snes->ppu.obj.range_overflow = snes->ppu.obj.time_overflow = 0;
    }
    else { // VBLANK on partway through frame
    }
    R5A22_update_nmi(snes);
}

static void new_frame(void* ptr, u64 key, u64 cur_clock, u32 jitter)
{
    u64 cur = cur_clock - jitter;
    //printf("\nNEW FRAME @%lld", cur);
    struct SNES* snes = (struct SNES*)ptr;

    debugger_report_frame(snes->dbg.interface);
    snes->ppu.cur_output = ((u16 *)snes->ppu.display->output[snes->ppu.display->last_written ^ 1]);
    snes->clock.master_frame++;

    snes->clock.ppu.field ^= 1;
    snes->clock.ppu.y = 0;
    snes->clock.ppu.vblank_active = 0;
    snes->ppu.cur_output = snes->ppu.display->output[snes->ppu.display->last_written];
    memset(snes->ppu.cur_output, 0, 256*224*2);
    snes->ppu.display->last_written ^= 1;

    snes->ppu.display->scan_y = 0;
    snes->ppu.display->scan_x = 0;

    // vblank 1->0
    vblank(snes, 0, cur_clock, jitter);

    //set_clock_divisor(this);
    scheduler_only_add_abs(&snes->scheduler, cur + (snes->clock.timing.line.master_cycles * 224), 1, snes, &vblank, NULL);
    scheduler_only_add_abs_w_tag(&snes->scheduler, cur + snes->clock.timing.frame.master_cycles, 0, snes, &new_frame, NULL, 2);
}


void SNES_PPU_schedule_first(struct SNES *this)
{
    schedule_scanline(this, 0, 0, 0);
    scheduler_only_add_abs_w_tag(&this->scheduler, this->clock.timing.frame.master_cycles, 0, this, &new_frame, NULL, 2);
}