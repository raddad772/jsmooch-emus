
// Created by . on 4/21/25.
//
#include <cassert>
#include <cstring>

#include "snes_ppu.h"
#include "snes_bus.h"
#include "snes_debugger.h"

#define PPU_src_BG1 0
#define PPU_src_BG2 1
#define PPU_src_BG3 2
#define PPU_src_BG4 3
#define PPU_src_OBJ1 4
#define PPU_src_OBJ2 5
#define PPU_src_COL 6

void SNES_PPU::reset()
{
    mode7.a = mode7.d = 0x100;
    mode7.b = mode7.c = 0;
}

u32 SNES_PPU::read_oam(u32 addr)
{
    u32 n;
    if (!(addr & 0x200)) {
        n = addr >> 2;
        addr &= 3;
        switch(addr) {
            case 0:
                return obj.items[n].x & 0xFF;
            case 1:
                return obj.items[n].y & 0xFF;
            case 2:
                return obj.items[n].tile_num;
            default:
        }
        return (obj.items[n].name_select) | (obj.items[n].palette << 1) | (obj.items[n].priority << 4) | (obj.items[n].hflip << 6) | (obj.items[n].vflip << 7);
    } else {
        n = (addr & 0x1F) << 2;
        return (obj.items[n].x >> 8) |
        ((obj.items[n+1].x >> 8) << 2) |
        ((obj.items[n+2].x >> 8) << 4) |
        ((obj.items[n+3].x >> 8) << 6) |
        (obj.items[n].size << 1) |
        (obj.items[n+1].size << 3) |
        (obj.items[n+2].size << 5) |
        (obj.items[n+3].size << 7);
    }
}

void SNES_PPU::write_oam(u32 addr, u32 val) {
    if (!snes->clock.ppu.vblank_active && !io.force_blank) {
        printf("\nWARN OAM BAD WRITE TIME");
        return;
    }
    if (addr < 0x200) { // 0-0x1FF
        u32 n = addr >> 2;
        if (n < 128) {
            SNES_PPU_sprite *sp = &obj.items[n];
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
        obj.items[n].x = (obj.items[n].x & 0xFF) | ((val << 8) & 0x100);
        obj.items[n+1].x = (obj.items[n+1].x & 0xFF) | ((val << 6) & 0x100);
        obj.items[n+2].x = (obj.items[n+2].x & 0xFF) | ((val << 4) & 0x100);
        obj.items[n+3].x = (obj.items[n+3].x & 0xFF) | ((val << 2) & 0x100);
        obj.items[n].size = (val >> 1) & 1;
        obj.items[n+1].size = (val >> 3) & 1;
        obj.items[n+2].size = (val >> 5) & 1;
        obj.items[n+3].size = (val >> 7) & 1;
    }
}

u32 SNES_PPU::get_addr_by_map()
{
    u32 addr = io.vram.addr;
    switch(io.vram.mapping) {
        case 0: return addr;
        case 1:
            return (addr & 0x7F00) | ((addr << 3) & 0x00F8) | ((addr >> 5) & 7);
        case 2:
            return (addr & 0x7E00) | ((addr << 3) & 0x01F8) | ((addr >> 6) & 7);
        case 3:
            return (addr & 0x7C00) | ((addr << 3) & 0x03F8) | ((addr >> 7) & 7);
        default:
    }
    return 0x8000;
}

void SNES_PPU::update_video_mode()
{
		//snes->clock.scanline.bottom_scanline = overscan ? 240 : 225;
#define BGP(num, lo, hi) pbg[num-1].priority[0] = lo; pbg[num-1].priority[1] = hi
#define OBP(n0, n1, n2, n3) obj.priority[0] = n0; obj.priority[1] = n1; obj.priority[2] = n2; obj.priority[3] = n3
		switch(io.bg_mode) {
			case 0:
				pbg[0].tile_mode = SNES_PPU_BG::BPP2;
				pbg[1].tile_mode = SNES_PPU_BG::BPP2;
				pbg[2].tile_mode = SNES_PPU_BG::BPP2;
				pbg[3].tile_mode = SNES_PPU_BG::BPP2;
				BGP(1, 5, 2);
				BGP(2, 6, 3);
				BGP(3, 11, 8);
				BGP(4, 12, 9);
				OBP(10, 7, 4, 1);
				break;
			case 1:
				pbg[0].tile_mode = SNES_PPU_BG::BPP4;
				pbg[1].tile_mode = SNES_PPU_BG::BPP4;
				pbg[2].tile_mode = SNES_PPU_BG::BPP2;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
                BGP(1, 5, 2);
                BGP(2, 6, 3);
                OBP(10, 7, 4, 1);
				if (io.bg_priority) {
					BGP(3, 12, 8);
				} else {
					BGP(3, 11, 0);
				}
				break;
			case 2:
				pbg[0].tile_mode = SNES_PPU_BG::BPP4;
				pbg[1].tile_mode = SNES_PPU_BG::BPP4;
				pbg[2].tile_mode = SNES_PPU_BG::inactive;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
				BGP(1, 8, 2);
				BGP(2, 11, 5);
				OBP(10, 7, 4, 1);
				break;
			case 3:
				pbg[0].tile_mode = SNES_PPU_BG::BPP8;
				pbg[1].tile_mode = SNES_PPU_BG::BPP4;
				pbg[2].tile_mode = SNES_PPU_BG::inactive;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
				BGP(1, 8, 2);
				BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 4:
				pbg[0].tile_mode = SNES_PPU_BG::BPP8;
				pbg[1].tile_mode = SNES_PPU_BG::BPP2;
				pbg[2].tile_mode = SNES_PPU_BG::inactive;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
                BGP(1, 8, 2);
                BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 5:
				pbg[0].tile_mode = SNES_PPU_BG::BPP4;
				pbg[1].tile_mode = SNES_PPU_BG::BPP2;
				pbg[2].tile_mode = SNES_PPU_BG::inactive;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
                BGP(1, 8, 2);
                BGP(2, 11, 5);
                OBP(10, 7, 4, 1);
				break;
			case 6:
				pbg[0].tile_mode = SNES_PPU_BG::BPP4;
				pbg[1].tile_mode = SNES_PPU_BG::inactive;
				pbg[2].tile_mode = SNES_PPU_BG::inactive;
				pbg[3].tile_mode = SNES_PPU_BG::inactive;
                BGP(1, 8, 2);
                OBP(10, 7, 4, 1);
				break;
			case 7:
                pbg[0].tile_mode = SNES_PPU_BG::mode7;
                pbg[1].tile_mode = io.extbg ? SNES_PPU_BG::mode7 : SNES_PPU_BG::inactive;
                pbg[2].tile_mode = SNES_PPU_BG::inactive;
                pbg[3].tile_mode = SNES_PPU_BG::inactive;
                BGP(1, 8, 8);
                BGP(2, 5, 11);
                OBP(10, 7, 4, 1);
				break;
		    default:
		        assert(1==2);
		        break;
		}
        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            constexpr u32 theight[4] = { 32, 32, 64, 64 };
            constexpr u32 twidth[4] = { 32, 64, 32, 64};
            SNES_PPU_BG *bg = &pbg[bgnum];
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
                case SNES_PPU_BG::BPP2:
                    bg->palette_offset = 0;
                    if (io.bg_mode == 0) {
                        bg->palette_offset = 0x20 * bgnum;
                    }
                    bg->palette_shift = 2; // * 4
                    bg->palette_mask = 7;
                    bg->num_planes = 1;
                    bg->tile_bytes = 0x10;
                    bg->tile_row_bytes = 2;
                    break;
                case SNES_PPU_BG::BPP4:
                    bg->palette_shift = 4; // * 16
                    bg->palette_mask = 7;
                    bg->num_planes = 2;
                    bg->tile_bytes = 0x20;
                    bg->tile_row_bytes = 4;
                    break;
                case SNES_PPU_BG::BPP8:
                    bg->palette_shift = 0; //
                    bg->palette_mask = 0xFF;
                    bg->num_planes = 4;
                    bg->tile_bytes = 0x40;
                    bg->tile_row_bytes = 8;
                    break;
                case SNES_PPU_BG::inactive:
                    break;
                case SNES_PPU_BG::mode7:
                    bg->palette_shift = 0;
                    bg->palette_mask = 0xFF;
                    bg->num_planes = 1;
                    break;
            }
        }
}

