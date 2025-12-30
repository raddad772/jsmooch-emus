//
// Created by . on 12/4/24.
//

#include <cstring>
#include <cassert>

#include "helpers/color.h"
#include "gba.h"
#include "gba_bus.h"
#include "gba_debugger.h"
#include "gba_timers.h"

namespace GBA {

#define MASTER_CYCLES_PER_SCANLINE 1232
#define HBLANK_CYCLES 226
#define MASTER_CYCLES_BEFORE_HBLANK (MASTER_CYCLES_PER_SCANLINE - HBLANK_CYCLES)
#define MASTER_CYCLES_PER_FRAME (228 * MASTER_CYCLES_PER_SCANLINE)
#define MASTER_CYCLES_PER_SECOND (MASTER_CYCLES_PER_FRAME * 60)
#define SCANLINE_HBLANK 1006


static void create_and_bind_registers_ARM7TDMI(core* th, disassembly_view *dv)
{
    u32 tkindex = 0;
    /*cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "B");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "C");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "E");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "F");
    rg->kind = RK_int32;
    rg->index = tkindex++;
    rg->custom_render = &render_f_z80;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SP");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IX");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IY");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "EI");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HALT");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "AF_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "BC_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "DE_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "CxI");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

#define BIND(dn, index) th->dbg.dasm_z80. dn = cvec_get(&dv->cpu.regs, index)
    BIND(A, 0);
    BIND(B, 1);
    BIND(C, 2);
    BIND(D, 3);
    BIND(E, 4);
    BIND(F, 5);
    BIND(HL, 6);
    BIND(PC, 7);
    BIND(SP, 8);
    BIND(IX, 9);
    BIND(IY, 10);
    BIND(EI, 11);
    BIND(HALT, 12);
    BIND(AF_, 13);
    BIND(BC_, 14);
    BIND(DE_, 15);
    BIND(HL_, 16);
    BIND(CE, 17);
#undef BIND*/
}

static void fill_disassembly_view(void *gbaptr, disassembly_view &dview)
{
    auto *th = static_cast<core *>(gbaptr);
    /*for (u32 i = 0; i < 8; i++) {
        th->dbg.dasm_m68k.D[i]->int32_data = th->m68k.regs.D[i];
        if (i < 7) th->dbg.dasm_m68k.A[i]->int32_data = th->m68k.regs.A[i];
    }
    th->dbg.dasm_m68k.PC->int32_data = th->m68k.regs.PC;
    th->dbg.dasm_m68k.SR->int32_data = th->m68k.regs.SR.u;
    if (th->dbg.dasm_m68k.SR->int32_data & 0x2000) { // supervisor mode
        th->dbg.dasm_m68k.USP->int32_data = th->m68k.regs.ASP;
        th->dbg.dasm_m68k.SSP->int32_data = th->m68k.regs.A[7];
    }
    else { // user mode
        th->dbg.dasm_m68k.USP->int32_data = th->m68k.regs.A[7];
        th->dbg.dasm_m68k.SSP->int32_data = th->m68k.regs.ASP;
    }
    th->dbg.dasm_m68k.supervisor->bool_data = th->m68k.regs.SR.S;
    th->dbg.dasm_m68k.trace->bool_data = th->m68k.regs.SR.T;
    th->dbg.dasm_m68k.IMASK->int32_data = th->m68k.regs.SR.I;
    th->dbg.dasm_m68k.CSR->int32_data = th->m68k.regs.SR.u & 0x1F;
    th->dbg.dasm_m68k.IR->int32_data = th->m68k.regs.IR;
    th->dbg.dasm_m68k.IRC->int32_data = th->m68k.regs.IRC;*/
}

static void get_obj_tile_size(u8 sz, u32 shape, u32 *htiles, u32 *vtiles)
{
#define T(s1, s2, hn, vn) case (((s1) << 2) | (s2)): *htiles = hn; *vtiles = vn; return;
    switch((sz << 2) | shape) {
        T(0, 0, 1, 1)
        T(0, 1, 2, 1)
        T(0, 2, 1, 2)
        T(1, 0, 2, 2)
        T(1, 1, 4, 1)
        T(1, 2, 1, 4)
        T(2, 0, 4, 4)
        T(2, 1, 4, 2)
        T(2, 2, 2, 4)
        T(3, 0, 8, 8)
        T(3, 1, 8, 4)
        T(3, 2, 4, 8)
        default: NOGOHERE;
    }
    printf("\nHEY! INVALID SHAPE %d", shape);
    *htiles = 1;
    *vtiles = 1;
#undef T
}

static void render_image_view_sys_info(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    auto *tb = &dview->options.at(0).textbox;
    tb->clear();
#define spf(...) tb->sprintf(__VA_ARGS__)
    for (u32 i = 0; i < 4; i++) {
        auto *t = &th->timer[i];
        spf("\nTimer:%d  enabled:%d  shift:%d  reload:%04x  cur:%04x", i, th->timer[i].enabled(), t->shift, t->reload, th->timer[i].read());
        u32 fifo=0;
        if (th->apu.fifo[0].timer_id == i) { fifo = 1; spf("  sound_fifo_A"); }
        if (th->apu.fifo[1].timer_id == i) { fifo = 1; spf("  sound_fifo_B"); }
        if (fifo) {
            u32 freq = MASTER_CYCLES_PER_SECOND;
            const u32 r = t->reload_ticks;
            freq /= r;
            spf("  calc_freq:%dhz  ", freq);
        }
    }
#undef spf
}

static void render_image_view_sprites(debugger_interface *dbgr, debugger_view *dview, void *my_ptr, u32 out_width) {
    auto *th = static_cast<core *>(my_ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 160);

    // Draw sprites!!!
    auto *cb_draw_transparency = &dview->options.at(0).checkbox;
    auto *cb_draw_4bpp = &dview->options.at(1).checkbox;
    auto *cb_draw_8bpp = &dview->options.at(2).checkbox;
    auto *cb_draw_normal = &dview->options.at(3).checkbox;
    auto *cb_draw_affine = &dview->options.at(4).checkbox;
    auto *cb_highlight_4bpp = &dview->options.at(5).checkbox;
    auto *cb_highlight_8bpp = &dview->options.at(6).checkbox;
    auto *cb_highlight_normal = &dview->options.at(7).checkbox;
    auto *cb_highlight_affine = &dview->options.at(8).checkbox;
    auto *cb_highlight_window = &dview->options.at(9).checkbox;
    auto *cb_wb_4bpp = &dview->options.at(10).checkbox;
    auto *cb_wb_8bpp = &dview->options.at(11).checkbox;
    auto *cb_wb_normal = &dview->options.at(12).checkbox;
    auto *cb_wb_affine = &dview->options.at(13).checkbox;
    auto *cb_wb_window = &dview->options.at(14).checkbox;
    u32 draw_transparency = cb_draw_transparency->value;
    u32 draw_4bpp = cb_draw_4bpp->value;
    u32 draw_8bpp = cb_draw_8bpp->value;
    u32 draw_normal = cb_draw_normal->value;
    u32 draw_affine = cb_draw_affine->value;
    u32 highlight_4bpp = cb_highlight_4bpp->value;
    u32 highlight_8bpp = cb_highlight_8bpp->value;
    u32 highlight_normal = cb_highlight_normal->value;
    u32 highlight_affine = cb_highlight_affine->value;
    u32 highlight_window = cb_highlight_window->value;
    u32 wb_4bpp = cb_wb_4bpp->value;
    u32 wb_8bpp = cb_wb_8bpp->value;
    u32 wb_normal = cb_wb_normal->value;
    u32 wb_affine = cb_wb_affine->value;
    u32 wb_window = cb_wb_window->value;

    for (u32 i = 0; i < 128; i++) {
        u32 sn = 127 - i;
        u16 *ptr = ((u16 *)th->ppu.OAM) + (sn * 4);
        u32 x = ptr[1] & 0x1FF;
        x = SIGNe9to32(x);
        u32 affine = (ptr[0] >> 8) & 1;
        if (affine && !draw_affine) continue;
        if (!affine && !draw_normal) continue;
        u32 obj_disable = (ptr[0] >> 9) & 1;
        if (!affine && obj_disable) continue;
        u32 shape = (ptr[0] >> 14) & 3; // 0 = square, 1 = horizontal, 2 = vertical
        u32 sz = (ptr[1] >> 14) & 3;
        u32 hflip = (ptr[1] >> 12) & 1;
        u32 vflip = (ptr[1] >> 13) & 1;
        u32 htiles, vtiles;
        get_obj_tile_size(sz, shape, &htiles, &vtiles);
        i32 tex_x_tiles = static_cast<i32>(htiles);
        i32 tex_x_pixels = static_cast<i32>(htiles * 8);
        i32 tex_y_pixels = static_cast<i32>(vtiles * 8);
        u32 dpix = ((ptr[0] >> 9) & 1);
        if (affine && dpix) {
            htiles *= 2;
            vtiles *= 2;
        }
        u32 hpixels = htiles * 8;
        u32 vpixels = vtiles * 8;

        i32 y_min = ptr[0] & 0xFF;
        i32 y_max = (y_min + (i32)vpixels) & 255;
        if(y_max < y_min)
            y_min -= 256;
        //if ((y_min > 160) || (y_max < 0)) continue;

        u32 mode = (ptr[0] >> 10) & 3;
        u32 mosaic = (ptr[0] >> 12) & 1;
         bool bpp8 = (ptr[0] >> 13) & 1;
        if (bpp8 && !draw_8bpp) continue;
        if (!bpp8 && !draw_4bpp) continue;
        u32 palette = bpp8 ? 0 : ((ptr[2] >> 12) & 15);
        //if (y_min!=-1) printf("\ndbgSPRITE%d y:%d PALETTE:%d", sn, y_min, palette);
        u32 bytes_per_line = bpp8 ? 8 : 4;
        i32 screen_x = x;
        i32 screen_y = y_min;

        // Setup stuff for non-affine....
        i32 pa = 1 << 8, pb = 0, pc = 0, pd = 1 << 8;
        i32 tx, ty;

        i32 half_x = 0 - ((i32)hpixels >> 1);
        i32 half_y = 0 - ((i32)vpixels >> 1);

        // Setup for affine...
        if (affine) {
            u32 pgroup = (ptr[1] >> 9) & 31;
            u32 pbase = (pgroup * 0x20) >> 1;
            u16 *pbase_ptr = ((u16 *)th->ppu.OAM) + pbase;
            pa = SIGNe16to32(pbase_ptr[3]);
            pb = SIGNe16to32(pbase_ptr[7]);
            pc = SIGNe16to32(pbase_ptr[11]);
            pd = SIGNe16to32(pbase_ptr[15]);
            tx = (pa*half_x + pb*half_y);
            ty = (pc*half_x + pd*half_y);
        }
        else {
            tx = half_x << 8;
            ty = half_y << 8;
            if (hflip) {
                tx = (i32)(hpixels<<7) - (1 << 8);
                pa = (i32)0xFFFFFF00;
            }
            if (vflip) {
                ty = (i32)(vpixels<<7) - (1 << 8);
                pd = (i32)0xFFFFFF00;
            }
        }
        i32 screen_x_right = screen_x + (i32)hpixels;
        i32 screen_y_bottom = screen_y + (i32)vpixels;

        for (i32 draw_y = screen_y; draw_y < screen_y_bottom; draw_y++) {
            u32 *rowptr = outbuf + (draw_y * out_width);
            i32 fx = tx;
            i32 fy = ty;
            for (i32 draw_x = screen_x; draw_x < screen_x_right; draw_x++) {
                u32 th_px_highlight = 0;
                u32 has = 0;
                u32 win_pixel = 0;
                th_px_highlight = 0;
                th_px_highlight |= bpp8 && wb_8bpp;
                th_px_highlight |= (!bpp8) && wb_4bpp;
                th_px_highlight |= wb_normal && !affine;
                th_px_highlight |= wb_affine && affine;
                th_px_highlight |= wb_window && (mode == 2);

                // Get pixel at coord fx, fy
                i32 tex_x = (fx >> 8) + ((i32)tex_x_pixels >> 1);
                i32 tex_y = (fy >> 8) + ((i32)tex_y_pixels >> 1);

                fx += pa;
                fy += pc;

                if ((draw_x >= 0) && (draw_x < 240) && (draw_y >= 0) && (draw_y < 140)) {
                    u32 color;
                    if (!th_px_highlight) {
                        // Sample the texture
                        if ((tex_x >= 0) && (tex_x < tex_x_pixels) && (tex_y >= 0) && (tex_y < tex_y_pixels)) {
                            u32 block_x = tex_x >> 3;
                            u32 block_y = tex_y >> 3;
                            u32 tile_x = tex_x & 7;
                            u32 tile_y = tex_y & 7;
                            if (!affine) {
                                if (hflip) {
                                    block_x = htiles - block_x;
                                    tile_x = 7 - tile_x;
                                }
                                if (vflip) {
                                    block_y = vtiles - block_y;
                                    tile_y = 7 - tile_y;
                                }
                            }

                            // We now have a precise pixel in the block.
                            // Now we must figure out block number based on mapping
                            // 1d mapping=1 means its htiles*y
                            // 1d mapping=0 means its 32 * y
                            u32 tile_num = ptr[2] & 0x3FF; // Get tile number

                            // Now advance by block y
                            if (th->ppu.io.obj_mapping_1d) {
                                tile_num += ((tex_x_tiles << bpp8) * block_y);
                            }
                            else {
                                tile_num += (32 * block_y);
                            }

                            // Advance by block x
                            tile_num += block_x << bpp8;

                            if (bpp8) tile_num &= 0x3FE;
                            tile_num &= 0x3FF;

                            // Now get pointer to that tile
                            u32 tile_base_addr = 0x10000 + (32 * tile_num);
                            u32 tile_line_addr = tile_base_addr + (bytes_per_line * tile_y);

                            // Now get data
                            u32 tile_px_addr = bpp8 ? (tile_line_addr + tile_x) : (tile_line_addr + (tile_x >> 1));
                            u8 data = th->ppu.VRAM[tile_px_addr];
                            if (!bpp8) {
                                if (tile_x & 1) data >>= 4;
                                data &= 15;
                            }
                            if ((data == 0) && draw_transparency) continue;
                            if (bpp8) {
                                color = gba_to_screen(th->ppu.palette_RAM[0x100 + data]);
                            }
                            else {
                                color = gba_to_screen(th->ppu.palette_RAM[0x100 + (palette << 4) + data]);
                            }
                            if ((mode == 2) && !highlight_window) continue;
                            if ((mode == 2) && highlight_window) color = 0xFFFFFFFF;
                            else {
                                th_px_highlight |= bpp8 && highlight_8bpp;
                                th_px_highlight |= (!bpp8) && highlight_4bpp;
                                th_px_highlight |= highlight_normal && !affine;
                                th_px_highlight |= highlight_affine && affine;
                                th_px_highlight |= highlight_window && win_pixel;
                            }
                            if (th_px_highlight) color = 0xFFFFFFFF;
                        }
                        else {
                            //color = 0xFFFF00FF;
                            continue;
                        }
                    }
                    else {
                        color = 0xFFFFFFFF;
                    }
                    rowptr[draw_x] = color;
                }
            }
            tx += pb;
            ty += pd;
        }

    }
}

static void render_image_view_tiles(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 256);

    u32 tn = 0;
    // 32x32 8x8 tiles, so 256x256 total
     bool bpp8 = 0;
    u32 pal = 0;
    u8 *tile_ptr = th->ppu.VRAM + 0x10000;
    for (u32 ty = 0; ty < 32; ty++) {
        u32 tile_screen_y = (ty * 8);
        for (u32 tx = 0; tx < 32; tx++) {
            u32 tile_screen_x = (tx * 8);
            for (u32 inner_y = 0; inner_y < 8; inner_y++) {
                u32 screen_y = tile_screen_y + inner_y;
                u32 *screen_y_ptr = outbuf + (screen_y * out_width);
                u32 screen_x = tile_screen_x;
                if (!bpp8) {
                    for (u32 inner_x = 0; inner_x < 4; inner_x++) {
                        u8 data = *tile_ptr;
                        for (u32 i = 0; i < 2; i++) {
                            u32 c = data & 0x15;
                            data >>= 4;
                            screen_y_ptr[screen_x] = gba_to_screen(th->ppu.palette_RAM[0x100 + pal + c]);
                            screen_x++;
                        }
                        tile_ptr++;
                    }
                }
                else {
                    printf("\nUHOH");
                }
            }
        }
    }
}

// Thanks tonc!
static u32 se_index_fast(u32 tx, u32 ty, u32 bgcnt) {
    u32 n = tx + ty * 32;
    if (tx >= 32)
        n += 0x03E0;
    if (ty >= 32 && (bgcnt == 3))
        n += 0x0400;
    return n;
}