void SNES_PPU::write_VRAM(u32 addr, u32 val)
{
    addr &= 0x7FFF;
    DBG_EVENT(DBG_SNES_EVENT_WRITE_VRAM);
    dbgloglog(snes, SNES_CAT_PPU_VRAM_WRITE, DBGLS_INFO, "VRAM write %04x: %04x", addr, val);
    VRAM[addr] = val;
}

void SNES_PPU::write(u32 addr, u32 val, SNES_memmap_block *bl) {
    addr &= 0xFFFF;
    u32 addre;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_write(snes, addr, val); }
    switch (addr) {
        case 0x2100: // INIDISP
            io.force_blank = (val >> 7) & 1;
            io.screen_brightness = val & 15;
            return;
        case 0x2101: // OBSEL
            io.obj.sz = (val >> 5) & 7;
            io.obj.name_select = (val >> 3) & 3;
            io.obj.tile_addr = (val << 13) & 0x6000;
            return;
        case 0x2102: // OAMADDL
            io.oam.addr = (io.oam.addr & 0xFE00) | (val << 1);
            obj.first = io.oam.priority ? (io.oam.addr >> 2) & 0x7F : 0;
            return;
        case 0x2103: // OAMADDH
            io.oam.addr = (io.oam.addr & 0x1FE) | ((val & 1) << 9);
            io.oam.priority = (val >> 7) & 1;
            obj.first = io.oam.priority ? (io.oam.addr >> 2) & 0x7F : 0;
            return;
        case 0x2104: {// OAMDATA
            u32 latchbit = io.oam.addr & 1;
            addre = io.oam.addr;
            io.oam.addr = (io.oam.addr + 1) & 0x3FF;
            if (!latchbit) latch.oam = val;
            if (addre & 0x200) {
                write_oam(addre, val);
            } else if (latchbit) {
                write_oam(addre & 0xFFFE, latch.oam);
                write_oam((addre & 0xFFFE) + 1, val);
            }
            obj.first = io.oam.priority ? (io.oam.addr >> 2) & 0x7F : 0;
            return;
        }
        case 0x2105: { // BGMODE
            u32 old_bg_mode = io.bg_mode;
            io.bg_mode = val & 7;
            if (old_bg_mode != io.bg_mode) {
                printf("\nNEW BG MODE %d", io.bg_mode);
            }
            io.bg_priority = (val >> 4) & 1;
            pbg[0].io.tile_size = (val >> 4) & 1;
            pbg[1].io.tile_size = (val >> 5) & 1;
            pbg[2].io.tile_size = (val >> 6) & 1;
            pbg[3].io.tile_size = (val >> 7) & 1;
            update_video_mode();
            return;
        }
        case 0x2106: {// MOSAIC
            u32 latchbit = pbg[0].mosaic.enable | pbg[1].mosaic.enable | pbg[2].mosaic.enable |
                           pbg[3].mosaic.enable;
            /*bg[0].mosaic.enable = (val >> 0) & 1;
            pbg[1].mosaic.enable = (val >> 1) & 1;
            pbg[2].mosaic.enable = (val >> 2) & 1;
            pbg[3].mosaic.enable = (val >> 3) & 1;*/
            io.mosaic.size = ((val >> 4) & 15) + 1;
            if (!latchbit && (val & 15)) {
                mosaic.counter = io.mosaic.size + 1;
            }
            return;
        }
        case 0x2107: // BG1SC
        case 0x2108: // BG2SC
        case 0x2109: // BG3SC
        case 0x210A: { // BG4SC
            u32 bgnum = addr - 0x2107;
            SNES_PPU_BG *bg = &pbg[bgnum];
            bg->io.screen_size = val & 3;
            bg->io.screen_addr = (val << 8) & 0x7C00;
            update_video_mode();
            return;
        }
        case 0x210B: // BG12NBA
            pbg[0].io.tiledata_addr = (val << 12) & 0x7000;
            pbg[1].io.tiledata_addr = (val << 8) & 0x7000;
            return;
        case 0x210C: // BG34NBA
            pbg[2].io.tiledata_addr = (val << 12) & 0x7000;
            pbg[3].io.tiledata_addr = (val << 8) & 0x7000;
            return;
        case 0x210D: // BG1HOFS
            pbg[0].io.hoffset = (val << 8) | (latch.ppu1.bgofs & 0xF8) | (latch.ppu2.bgofs & 7);
            latch.ppu1.bgofs = latch.ppu2.bgofs = val;

            mode7.hoffset = (val << 8) | (latch.mode7);
            latch.mode7 = val;
            mode7.rhoffset = SIGNe13to32(mode7.hoffset);
            return;
        case 0x210E: // BG1VOFS
            pbg[0].io.voffset = (val << 8) | (latch.ppu1.bgofs);
            latch.ppu1.bgofs = val;

            mode7.voffset = (val << 8) | (latch.mode7);
            latch.mode7 = val;
            mode7.rvoffset = SIGNe13to32(mode7.voffset);
            return;
        case 0x210F: // BG2HOFS
            pbg[1].io.hoffset = (val << 8) | (latch.ppu1.bgofs & 0xF8) | (latch.ppu2.bgofs & 7);
            latch.ppu1.bgofs = latch.ppu2.bgofs = val;
            return;
        case 0x2110: // BG2VOFS
            pbg[1].io.voffset = (val << 8) | (latch.ppu1.bgofs);
            latch.ppu1.bgofs = val;
            return;
        case 0x2111: // BG3HOFS
            pbg[2].io.hoffset = (val << 8) | (latch.ppu1.bgofs & 0xF8) | (latch.ppu2.bgofs & 7);
            latch.ppu1.bgofs = latch.ppu2.bgofs = val;
            return;
        case 0x2112: // BG3VOFS
            pbg[2].io.voffset = (val << 8) | (latch.ppu1.bgofs);
            latch.ppu1.bgofs = val;
            return;
        case 0x2113: // BG4HOFS
            pbg[3].io.hoffset = (val << 8) | (latch.ppu1.bgofs & 0xF8) | (latch.ppu2.bgofs & 7);
            latch.ppu1.bgofs = latch.ppu2.bgofs = val;
            return;
        case 0x2114: // BG4VOFS
            pbg[3].io.voffset = (val << 8) | (latch.ppu1.bgofs);
            latch.ppu1.bgofs = val;
            return;
        case 0x2115: {// VRAM increment
            constexpr int steps[4] = {1, 32, 128, 128};
            io.vram.increment_step = steps[val & 3];
            io.vram.mapping = (val >> 2) & 3;
            io.vram.increment_mode = (val >> 7) & 1;
            return; }
        case 0x2116: // VRAM address lo
            io.vram.addr = (io.vram.addr & 0xFF00) | val;
            latch.vram = VRAM[get_addr_by_map()];
            return;
        case 0x2117: // VRAM address hi
            io.vram.addr = (val << 8) | (io.vram.addr & 0xFF);
            latch.vram = VRAM[get_addr_by_map()];
            return;
        case 0x2118: // VRAM data lo
            /*if (!snes->clock.ppu.vblank_active && !io.force_blank) {
                printf("\nDISCARD WRITE ON VBLANK OR FORCE BLANK");
                return;
            }*/
            addre = get_addr_by_map();
            write_VRAM(addre, (VRAM[addre] & 0xFF00) | (val & 0xFF));
            if (io.vram.increment_mode == 0) io.vram.addr = (io.vram.addr + io.vram.increment_step) & 0x7FFF;
            return;
        case 0x2119: // VRAM data hi
            //if (!snes->clock.ppu.vblank_active && !io.force_blank) return;
            addre = get_addr_by_map();
            write_VRAM(addre, (val << 8) | (VRAM[addre] & 0xFF));
            if (io.vram.increment_mode == 1) io.vram.addr = (io.vram.addr + io.vram.increment_step) & 0x7FFF;
            return;
        case 0x211A: // M7SEL
            mode7.hflip = val & 1;
            mode7.vflip = (val >> 1) & 1;
            mode7.repeat = (val >> 6) & 3;
            return;
        case 0x211B: // M7A
            mode7.a = static_cast<i16>((val << 8) | latch.mode7);
            latch.mode7 = val;
            return;
        case 0x211C: // M7B
            mode7.b = static_cast<i16>((val << 8) | latch.mode7);
            latch.mode7 = val;
            return;
        case 0x211D: // M7C
            mode7.c = static_cast<i16>((val << 8) | latch.mode7);
            latch.mode7 = val;
            return;
        case 0x211E: // M7D
            mode7.d = static_cast<i16>((val << 8) | latch.mode7);
            latch.mode7 = val;
            return;
        case 0x211F: // M7E
            mode7.x = (val << 8) | latch.mode7;
            mode7.rx = SIGNe13to32(mode7.x);
            latch.mode7 = val;
            return;
        case 0x2120: // M7F
            mode7.y = (val << 8) | latch.mode7;
            mode7.ry = SIGNe13to32(mode7.y);
            latch.mode7 = val;
            return;
        case 0x2121: // Color RAM address
            io.cgram_addr = val;
            latch.cgram_addr = 0;
            return;
        case 0x2122: // Color RAM data
            if (latch.cgram_addr == 0) {
                latch.cgram_addr = 1;
                latch.cgram = val;
            }
            else {
                latch.cgram_addr = 0;
                CGRAM[io.cgram_addr] = ((val & 0x7F) << 8) | latch.cgram;
                io.cgram_addr = (io.cgram_addr + 1) & 0xFF;
            }
            return;
        case 0x2123: // W12SEL
            pbg[0].window.invert[0] = val  & 1;
            pbg[0].window.enable[0] = (val >> 1) & 1;
            pbg[0].window.invert[1] = (val >> 2) & 1;
            pbg[0].window.enable[1] = (val >> 3) & 1;
            pbg[1].window.invert[0] = (val >> 4)  & 1;
            pbg[1].window.enable[0] = (val >> 5) & 1;
            pbg[1].window.invert[1] = (val >> 6) & 1;
            pbg[1].window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2124: // W34SEL
            pbg[2].window.invert[0] = val  & 1;
            pbg[2].window.enable[0] = (val >> 1) & 1;
            pbg[2].window.invert[1] = (val >> 2) & 1;
            pbg[2].window.enable[1] = (val >> 3) & 1;
            pbg[3].window.invert[0] = (val >> 4)  & 1;
            pbg[3].window.enable[0] = (val >> 5) & 1;
            pbg[3].window.invert[1] = (val >> 6) & 1;
            pbg[3].window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2125: // WOBJSEL
            obj.window.invert[0] = val & 1;
            obj.window.enable[0] = (val >> 1) & 1;
            obj.window.invert[1] = (val >> 2) & 1;
            obj.window.enable[1] = (val >> 3) & 1;
            color_math.window.invert[0] = (val >> 4) & 1;
            color_math.window.enable[0] = (val >> 5) & 1;
            color_math.window.invert[1] = (val >> 6) & 1;
            color_math.window.enable[1] = (val >> 7) & 1;
            return;
        case 0x2126: // WH0
            //if (dbg.log_windows) console.log('WINDOW ONE LEFT WRITE', val);
            //window.one_left = 0;
            window[0].left = val;
            return;
        case 0x2127: // WH1...
            //if (val == 128) debugger;
            //if (dbg.log_windows) console.log('WINDOW ONE RIGHT WRITE', val);
            window[0].right = val;
            //window.one_right = 256;
            return;
        case 0x2128: // WH2
            window[1].left = val;
            return;
        case 0x2129: // WH3
            window[1].right = val;
            return;
        case 0x212A: // WBGLOG
            pbg[0].window.mask = val & 3;
            pbg[1].window.mask = (val >> 2) & 3;
            pbg[2].window.mask = (val >> 4) & 3;
            pbg[3].window.mask = (val >> 6) & 3;
            return;
        case 0x212B: // WOBJLOG
            obj.window.mask = val & 3;
            color_math.window.mask = (val >> 2) & 3;
            return;
        case 0x212C: // TM
            pbg[0].main_enable = val & 1;
            pbg[1].main_enable = (val >> 1) & 1;
            pbg[2].main_enable = (val >> 2) & 1;
            pbg[3].main_enable = (val >> 3) & 1;
            obj.main_enable = (val >> 4) & 1;
            return;
        case 0x212D: // TS
            pbg[0].sub_enable = val & 1;
            pbg[1].sub_enable = (val >> 1) & 1;
            pbg[2].sub_enable = (val >> 2) & 1;
            pbg[3].sub_enable = (val >> 3) & 1;
            obj.sub_enable = (val >> 4) & 1;
            return;
        case 0x212E: // TMW
            pbg[0].window.main_enable = val & 1;
            pbg[1].window.main_enable = (val >> 1) & 1;
            pbg[2].window.main_enable = (val >> 2) & 1;
            pbg[3].window.main_enable = (val >> 3) & 1;
            obj.window.main_enable = (val >> 4) & 1;
            return;
        case 0x212F: // TSW
            pbg[0].window.sub_enable = val & 1;
            pbg[1].window.sub_enable = (val >> 1) & 1;
            pbg[2].window.sub_enable = (val >> 2) & 1;
            pbg[3].window.sub_enable = (val >> 3) & 1;
            obj.window.sub_enable = (val >> 4) & 1;
            return;
        case 0x2130: // CGWSEL
            color_math.direct_color = val & 1;
            color_math.blend_mode = (val >> 1) & 1;
            color_math.window.sub_mask = (val >> 4) & 3;
            color_math.window.main_mask = (val >> 6) & 3;
            return;
        case 0x2131: // CGADDSUB
            color_math.enable[PPU_src_BG1] = val & 1;
            color_math.enable[PPU_src_BG2] = (val >> 1) & 1;
            color_math.enable[PPU_src_BG3] = (val >> 2) & 1;
            color_math.enable[PPU_src_BG4] = (val >> 3) & 1;
            color_math.enable[PPU_src_OBJ1] = 0;
            color_math.enable[PPU_src_OBJ2] = (val >> 4) & 1;
            color_math.enable[PPU_src_COL] = (val >> 5) & 1;
            color_math.halve = (val >> 6) & 1;
            color_math.math_mode = (val >> 7) & 1;
            return;
        case 0x2132: // COLDATA weirdness
            if (val & 0x20) color_math.fixed_color = (color_math.fixed_color & 0x7FE0) | (val & 31);
            if (val & 0x40) color_math.fixed_color = (color_math.fixed_color & 0x7C1F) | ((val & 31) << 5);
            if (val & 0x80) color_math.fixed_color = (color_math.fixed_color & 0x3FF) | ((val & 31) << 10);
            DBG_EVENT(DBG_SNES_EVENT_WRITE_COLDATA);
            return;
        case 0x2133: // SETINI
            io.interlace = val & 1;
            io.obj.interlace = (val >> 1) & 1;
            io.overscan = (val >> 2) & 1;
            io.pseudo_hires = (val >> 3) & 1;
            io.extbg = (val >> 6) & 1;
            update_video_mode();
            return;

        case 0x2180: // WRAM access port
            SNES_wdc65816_write(snes, 0x7E0000 | io.wram_addr, val);
            io.wram_addr = (io.wram_addr + 1) & 0x1FFFF;
            return;
        case 0x2181: // WRAM addr low
            io.wram_addr = (io.wram_addr & 0x1FF00) + val;
            return;
        case 0x2182: // WRAM addr med
            io.wram_addr = (val << 8) + (io.wram_addr & 0x100FF);
            return;
        case 0x2183: // WRAM bank
            io.wram_addr = ((val & 1) << 16) | (io.wram_addr & 0xFFFF);
            return;
        case 0x2134:
            return;
        default:
    }
    printf("\nWARN SNES PPU WRITE NOT IN %04x %02x", addr, val);
    int num = 0;
    num++;
    if (num == 10) {
        //dbg_break("PPU WRITE TOOMANYBAD", snes->clock.master_cycle_count);
    }
}