static inline u32 shade_boundary_func(u32 kind, u32 incolor)
{
    switch(kind) {
        case 0: // Nothin
            return incolor;
        case 1: {// Shaded
            u32 c = (incolor & 0xFFFFFF00) | 0xFF000000;
            u32 v = incolor & 0xFF;
            v += 0xFF;
            v >>= 1;
            if (v > 255) v = 255;
            return c | v; }
        case 2: // black
            return 0xFF000000;
        case 3: // white
            return 0xFFFFFFFF;
    }
    return 0;
}

static void render_image_view_bgmap(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width, int bg_num) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    auto *bg = &th->ppu.mbg[bg_num];

    image_view *iv = &dview->image;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    u32 max_vsize = bg->vpixels;
    u32 max_hsize = bg->hpixels;
    memset(outbuf, 0, out_width * 4 * 1024);
    u32 affine_char_base;
    u32 affine_screen_base;
    u32 has_affine = 0;
    u32 affine_htiles = 0;
    // Search through debug info for the bg
    for (auto & line : th->dbg_info.line) {
        u32 do_affine = 0;
        if (line.bg_mode == 1) {
            if (bg_num == 2) do_affine = 1;
        }
        else if (line.bg_mode == 2) {
            if (bg_num >= 2) do_affine = 1;
        }
        auto *dbgbg = &line.bg[bg_num];
        if (do_affine) {
            if (!has_affine) {
                u32 hsize = dbgbg->htiles * 8;
                u32 vsize = dbgbg->vtiles * 8;
                if (hsize > max_hsize) max_hsize = hsize;
                if (vsize > max_vsize) max_vsize = vsize;
                affine_char_base = dbgbg->character_base_block;
                affine_screen_base = dbgbg->screen_base_block;
                affine_htiles = dbgbg->htiles;
            }
            has_affine = 1;
        }
    }
    if (!has_affine) {
        switch (th->ppu.io.bg_mode) {
            case 3: // 240x160 16-bit 1 buffer
                max_hsize = 240;
                max_vsize = 160;
                break;
            case 4: // 240x160 8-bit 2 buffer
                max_hsize = 480;
                max_vsize = 160;
                break;
            case 5: // 160x128 16-bit 2 buffer
                max_hsize = 320;
                max_vsize = 128;
                break;
        }
    }

    iv->viewport.p[1].x = static_cast<i32>(max_hsize);
    iv->viewport.p[1].y = static_cast<i32>(max_vsize);

    debugger_widget_textbox *tb = &dview->options.at(0).textbox;
    tb->clear();
    tb->sprintf("hsize:%d vsize:%d", max_hsize, max_vsize);

    if (th->ppu.io.bg_mode == 4) {
        for (u32 screen_y = 0; screen_y < 160; screen_y++) {
            u32 *lineptr = (outbuf + (screen_y * out_width));
            for (u32 screen_x = 0; screen_x < 480; screen_x++) {
                u32 get_x = screen_x % 240;
                u32 base_addr = (screen_x >= 240) ? 0xA000 : 0;
                u32 c = th->ppu.palette_RAM[th->ppu.VRAM[base_addr + (screen_y * 240) + get_x]];
                if (th->ppu.io.frame ^ (screen_x >= 240)) {
                    u32 b = (c >> 16) & 0xFF;
                    u32 g = (c >> 8) & 0xFF;
                    u32 r = (c >> 0) & 0xFF;
                    c = 0xFF000000 | (b << 16) | (g << 8) | r;
                }
                lineptr[screen_x] = gba_to_screen(c);
            }
        }
    }
    else {
        bool bpp8 = bg->bpp8;
        u32 character_base_block = affine_char_base;
        u32 screen_base_block = affine_screen_base;
        if (has_affine) {
            for (u32 tilemap_y = 0; tilemap_y < max_vsize; tilemap_y++) {
                u32 *lineptr = (outbuf + (tilemap_y * out_width));
                u8 *scrollptr = &th->dbg_info.bg_scrolls[bg_num].lines[128 * tilemap_y];
                for (u32 tilemap_x = 0; tilemap_x < max_hsize; tilemap_x++) {
                    u32 is_shaded = (scrollptr[tilemap_x >> 3] >> (tilemap_x & 7)) & 1;
                    u32 px = tilemap_x;
                    u32 py = tilemap_y;
                    u32 block_x = px >> 3;
                    u32 block_y = py >> 3;
                    u32 line_in_tile = py & 7;

                    u32 tile_width = affine_htiles;

                    u32 screenblock_addr = screen_base_block;
                    screenblock_addr += block_x + (block_y * tile_width);
                    u32 tile_num = ((u8 *) th->ppu.VRAM)[screenblock_addr];

                    // so now, grab that tile...
                    u32 tile_start_addr = character_base_block + (tile_num * 64);
                    u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
                    line_start_addr &= 0xFFFF;
                    u32 has = 0;
                    u8 *lsptr = ((u8 *) th->ppu.VRAM) + line_start_addr;
                    u8 data = lsptr[px & 7];
                    u32 color = 0xFF000000;
                    if (data != 0) {
                        has = 1;
                        color = gba_to_screen(th->ppu.palette_RAM[data]);
                    }
                    lineptr[tilemap_x] = shade_boundary_func(is_shaded * 3, color);
                }
            }
        }
    }
}