u32 SNES_PPU::mode7_mul()
{
    return static_cast<u32>((mode7.a) * static_cast<i32>(static_cast<i8>(mode7.b >> 8)));
}

void SNES_PPU::latch_counters() {
    snes->ppu.io.vcounter = snes->clock.ppu.y;
    snes->ppu.io.hcounter = (snes->clock.master_cycle_count - snes->clock.ppu.scanline_start) >> 2;
    snes->ppu.latch.counters = 1;
}

u32 SNES_PPU::read(u32 addr, u32 old, u32 has_effect, SNES_memmap_block *bl)
{
    addr &= 0xFFFF;
    if (addr >= 0x2140 && addr < 0x217F) { return SNES_APU_read(snes, addr, old, has_effect); }
    u32 result;
    switch(addr) {
        case 0x2134: // MPYL
            result = mode7_mul();
            return result & 0xFF;
        case 0x2135: // MPYM
            result = mode7_mul();
            return (result >> 8) & 0xFF;
        case 0x2136: // MPYH
            result = mode7_mul();
            return (result >> 16) & 0xFF;
        case 0x2137: // SLHV?
            if (has_effect && (snes->r5a22.io.pio & 0x80)) latch_counters();
            return old;
        case 0x2138: {// OAMDATAREAD
            u32 data = read_oam(io.oam.addr);
            if (has_effect) {
                io.oam.addr = (io.oam.addr + 1) & 0x3FF;
                obj.first = io.oam.priority ? (io.oam.addr >> 2) & 0x7F : 0;
            }
            return data; }
        case 0x2139: // VMDATAREADL
            result = latch.vram & 0xFF;
            if (has_effect && io.vram.increment_mode == 0) {
                latch.vram = VRAM[get_addr_by_map()];
                io.vram.addr = (io.vram.addr + io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213A: // VMDATAREADH
            result = (latch.vram >> 8) & 0xFF;
            if (has_effect && (io.vram.increment_mode == 1)) {
                latch.vram = VRAM[get_addr_by_map()];
                io.vram.addr = (io.vram.addr + io.vram.increment_step) & 0x7FFF;
            }
            return result;
        case 0x213D: // OPVCT
            if (has_effect) {
                if (latch.vcounter == 0) {
                    latch.vcounter = 1;
                    latch.ppu2.mdr = snes->clock.ppu.y;
                } else {
                    latch.vcounter = 0;
                    latch.ppu2.mdr = (snes->clock.ppu.y >> 8) | (latch.ppu2.mdr & 0xFE);
                }
            }
            return latch.ppu2.mdr;
        case 0x213E: // STAT77
            if (has_effect)
                latch.ppu1.mdr = 1 | (obj.range_overflow << 6) | (obj.time_overflow << 7);
            return latch.ppu1.mdr;
        case 0x213F:
            if (has_effect) {
                latch.hcounter = 0;
                latch.vcounter = 0;
                latch.ppu2.mdr &= 32;
                latch.ppu2.mdr |= 0x03 | (snes->clock.ppu.field << 7);
                if (!(snes->r5a22.io.pio & 0x80)) {
                    latch.ppu2.mdr |= 1 << 6;
                } else {
                    latch.ppu2.mdr |= latch.counters << 6;
                    latch.counters = 0;
                }
            }
            return latch.ppu2.mdr;
        case 0x2180: {// WRAM access port
            u32 r = SNES_wdc65816_read(snes, 0x7E0000 | io.wram_addr, 0, has_effect);
            if (has_effect) {
                io.wram_addr++;
                if (io.wram_addr > 0x1FFFF) io.wram_addr = 0;
            }
            return r; }
        default:
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

void SNES_PPU::new_scanline(u64 cur_clock)
{
    // TODO: this
    // TODO: hblank exit here too
    snes->clock.ppu.scanline_start = cur_clock;
    snes->clock.ppu.y++;
    if (snes->dbg.events.view.vec) {
        debugger_report_line(snes->dbg.interface, snes->clock.ppu.y);
    }
    snes->r5a22.status.hirq_line = 0;
}

void dram_refresh(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    scheduler_from_event_adjust_master_clock(&snes->scheduler, 40);
}

struct slice {
    SNES_PPU_px px[16];
    u32 priority;
};

static inline u32 direct_color(u32 palette_index, u32 palette_color)
{
    return ((palette_color << 2) & 0x001C) + ((palette_index << 1) & 2) +
           ((palette_color << 4) & 0x380) + ((palette_index << 5) & 0x40) +
           ((palette_color << 7) & 0x6000) + ((palette_index << 10) & 0x1000);
}

static inline void setpx(SNES_PPU_px *px, u32 source, u32 priority, u32 color, u32 bpp, u32 dbg_priority)
{
    px->has = 1;
    px->source = source;
    px->priority = priority;
    px->color = color;
    px->bpp = bpp;
    px->dbg_priority = dbg_priority;
}

void SNES_PPU::draw_bg_line_mode7(u32 source, i32 y)
{
    SNES_PPU_BG *mbg = &pbg[source];

    y = mode7.vflip ? 255 - y : y;
    u32 dbp;

    u16 m_color = 0;
    u32 m_counter = 1;
    u32 m_palette = 0;
    u8  m_priority = 0;

    i32 hohc = (mode7.rhoffset - mode7.rx);
    i32 vovc = (mode7.rvoffset - mode7.ry);

    // sign extend hohc and vovc
    hohc = (hohc & 0x2000) ? (hohc | ~1023) : (hohc & 1023);
    vovc = (vovc & 0x2000) ? (vovc | ~1023) : (vovc & 1023);

    i32 origin_x = (mode7.a * hohc & ~63) + (mode7.b * vovc & ~63) + (mode7.b * y & ~63) + (mode7.rx << 8);
    i32 origin_y = (mode7.c * hohc & ~63) + (mode7.d * vovc & ~63) + (mode7.d * y & ~63) + (mode7.ry << 8);

    for (i32 sx = 0; sx < 256; sx++) {
        i32 x = mode7.hflip ? 255 - sx : sx;
        i32 pixel_x = (origin_x + mode7.a * x) >> 8;
        i32 pixel_y = (origin_y + mode7.c * x) >> 8;
        i32 tile_x = (pixel_x >> 3) & 127;
        i32 tile_y = (pixel_y >> 3) & 127;
        i32 out_of_bounds = (pixel_x | pixel_y) & ~1023;
        i32 tile_addr = (tile_y << 7) | tile_x;
        i32 pal_addr = ((pixel_y & 7) << 3) + (pixel_x & 7);
        u32 tile = (((mode7.repeat == 3) && out_of_bounds) ? 0 : VRAM[tile_addr & 0x7FFF]) & 0xFF;
        if ((sx == 12) && (y == 96)) {
            printf("\npx:%d  py:%d  tile_addr:%04x  tile_val:%02x", pixel_x, pixel_y, tile_addr, tile);
        }
        u32 palette = (((mode7.repeat == 2) && out_of_bounds) ? 0 : VRAM[(tile << 6 | pal_addr) & 0x7FFF] >> 8) & 0xFF;

        u32 priority;
        if (source == 0) {
            priority = mbg->priority[0];
            dbp = 0;
        }
        else {
            priority = mbg->priority[palette >> 7];
            dbp = palette >> 7;
            palette &= 0x7F;
        }

        if (--m_counter == 0) {
            m_counter = mbg->mosaic.enable ? mbg->mosaic.size : 1;
            m_palette = palette;
            m_priority = priority;
            if (color_math.direct_color && source == 0) {
                m_color = direct_color(0, palette);
            }
            else {
                m_color = CGRAM[palette];
            }
        }

        if (!m_palette) continue;
        setpx(&mbg->line[sx], source, m_priority, m_color, BPP_8, dbp);
    }
}

u32 SNES_PPU::get_tile(SNES_PPU_BG *bg, i32 hoffset, i32 voffset)
{
    const u32 hires = io.bg_mode == 5 || io.bg_mode == 6;
    const u32 tile_height = 3 + bg->io.tile_size;
    const u32 tile_width = !hires ? tile_height : 4;
    const u32 screen_x = bg->io.screen_size & 1 ? 0x400 : 0;
    const u32 screen_y = bg->io.screen_size & 2 ? 32 << (5 + (bg->io.screen_size & 1)) : 0;
    const u32 tile_x = hoffset >> tile_width;
    const u32 tile_y = voffset >> tile_height;
    u32 offset = ((tile_y & 0x1F) << 5) | (tile_x & 0x1F);
    if (tile_x & 0x20) offset += screen_x;
    if (tile_y & 0x20) offset += screen_y;
    const u32 addr = (bg->io.screen_addr + offset) & 0x7FFF;
    return VRAM[addr];
}

void SNES_PPU::draw_bg_line(u32 source, u32 y)
{
    SNES_PPU_BG *bg = &pbg[source];
    memset(bg->line, 0, sizeof(bg->line));
    if ((bg->tile_mode == SNES_PPU_BG::inactive) || (!bg->main_enable && !bg->sub_enable)) {
        return;
    }
    if (bg->tile_mode == SNES_PPU_BG::mode7) {
        return draw_bg_line_mode7(source, y);
    }
    u32 bpp = BPP_4;
    if (bg->tile_mode == SNES_PPU_BG::BPP2) bpp = BPP_2;
    if (bg->tile_mode == SNES_PPU_BG::BPP8) bpp = BPP_8;

    u32 hires = io.bg_mode == 5 || io.bg_mode == 6;
    u32 offset_per_tile_mode = io.bg_mode == 2 || io.bg_mode == 4 || io.bg_mode == 6;

    u32 direct_color_mode = color_math.direct_color && source == 0 && (io.bg_mode == 3 || io.bg_mode == 4);
    u32 color_shift = 3 + bg->tile_mode;
    i32 width = 256 << hires;
    //console.log('RENDERING PPU y:', y, 'BG MODE', io.bg_mode);

    u32 tile_height = 3 + bg->io.tile_size;
    u32 tile_width = hires ? 4 : tile_height;
    u32 tile_mask = 0xFFF >> bg->tile_mode;
    bg->tiledata_index = bg->io.tiledata_addr >> (3 + bg->tile_mode);

    bg->palette_base = io.bg_mode == 0 ? source << 5 : 0;
    bg->palette_shift = 2 << bg->tile_mode;

    u32 hscroll = bg->io.hoffset;
    u32 vscroll = bg->io.voffset;
    u32 hmask = (width << bg->io.tile_size << (bg->io.screen_size & 1)) - 1;
    u32 vmask = (width << bg->io.tile_size << ((bg->io.screen_size & 2) >> 1)) - 1;
    //if (snes->clock.ppu.y == 40 && source == 0) printf("\nHSCROLL:%d VSCROLL:%d", hscroll, vscroll);

    if (hires) {
        hscroll <<= 1;
        // TODO: this?
        //if (io.interlace) y = y << 1 | +(cache.control.field && !bg.mosaic_enable);
    }
    if (bg->mosaic.enable) {
        //printf("\nWHAT ENABLED?");
        y -= (io.mosaic.size - bg->mosaic.counter) << (hires && io.interlace);
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
                i32 hlookup = get_tile(&pbg[2], (offset_x - 8) + (pbg[2].io.hoffset & 0xFFF8), pbg[2].io.voffset);
                if (io.bg_mode == 4) {
                    if (hlookup & valid_bit) {
                        if (!(hlookup & 0x8000)) {
                            hoffset = offset_x + (hlookup & 0xFFF8);
                        } else {
                            voffset = y + hlookup;
                        }
                    }
                } else {
                    i32 vlookup = get_tile( &pbg[2], (offset_x - 8) + (pbg[2].io.hoffset & 0xFFF8), pbg[2].io.voffset + 8);
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

        u32 tile_number = get_tile(bg, hoffset, voffset);

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

        u32 datalo = (VRAM[(address + 8) & 0x7FFF] << 16) | (VRAM[address]);
        u32 datahi = (VRAM[(address + 24) & 0x7FFF] << 16) | (VRAM[(address + 16) & 0x7FFF]);
        u32 datamid = ((datalo >> 16) & 0xFFFF) | ((datahi << 16) & 0xFFFF0000); // upper 16 bits of data lo or lower 16 bits of data high
        for (i32 tile_x = 0; tile_x < 8; tile_x++, x++) {
            if (x < 0 || x >= width) continue; // x < 0 || x >= width
            if (--mosaic_counter == 0) {
                u32 color;
                i32 shift = mirror_x ? tile_x : 7 - tile_x;
                {
                    color = (datalo >> shift) & 1;
                    color += (datalo >> (shift + 7)) & 2; // 0-2 + 7-9
                }
                if (bg->tile_mode >= SNES_PPU_BG::BPP4) {
                    color += (datalo >> (shift + 14)) & 4; // bits 16-24
                    color += (datalo >> (shift + 21)) & 8; // bits 24-31
                }
                if (bg->tile_mode >= SNES_PPU_BG::BPP8) {
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
                    mosaic_color = CGRAM[palette_index + mosaic_palette];
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

void SNES_PPU::do_color_math(SNES_PPU_px *main, SNES_PPU_px *sub)
{
    main->color_math_flags = 1;
    i32 mr = main->color & 0x1F;
    i32 mg = (main->color >> 5) & 0x1F;
    i32 mb = (main->color >> 10) & 0x1F;
    i32 sr = sub->color & 0x1F;
    i32 sg = (sub->color >> 5) & 0x1F;
    i32 sb = (sub->color >> 10) & 0x1F;

    if (color_math.math_mode) {
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
    if (color_math.halve) {
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

void SNES_PPU::draw_sprite_line(i32 ppu_y)
{
    memset(obj.line, 0, sizeof(obj.line));

    if (!obj.main_enable && !obj.sub_enable) return;
    const i32 spwidth[8][2] = { // [io.obj.sz][sprite_sz] }
            {8, 16},
            {8, 32},
            {8, 64},
            {16, 32},
            {16, 64},
            {32, 64},
            {16, 32},
            {16, 32}
    };
    const i32 spheight[8][2] = {
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
        u32 sn = (mbn + obj.first) & 0x7F;

        SNES_PPU_sprite *sp = &obj.items[sn];

        i32 height = spheight[io.obj.sz][sp->size];
        i32 width = spwidth[io.obj.sz][sp->size];

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

        u32 tile_addr = io.obj.tile_addr + sp->name_select_add;
        u32 character_x = sp->tile_num & 15;
        u32 character_y = (((sp->tile_num >> 4) + (sp_line >> 3)) & 15) << 4;

        for (u32 n = 0; n < num_h_tiles; n++) {
            if (tile_limit < 1) return;
            u32 block_x = n ^ tile_xxor;
            u32 addr = tile_addr + ((character_y + (character_x + block_x & 15)) << 4);
            addr = (addr & 0x7FF0) + tile_y;
            u32 data = VRAM[addr] | (VRAM[(addr + 8) & 0x7FFF]) << 16;
            for (u32 pxn = 0; pxn < 8; pxn++) {
                i32 rx = sp->x + in_sp_x;
                if ((rx >= 0) && (rx < 256)) {
                    u32 mpx = sp->hflip ? pxn : 7 - pxn;
                    u32 color = (data >> mpx) & 1;
                    color += (data >> (mpx + 7)) & 2;
                    color += (data >> (mpx + 14)) & 4;
                    color += (data >> (mpx + 21)) & 8;
                    if (color != 0) {
                        SNES_PPU_px *px = &obj.line[rx];
                        px->has = 1;
                        px->source = 4; // OBJ = 4
                        px->priority = obj.priority[sp->priority];
                        px->dbg_priority = sp->priority;
                        px->color = CGRAM[sp->pal_offset + color];
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

void SNES_PPU::draw_line()
{
    // Draw sprite line
    //printf("\nDraw line %d", snes->clock.ppu.y);
    if (snes->clock.ppu.y == 0) memset(&snes->dbg_info.line, 0, sizeof(snes->dbg_info.line));
    draw_sprite_line(snes->clock.ppu.y);
    draw_bg_line(0, snes->clock.ppu.y);
    draw_bg_line(1, snes->clock.ppu.y);
    draw_bg_line(2, snes->clock.ppu.y);
    draw_bg_line(3, snes->clock.ppu.y);
    for (u32 i = 0; i < 4; i++) {
        memcpy(snes->dbg_info.line[snes->clock.ppu.y].bg[i].px, pbg[i].line, sizeof(pbg[i].line));
    }
    memcpy(snes->dbg_info.line[snes->clock.ppu.y].sprite_px, obj.line, sizeof(obj.line));
    u16 *line_output = cur_output + (snes->clock.ppu.y * 512);
    for (u32 x = 0; x < 256; x++) {
        SNES_PPU_px main_px = {.source=SRC_BACK};
        SNES_PPU_px sub_px = {.source=SRC_BACK};
        sub_px.priority = 15;
        sub_px.color = color_math.fixed_color;
        sub_px.has = 0;
        main_px.priority = 15;
        main_px.color = CGRAM[0];
        main_px.has = 0;
#define MAINPRIO 0
#define SUBPRIO 1
        for (u32 bgnum = 0; bgnum < 4; bgnum++) {
            SNES_PPU_BG *bg = &pbg[bgnum];
            SNES_PPU_px *cmp = &bg->line[x];
            if (bg->main_enable && cmp->has && (main_px.priority > cmp->priority)) {
                main_px.color = cmp->color;
                main_px.has = 1;
                main_px.priority = cmp->priority;
                main_px.source = bgnum;
                main_px.dbg_priority = bg->priority[MAINPRIO];
            }
            if (color_math.blend_mode && bg->sub_enable && cmp->has && (sub_px.priority > cmp->priority)) {
                sub_px.color = cmp->color;
                sub_px.has = 1;
                sub_px.priority = cmp->priority;
                sub_px.source = bgnum;
            }
        }
        SNES_PPU_px *cmp = &obj.line[x];
        if (obj.main_enable && cmp->has && (main_px.priority > cmp->priority)) {
            main_px.color = cmp->color;
            main_px.has = 1;
            main_px.priority = cmp->priority;
            main_px.source = cmp->palette < 4 ? SRC_OBJ_PAL03 : SRC_OBJ_PAL47;
        }
        if (color_math.blend_mode && obj.sub_enable && cmp->has && (sub_px.priority > cmp->priority)) {
            sub_px.color = cmp->color;
            sub_px.has = 1;
            sub_px.priority = cmp->priority;
            sub_px.source = cmp->palette < 4 ? SRC_OBJ_PAL03 : SRC_OBJ_PAL47;
        }

        main_px.color_math_flags = 0;
        if (color_math.enable[main_px.source]) do_color_math(&main_px, &sub_px);

        u16 c = main_px.color;
        // io.screen_brightness
        u32 b = (c >> 10) & 0x1F;
        u32 g = (c >> 5) & 0x1F;
        u32 r = c & 0x1F;
        b *= io.screen_brightness;
        g *= io.screen_brightness;
        r *= io.screen_brightness;
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


void hblank(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    snes->clock.ppu.hblank_active = key;

    snes->r5a22.hblank(key);
    // Draw layers and display

    if (key == 0) {
        // Draw bg
        // Output bg &  sprite
        if (snes->clock.ppu.y < 224)
            snes->ppu.draw_line();
    }
    else { // HBLANK before end of line.
    }

    // Do hblank DMA? no this is scheduled

}



void hdma_setup(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    u32 cn = 8;
    for (u32 n = 0; n < 8; n++) {
        cn += snes->r5a22.dma.channels[n].hdma_setup_ch();
    }
    scheduler_from_event_adjust_master_clock(&snes->scheduler, cn);
}

void hdma(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    if (snes->r5a22.hdma_is_enabled()) {
        snes->r5a22.status.dma_running = 1;
    }
}

void assert_hirq(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    R5A22 *th = &snes->r5a22;
    th->status.hirq_line = key;
    snes->r5a22.update_irq();
    if (key) DBG_EVENT(DBG_SNES_EVENT_HIRQ);
}

static void schedule_scanline(void *ptr, u64 key, u64 clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    u64 cur = clock - jitter;
    u32 vblank = snes->clock.ppu.y >= 224;
    snes->ppu.new_scanline(cur);

    // hblank DMA setup, 12-20ish
    if (!vblank) {
        snes->clock.timing.line.hdma_setup_position = snes->clock.rev == 1 ? 12 + 8 - (snes->clock.master_cycle_count & 7) : 12 + (snes->clock.master_cycle_count & 7);
        scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hdma_setup_position, 0, snes, &hdma_setup, nullptr);
    }

    // hblank out - 108 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hblank_stop, 0, snes, &hblank, nullptr);

    // DRAM refresh - ~510 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.dram_refresh, 0, snes, &dram_refresh, nullptr);


    // HIRQ
    if (snes->r5a22.io.irq_enable) {
        if (!snes->r5a22.io.hirq_enable) assert_hirq(snes, 1, cur, 0);
        else
            snes->ppu.hirq.sched_id = scheduler_only_add_abs(&snes->scheduler, cur + ((snes->r5a22.io.htime + 21) * 4), 1, snes, &assert_hirq, &snes->ppu.hirq.still_sched);
    }
    snes->r5a22.update_irq();

    // HDMA - 1104 ish
    if (!vblank) {
        scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hdma_position, 0, snes, &hdma, nullptr);
    }

    // Hblank in - 1108 ish
    scheduler_only_add_abs(&snes->scheduler, cur + snes->clock.timing.line.hblank_start, 1, snes, &hblank, nullptr);

    // new line - 1364
    scheduler_only_add_abs_w_tag(&snes->scheduler, cur + snes->clock.timing.line.master_cycles, 0, snes, &schedule_scanline, nullptr, 1);
}

void vblank(void *ptr, u64 key, u64 cur_clock, u32 jitter)
{
    SNES *snes = static_cast<SNES *>(ptr);
    snes->clock.ppu.vblank_active = key;
    snes->r5a22.status.nmi_flag = key;
    if (!key) { // VBLANK off at start of frame
        //printf("\nNMI OFF");
        snes->ppu.obj.range_overflow = snes->ppu.obj.time_overflow = 0;
    }
    else { // VBLANK on partway through frame
        //printf("\nNMI ON");
    }
    snes->r5a22.update_nmi();
}

void new_frame(void* ptr, u64 key, u64 cur_clock, u32 jitter)
{
    u64 cur = cur_clock - jitter;
    //printf("\nNEW FRAME @%lld", cur);
    SNES* snes = static_cast<SNES *>(ptr);

    if (snes->dbg.events.view.vec) {
        debugger_report_frame(snes->dbg.interface);
    }
    snes->ppu.cur_output = static_cast<u16 *>(snes->ppu.display->output[snes->ppu.display->last_written ^ 1]);
    snes->clock.master_frame++;

    snes->clock.ppu.field ^= 1;
    snes->clock.ppu.y = -1;

    /*snes->clock.ppu.scanline_start = cur_clock;
    if (snes->dbg.events.view.vec) {
        debugger_report_line(snes->dbg.interface, 0);
    }*/

    snes->clock.ppu.vblank_active = 0;
    snes->ppu.cur_output = static_cast<u16 *>(snes->ppu.display->output[snes->ppu.display->last_written]);
    memset(snes->ppu.cur_output, 0, 256*224*2);
    snes->ppu.display->last_written ^= 1;

    snes->ppu.display->scan_y = 0;
    snes->ppu.display->scan_x = 0;

    // vblank 1->0
    vblank(snes, 0, cur_clock, jitter);

    //set_clock_divisor();
    scheduler_only_add_abs(&snes->scheduler, cur + (snes->clock.timing.line.master_cycles * 225), 1, snes, &vblank, nullptr);
    scheduler_only_add_abs_w_tag(&snes->scheduler, cur + snes->clock.timing.frame.master_cycles, 0, snes, &new_frame, nullptr, 2);
}

void SNES_PPU::schedule_first()
{
    schedule_scanline(this, 0, 0, 0);
    snes->scheduler.only_add_abs_w_tag(snes->clock.timing.frame.master_cycles, 0, this, &new_frame, nullptr, 2);
}