static void render_image_view_bg(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width, int bg_num)
{
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 160);

    auto *tb = &dview->options.at(0).textbox;
    auto *cb_highlight_transparent = &dview->options.at(1).checkbox;
    auto *cb_highlight_window_shaded = &dview->options.at(2).checkbox;
    auto *cb_highlight_window_occluded = &dview->options.at(3).checkbox;
    u32 highlight_transparent = cb_highlight_transparent->value;
    u32 highlight_window_shaded = cb_highlight_window_shaded->value;
    u32 highlight_window_occluded = cb_highlight_window_occluded->value;

    auto *bg = &th->ppu.mbg[bg_num];

    tb->clear();
    u32 is_affine = (((th->ppu.io.bg_mode == 1) && (bg_num == 2)) || ((th->ppu.io.bg_mode == 2) && (bg_num >= 2)));
    tb->sprintf("enable:%d  bpp:%c  htiles:%d  vtiles:%d\naffine:%d  mode:%d  priority:%d\nsample a:%d  b:%d", bg->enable, bg->bpp8 ? '8' : '4', bg->htiles, bg->vtiles, is_affine, th->ppu.io.bg_mode, bg->priority, th->ppu.blend.targets_a[bg_num+1], th->ppu.blend.targets_b[bg_num+1]);
    i32 h_origin = 0;
    i32 v_origin = 0;
    for (i32 screen_y = 0; screen_y < 160; screen_y++) {
        auto *dbgl = &th->dbg_info.line[screen_y];
        auto *bgd = &dbgl->bg[bg_num];
        u32 *lineptr = (outbuf + (screen_y * out_width));

        is_affine = (((dbgl->bg_mode == 1) && (bg_num == 2)) || ((dbgl->bg_mode == 2) && (bg_num >= 2)));

        // Affine infos for non-affine
        // pa is x change per x
        // pb is x change per y
        // pc is y change per x
        // pd is y change per y
        i32 pa = 1 << 8, pb = 0, pc = 0, pd = 1 << 8;
        u32 hpixels = bgd->htiles * 8;
        u32 vpixels = bgd->vtiles * 8;
        i32 hpixels_mask = (i32)hpixels - 1;
        i32 vpixels_mask = (i32)vpixels - 1;

        i32 sxadd = 0, syadd = 0;

        i64 hpos = ((bgd->hscroll & hpixels_mask)) << 8;
        i64 vpos = ((bgd->vscroll & vpixels_mask)) << 8;
        if (is_affine) {
            pa = bgd->pa; pb = bgd->pb; pc = bgd->pc; pd = bgd->pd;

            //pos_y = bgd->vpos + (pd*screen_y);
            //pos_x = bgd->hpos + (pb*screen_y);
            hpos = bgd->x_lerp;
            vpos = bgd->y_lerp;
            //hpos = bgd->hpos;
            //vpos = bgd->vpos;

            sxadd = 0;
            syadd = 0;
        }

        // Each line is set up separately...
        u32 no_px = 0; // set to 1 if we flunk outta here....
        for (i32 screen_x = 0; screen_x < 240; screen_x++) {
            i64 ix = screen_x + sxadd;
            i64 iy = screen_y + syadd;
            i64 px, py;
            if (is_affine) {
                px = hpos >> 8;
                py = vpos >> 8;
                hpos += pa;
                vpos += pc;
                if (bgd->display_overflow) {
                    px &= hpixels_mask;
                    py &= vpixels_mask;
                }
                else {
                    no_px = ((px < 0) || (px >= hpixels) || (py < 0) || (py >= vpixels));
                }
            } else {
                px = ((pa*ix + pb*iy)+hpos)>>8;    // get x texture coordinate
                py = ((pc*ix + pd*iy)+vpos)>>8;    // get y texture coordinate
                px &= hpixels_mask;
                py &= vpixels_mask;
            }

            // Deal with clipping
            u32 color=0, has=0, palette=0, bpp8=0, priority=0;
            if (!no_px) {
                // Deal with fetching pixel
                u32 block_x = px >> 3;
                u32 block_y = py >> 3;
                if (is_affine) {
                    u32 line_in_tile = py & 7;
                    u32 tile_width = bgd->htiles;
                    u32 screenblock_addr = bgd->screen_base_block;
                    screenblock_addr += block_x + (block_y * tile_width);
                    u32 tile_num = ((u8 *)th->ppu.VRAM)[screenblock_addr];
                    u32 tile_start_addr = bgd->character_base_block + (tile_num * 64);
                    u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
                    u8 *tileptr = ((u8 *)th->ppu.VRAM) + line_start_addr;
                    color = tileptr[px & 7];
                    if (color != 0) {
                        has = 1;
                        bpp8 = 1;
                        palette = 0;
                        priority = bgd->priority;
                    }
                }
                else {
                    u32 screenblock_addr = bgd->screen_base_block + (se_index_fast(block_x, block_y, bg->screen_size) << 1);
                    u16 attr = *(u16 *)(((u8 *)th->ppu.VRAM) + screenblock_addr);
                    u32 tile_num = attr & 0x3FF;
                    u32 hflip = (attr >> 10) & 1;
                    u32 vflip = (attr >> 11) & 1;
                    u32 palbank = bgd->bpp8 ? 0 : ((attr >> 12) & 15) << 4;
                    u32 line_in_tile = py & 7;
                    if (vflip) line_in_tile = 7 - line_in_tile;
                    u32 col_in_tile = px & 7;
                    if (hflip) col_in_tile = 7 - col_in_tile;
                    if (!bgd->bpp8) col_in_tile>>=1;
                    u32 tile_bytes = bg->bpp8 ? 64 : 32;
                    u32 line_size = bg->bpp8 ? 8 : 4;
                    u32 tile_start_addr = bg->character_base_block + (tile_num * tile_bytes);
                    u32 line_addr = tile_start_addr + (line_in_tile * line_size);
                    if (line_addr < 0x10000) { // hardware doesn't draw from up there
                        u8 *tileptr = ((u8 *)th->ppu.VRAM) + line_addr;
                        color = tileptr[col_in_tile];
                        if (!bpp8) {
                            if (px&1) color >>= 4;
                            color &= 15;
                            palette = palbank;
                        }
                        if (color != 0) {
                            has = 1;
                            bpp8 = bgd->bpp8;
                            priority = bgd->priority;
                        }
                    }
                }
                if (has) color = th->ppu.palette_RAM[palette + color];
            }
            if (!has || no_px) {
                if (highlight_transparent) color = 0b111111111111111;
            }

            *lineptr = gba_to_screen(color);
            lineptr++;
        }
    }
}

static void render_image_view_window(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width, int win_num) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 160);
    u8 bit = 1 << win_num;

    debugger_widget_textbox *tb = &dview->options.at(0).textbox;
    auto *w = &th->ppu.window[win_num];
    tb->clear();
    tb->sprintf("enable:%d  color_effect:%d\nbg0:%d  bg1:%d  bg2:%d  bg3:%d  obj:%d", w->enable, w->active[5], w->active[1], w->active[2], w->active[3], w->active[4], w->active[0]);

    for (u32 y = 0; y < 160; y++) {
        u32 *rowptr = outbuf + (y * out_width);
        u8 *wptr = &th->dbg_info.line[y].window_coverage[0];
        for (u32 x = 0; x < 240; x++) {
            rowptr[x]= ((*wptr) & bit) ? 0xFFFFFFFF : 0xFF000000;
            wptr++;
        }
    }
}

static void render_image_view_window0(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_window1(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_window2(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 2);
}

static void render_image_view_window3(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 3);
}

static void render_image_view_bg0(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_bg1(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_bg2(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 2);
}

static void render_image_view_bg3(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 3);
}

static void render_image_view_bg0map(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bgmap(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_bg1map(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bgmap(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_bg2map(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bgmap(dbgr, dview, ptr, out_width, 2);
}

static void render_image_view_bg3map(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bgmap(dbgr, dview, ptr, out_width, 3);
}


#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11
static void render_image_view_palette(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2)); // Clear out at least 4 rows worth

    u32 offset = 0;
    u32 y_offset = 0;
    for (u32 lohi = 0; lohi < 0x200; lohi += 0x100) {
        for (u32 palette = 0; palette < 16; palette++) {
            for (u32 color = 0; color < 16; color++) {
                u32 y_top = y_offset + offset;
                u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
                u32 c = gba_to_screen(th->ppu.palette_RAM[lohi + (palette << 4) + color]);
                for (u32 y = 0; y < PAL_BOX_SIZE; y++) {
                    u32 *box_ptr = (outbuf + ((y_top + y) * out_width) + x_left);
                    for (u32 x = 0; x < PAL_BOX_SIZE; x++) {
                        box_ptr[x] = c;
                    }
                }
            }
            y_offset += PAL_BOX_SIZE;
        }
        offset += 2;
    }
}




static void get_disassembly_ARM7TDMI(void *gbaptr, disassembly_view &dview, disassembly_entry &entry)
{
    auto* th = static_cast<core *>(gbaptr);
    th->cpu.disassemble_entry(entry);
}

static disassembly_vars get_disassembly_vars_ARM7TDMI(void *gbaptr, disassembly_view &dv)
{
    const auto* th = static_cast<core *>(gbaptr);
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.trace.ins_PC;
    dvar.current_clock_cycle = th->clock.master_cycle_count;
    return dvar;
}

static void setup_cpu_trace(debugger_interface *dbgr, core *th)
{
    cvec_ptr p = dbgr->make_view(dview_trace);
    debugger_view *dview = &p.get();
    trace_view *tv = &dview->trace;
    snprintf(tv->name, sizeof(tv->name), "ARM7TDMI Trace");
    tv->add_col("Arch", 5);
    tv->add_col("Cycle#", 10);
    tv->add_col("Addr", 8);
    tv->add_col("Opcode", 8);
    tv->add_col("Disassembly", 45);
    tv->add_col("Context", 32);
    tv->autoscroll = 1;
    tv->display_end_top = 0;

    th->cpu.dbg.tvptr = tv;
}

static void setup_ARM7TDMI_disassembly(debugger_interface *dbgr, core* th)
{
    cvec_ptr p = dbgr->make_view(dview_disassembly);
    debugger_view *dview = &p.get();
    disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFFFF;
    dv->addr_column_size = 8;
    dv->has_context = 1;
    dv->processor_name.sprintf("ARM7TDMI");

    create_and_bind_registers_ARM7TDMI(th, dv);
    dv->fill_view.ptr = static_cast<void *>(th);
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = static_cast<void *>(th);
    dv->get_disassembly.func = &get_disassembly_ARM7TDMI;

    dv->get_disassembly_vars.ptr = (void *)th;
    dv->get_disassembly_vars.func = &get_disassembly_vars_ARM7TDMI;
}

static void setup_image_view_tiles(core* th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.tiles = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.tiles.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 256;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 256, 256 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_tiles;
    snprintf(iv->label, sizeof(iv->label), "Tile Viewer");
}

static void setup_image_view_sprites(core* th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.sprites = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.sprites.get();
    image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;

    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = ivec2(iv->width, iv->height);

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sprites;
    snprintf(iv->label, sizeof(iv->label), "Layer/Sprites Viewer");

    dview->options.reserve(20);
    debugger_widgets_add_checkbox(dview->options, "Draw transparencies", 1, 1, 1);
    debugger_widgets_add_checkbox(dview->options, "Draw 4bpp", 1, 1, 0);
    debugger_widgets_add_checkbox(dview->options, "Draw 8bpp", 1, 1, 1);
    debugger_widgets_add_checkbox(dview->options, "Draw normal", 1, 1, 1);
    debugger_widgets_add_checkbox(dview->options, "Draw affine", 1, 1, 1);
    debugger_widgets_add_checkbox(dview->options, "Highlight 4bpp", 1, 0, 0);
    debugger_widgets_add_checkbox(dview->options, "Highlight 8bpp", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "Highlight normal", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "Highlight affine", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "Highlight window", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "White box 4bpp", 1, 0, 0);
    debugger_widgets_add_checkbox(dview->options, "White box 8bpp", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "White box normal", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "White box affine", 1, 0, 1);
    debugger_widgets_add_checkbox(dview->options, "White box window", 1, 0, 1);
}

static void setup_image_view_palettes(core* th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.palettes = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.palettes.get();
    image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = ((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2;

    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = ivec2(iv->width, iv->height);

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");
}

static void setup_image_view_bgmap(core* th, debugger_interface *dbgr, u32 wnum) {
    debugger_view *dview;
    switch(wnum) {
        case 0:
            th->dbg.image_views.bg0map = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg0map.get();
            break;
        case 1:
            th->dbg.image_views.bg1map = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg1map.get();
            break;
        case 2:
            th->dbg.image_views.bg2map = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg2map.get();
            break;
        case 3:
            th->dbg.image_views.bg3map = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg3map.get();
            break;
        default: NOGOHERE;
    }
    image_view *iv = &dview->image;
    iv->width = 1024;
    iv->height = 1024;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 256, 256 };

    iv->update_func.ptr = th;

    dview->options.reserve(10);
    debugger_widgets_add_textbox(dview->options, "hsize:0  vsize:0", 1);
    debugger_widgets_add_checkbox(dview->options, "Testing test", 1, 0, 1);
    debugger_widget *rg = &debugger_widgets_add_radiogroup(dview->options, "Shade scroll area", 1, 0, 1);
    rg->radiogroup.add_button("None", 0, 1);
    rg->radiogroup.add_button("Inside", 1, 1);
    rg->radiogroup.add_button("Outside", 2, 1);
    rg = &debugger_widgets_add_radiogroup(dview->options, "Shading method", 1, 0, 1);
    rg->radiogroup.add_button("Shaded", 0, 1);
    rg->radiogroup.add_button("Black", 1, 1);
    rg->radiogroup.add_button("White", 2, 1);

    switch(wnum) {
        case 0:
            iv->update_func.func = &render_image_view_bg0map;
            snprintf(iv->label, sizeof(iv->label), "Tilemap/BG0");
            break;
        case 1:
            iv->update_func.func = &render_image_view_bg1map;
            snprintf(iv->label, sizeof(iv->label), "Tilemap/BG1");
            break;
        case 2:
            iv->update_func.func = &render_image_view_bg2map;
            snprintf(iv->label, sizeof(iv->label), "Tilemap/BG2");
            break;
        case 3:
            iv->update_func.func = &render_image_view_bg3map;
            snprintf(iv->label, sizeof(iv->label), "Tilemap/BG3");
            break;
        default: NOGOHERE;
    }

}

static void setup_image_view_bg(core* th, debugger_interface *dbgr, u32 wnum)
{
    debugger_view *dview;
    switch(wnum) {
        case 0:
            th->dbg.image_views.bg0 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg0.get();
            break;
        case 1:
            th->dbg.image_views.bg1 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg1.get();
            break;
        case 2:
            th->dbg.image_views.bg2 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg2.get();
            break;
        case 3:
            th->dbg.image_views.bg3 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.bg3.get();
            break;
        default: NOGOHERE;
    }
    image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 240, 160 };

    iv->update_func.ptr = th;
    dview->options.reserve(dview->options.size()+5);
    debugger_widgets_add_textbox(dview->options, "num:%d enable:0 bpp:4", 1);
    debugger_widgets_add_checkbox(dview->options, "Highlight transparent pixels", 1, 0, 0);
    debugger_widgets_add_checkbox(dview->options, "Highlight window-shaded pixels", 1, 0, 0);
    debugger_widgets_add_checkbox(dview->options, "Highlight window-occluded pixels", 1, 0, 0);

    switch(wnum) {
        case 0:
            iv->update_func.func = &render_image_view_bg0;
            snprintf(iv->label, sizeof(iv->label), "Layer/BG0 Viewer");
            break;
        case 1:
            iv->update_func.func = &render_image_view_bg1;
            snprintf(iv->label, sizeof(iv->label), "Layer/BG1 Viewer");
            break;
        case 2:
            iv->update_func.func = &render_image_view_bg2;
            snprintf(iv->label, sizeof(iv->label), "Layer/BG2 Viewer");
            break;
        case 3:
            iv->update_func.func = &render_image_view_bg3;
            snprintf(iv->label, sizeof(iv->label), "Layer/BG3 Viewer");
            break;
        default: NOGOHERE;
    }
}

static void setup_events_view(core& th, debugger_interface *dbgr) {
    th.dbg.events.view = dbgr->make_view(dview_events);
    auto *dview = &th.dbg.events.view.get();
    events_view &ev = dview->events;

    ev.timing = ev_timing_master_clock,
    ev.master_clocks.per_line = 1232;
    ev.master_clocks.height = 228;
    ev.master_clocks.ptr = &th.clock.master_cycle_count;

    for (auto & di : ev.display) {
        di.width = 308;
        di.height = 228;
        di.buf = nullptr;
        di.frame_num = 0;
    }

    ev.associated_display = th.ppu.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_GBA_CATEGORY_PPU);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GBA_CATEGORY_CPU);
    ev.events.reserve(DBG_GBA_EVENT_MAX);
    DEBUG_REGISTER_EVENT("Affine regs write", 0xFFFF00, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_WRITE_AFFINE_REGS, 1);
    DEBUG_REGISTER_EVENT("Set IF.HBlank", 0xFF00FF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_HBLANK_IRQ, 1);
    DEBUG_REGISTER_EVENT("Set IF.VBlank", 0x00FF00, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_VBLANK_IRQ, 1);
    DEBUG_REGISTER_EVENT("Set IF.LY=LYC", 0x00FFFF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_LINECOUNT_IRQ, 1);
    DEBUG_REGISTER_EVENT("Write VRAM", 0x0000FF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_WRITE_VRAM, 1);

    SET_EVENT_VIEW(th.cpu);
    debugger_report_frame(th.dbg.interface);
}


static void setup_image_view_window(core* th, debugger_interface *dbgr, u32 wnum)
{
    debugger_view *dview;
    switch(wnum) {
        case 0:
            th->dbg.image_views.window0 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.window0.get();
            break;
        case 1:
            th->dbg.image_views.window1 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.window1.get();
            break;
        case 2:
            th->dbg.image_views.window2 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.window2.get();
            break;
        case 3:
            th->dbg.image_views.window3 = dbgr->make_view(dview_image);
            dview = &th->dbg.image_views.window3.get();
            break;
        default: NOGOHERE;
    }
    image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 240, 160 };

    dview->options.reserve(10);
    debugger_widgets_add_textbox(dview->options, "enable:%d\nbg0:0  bg1:0  bg2:0  bg3:0  obj:0", 1);

    iv->update_func.ptr = th;
    switch(wnum) {
        case 0:
            iv->update_func.func = &render_image_view_window0;
            snprintf(iv->label, sizeof(iv->label), "Window/0 Viewer");
            break;
        case 1:
            iv->update_func.func = &render_image_view_window1;
            snprintf(iv->label, sizeof(iv->label), "Window/1 Viewer");
            break;
        case 2:
            iv->update_func.func = &render_image_view_window2;
            snprintf(iv->label, sizeof(iv->label), "Window/Sprite Viewer");
            break;
        case 3:
            iv->update_func.func = &render_image_view_window3;
            snprintf(iv->label, sizeof(iv->label), "Window/Outside Viewer");
            break;
    }

}

static void setup_waveforms_view(core* th, debugger_interface *dbgr)
{
    printf("\nSETTING UP WAVEFORMS VIEW...");
    th->dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    debugger_view *dview = &th->dbg.waveforms.view.get();
    auto *wv = &dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "GBA APU");

    wv->waveforms.reserve(8);

    debug_waveform *dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.main.make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[0].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Square 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[1].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Square 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[2].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Waveform");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[3].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Noise");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[4].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "FIFO A");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms.chan[5].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "FIFO B");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}


static void setup_image_view_sys_info(core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.sys_info = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.sys_info.get();
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sys_info;

    snprintf(iv->label, sizeof(iv->label), "Sys Info");

    dview->options.reserve(15);
    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

static void setup_dbglog(debugger_interface *dbgr, core *th) {
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view *dv = &dview->dbglog;
    th->dbg.dvptr = dv;
    snprintf(dv->name, sizeof(dv->name), "Trace");
    dv->has_extra = 1;

    dbglog_category_node &root = dv->get_category_root();
    dbglog_category_node &arm7 = root.add_node(*dv, "ARM7TDMI", nullptr, 0, 0);
    arm7.add_node(*dv, "Instruction Trace", "ARM7", CAT_ARM7_INSTRUCTION, 0x80FF80);
    th->cpu.trace.dbglog.view = dv;
    th->cpu.trace.dbglog.id = CAT_ARM7_INSTRUCTION;
    arm7.add_node(*dv, "Halt", "ARM7.H", CAT_ARM7_HALT, 0xA0AF80);

    dbglog_category_node &dma = root.add_node(*dv, "DMA", nullptr, 0, 0);
    dma.add_node(*dv, "DMA Start", "dma.start", CAT_DMA_START, 0xFFFFFF);
}


void core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;
    dbgr.views.reserve(25);

    dbgr.supported_by_core = 0;
    dbgr.smallest_step = 1;

    setup_dbglog(&dbgr, this);
    //setup_ARM7TDMI_disassembly(dbgr, th);
    setup_waveforms_view(this, &dbgr);
    setup_events_view(*this, &dbgr);
    //setup_cpu_trace(dbgr, th);
    setup_image_view_palettes(this, &dbgr);
    setup_image_view_bg(this, &dbgr, 0);
    setup_image_view_bg(this, &dbgr, 1);
    setup_image_view_bg(this, &dbgr, 2);
    setup_image_view_bg(this, &dbgr, 3);
    setup_image_view_sprites(this, &dbgr);
    setup_image_view_bgmap(this, &dbgr, 0);
    setup_image_view_bgmap(this, &dbgr, 1);
    setup_image_view_bgmap(this, &dbgr, 2);
    setup_image_view_bgmap(this, &dbgr, 3);
    setup_image_view_tiles(this, &dbgr);
    setup_image_view_window(this, &dbgr, 0);
    setup_image_view_window(this, &dbgr, 1);
    setup_image_view_window(this, &dbgr, 2);
    setup_image_view_window(this, &dbgr, 3);
    setup_image_view_sys_info(this, &dbgr);
}
}