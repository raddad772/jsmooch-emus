//
// Created by . on 12/4/24.
//

#include <string.h>
#include <assert.h>

#include "helpers/color.h"
#include "gba.h"
#include "gba_bus.h"
#include "gba_debugger.h"

#define JTHIS struct GBA* this = (struct GBA*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct GBA* this

static void create_and_bind_registers_ARM7TDMI(struct GBA* this, struct disassembly_view *dv)
{
    u32 tkindex = 0;
    /*struct cpu_reg_context *rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "B");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "C");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "D");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "E");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "F");
    rg->kind = RK_int32;
    rg->index = tkindex++;
    rg->custom_render = &render_f_z80;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "SP");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IX");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "IY");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "EI");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HALT");
    rg->kind = RK_bool;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "AF_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "BC_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "DE_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "HL_");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

    rg = cvec_push_back(&dv->cpu.regs);
    snprintf(rg->name, sizeof(rg->name), "CxI");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = NULL;

#define BIND(dn, index) this->dbg.dasm_z80. dn = cvec_get(&dv->cpu.regs, index)
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

static void fill_disassembly_view(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dview)
{
    struct GBA* this = (struct GBA*)macptr;
    /*for (u32 i = 0; i < 8; i++) {
        this->dbg.dasm_m68k.D[i]->int32_data = this->m68k.regs.D[i];
        if (i < 7) this->dbg.dasm_m68k.A[i]->int32_data = this->m68k.regs.A[i];
    }
    this->dbg.dasm_m68k.PC->int32_data = this->m68k.regs.PC;
    this->dbg.dasm_m68k.SR->int32_data = this->m68k.regs.SR.u;
    if (this->dbg.dasm_m68k.SR->int32_data & 0x2000) { // supervisor mode
        this->dbg.dasm_m68k.USP->int32_data = this->m68k.regs.ASP;
        this->dbg.dasm_m68k.SSP->int32_data = this->m68k.regs.A[7];
    }
    else { // user mode
        this->dbg.dasm_m68k.USP->int32_data = this->m68k.regs.A[7];
        this->dbg.dasm_m68k.SSP->int32_data = this->m68k.regs.ASP;
    }
    this->dbg.dasm_m68k.supervisor->bool_data = this->m68k.regs.SR.S;
    this->dbg.dasm_m68k.trace->bool_data = this->m68k.regs.SR.T;
    this->dbg.dasm_m68k.IMASK->int32_data = this->m68k.regs.SR.I;
    this->dbg.dasm_m68k.CSR->int32_data = this->m68k.regs.SR.u & 0x1F;
    this->dbg.dasm_m68k.IR->int32_data = this->m68k.regs.IR;
    this->dbg.dasm_m68k.IRC->int32_data = this->m68k.regs.IRC;*/
}

static void get_obj_tile_size(u32 sz, u32 shape, u32 *htiles, u32 *vtiles)
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
    }
    printf("\nHEY! INVALID SHAPE %d", shape);
    *htiles = 1;
    *vtiles = 1;
#undef T
}

static void render_image_view_sprites(struct debugger_interface *dbgr, struct debugger_view *dview, void *my_ptr, u32 out_width) {
    struct GBA *this = (struct GBA *) my_ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 160);

    // Draw sprites!!!
    struct debugger_widget_checkbox *cb_draw_transparency = &((struct debugger_widget *)cvec_get(&dview->options, 0))->checkbox;
    struct debugger_widget_checkbox *cb_draw_4bpp = &((struct debugger_widget *)cvec_get(&dview->options, 1))->checkbox;
    struct debugger_widget_checkbox *cb_draw_8bpp = &((struct debugger_widget *)cvec_get(&dview->options, 2))->checkbox;
    struct debugger_widget_checkbox *cb_draw_normal = &((struct debugger_widget *)cvec_get(&dview->options, 3))->checkbox;
    struct debugger_widget_checkbox *cb_draw_affine = &((struct debugger_widget *)cvec_get(&dview->options, 4))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_4bpp = &((struct debugger_widget *)cvec_get(&dview->options, 5))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_8bpp = &((struct debugger_widget *)cvec_get(&dview->options, 6))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_normal = &((struct debugger_widget *)cvec_get(&dview->options, 7))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_affine = &((struct debugger_widget *)cvec_get(&dview->options, 8))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_window = &((struct debugger_widget *)cvec_get(&dview->options, 9))->checkbox;
    struct debugger_widget_checkbox *cb_wb_4bpp = &((struct debugger_widget *)cvec_get(&dview->options, 10))->checkbox;
    struct debugger_widget_checkbox *cb_wb_8bpp = &((struct debugger_widget *)cvec_get(&dview->options, 11))->checkbox;
    struct debugger_widget_checkbox *cb_wb_normal = &((struct debugger_widget *)cvec_get(&dview->options, 12))->checkbox;
    struct debugger_widget_checkbox *cb_wb_affine = &((struct debugger_widget *)cvec_get(&dview->options, 13))->checkbox;
    struct debugger_widget_checkbox *cb_wb_window = &((struct debugger_widget *)cvec_get(&dview->options, 14))->checkbox;
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
        u16 *ptr = ((u16 *)this->ppu.OAM) + (sn * 4);
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
        i32 tex_x_tiles = htiles;
        i32 tex_x_pixels = htiles * 8;
        i32 tex_y_pixels = vtiles * 8;
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
        u32 bpp8 = (ptr[0] >> 13) & 1;
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
            u16 *pbase_ptr = ((u16 *)this->ppu.OAM) + pbase;
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
        }
        i32 screen_x_right = screen_x + (i32)hpixels;
        i32 screen_y_bottom = screen_y + (i32)vpixels;

        for (i32 draw_y = screen_y; draw_y < screen_y_bottom; draw_y++) {
            u32 *rowptr = outbuf + (draw_y * out_width);
            i32 fx = tx;
            i32 fy = ty;
            for (i32 draw_x = screen_x; draw_x < screen_x_right; draw_x++) {
                u32 this_px_highlight = 0;
                u32 has = 0;
                u32 win_pixel = 0;
                this_px_highlight = 0;
                this_px_highlight |= bpp8 && wb_8bpp;
                this_px_highlight |= (!bpp8) && wb_4bpp;
                this_px_highlight |= wb_normal && !affine;
                this_px_highlight |= wb_affine && affine;
                this_px_highlight |= wb_window && (mode == 2);

                // Get pixel at coord fx, fy
                i32 tex_x = (fx >> 8) + ((i32)tex_x_pixels >> 1);
                i32 tex_y = (fy >> 8) + ((i32)tex_y_pixels >> 1);

                fx += pa;
                fy += pc;

                if ((draw_x >= 0) && (draw_x < 240) && (draw_y >= 0) && (draw_y < 140)) {
                    u32 color;
                    if (!this_px_highlight) {
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
                            if (this->ppu.io.obj_mapping_1d) {
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
                            u8 data = this->ppu.VRAM[tile_px_addr];
                            if (!bpp8) {
                                if (tile_x & 1) data >>= 4;
                                data &= 15;
                            }
                            if ((data == 0) && draw_transparency) continue;
                            if (bpp8) {
                                color = gba_to_screen(this->ppu.palette_RAM[0x100 + data]);
                            }
                            else {
                                color = gba_to_screen(this->ppu.palette_RAM[0x100 + (palette << 4) + data]);
                            }
                            if ((mode == 2) && !highlight_window) continue;
                            if ((mode == 2) && highlight_window) color = 0xFFFFFFFF;
                            else {
                                this_px_highlight |= bpp8 && highlight_8bpp;
                                this_px_highlight |= (!bpp8) && highlight_4bpp;
                                this_px_highlight |= highlight_normal && !affine;
                                this_px_highlight |= highlight_affine && affine;
                                this_px_highlight |= highlight_window && win_pixel;
                            }
                            if (this_px_highlight) color = 0xFFFFFFFF;
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

static void render_image_view_tiles(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct GBA *this = (struct GBA *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 256);

    u32 tn = 0;
    // 32x32 8x8 tiles, so 256x256 total
    u32 bpp8 = 0;
    u32 pal = 0;
    u8 *tile_ptr = this->ppu.VRAM + 0x10000;
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
                            screen_y_ptr[screen_x] = gba_to_screen(this->ppu.palette_RAM[0x100 + pal + c]);
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

static void render_image_view_bg(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width, int bg_num)
{
    struct GBA *this = (struct GBA *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 160);

    struct debugger_widget_textbox *tb = &((struct debugger_widget *)cvec_get(&dview->options, 0))->textbox;
    struct debugger_widget_checkbox *cb_highlight_transparent = &((struct debugger_widget *)cvec_get(&dview->options, 1))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_window_shaded = &((struct debugger_widget *)cvec_get(&dview->options, 2))->checkbox;
    struct debugger_widget_checkbox *cb_highlight_window_occluded = &((struct debugger_widget *)cvec_get(&dview->options, 3))->checkbox;
    u32 highlight_transparent = cb_highlight_transparent->value;
    u32 highlight_window_shaded = cb_highlight_window_shaded->value;
    u32 highlight_window_occluded = cb_highlight_window_occluded->value;

    struct GBA_PPU_bg *bg = &this->ppu.bg[bg_num];

    debugger_widgets_textbox_clear(tb);
    u32 is_affine = (((this->ppu.io.bg_mode == 2) && (bg_num == 2)) || ((this->ppu.io.bg_mode == 3) && (bg_num >= 2)));
    debugger_widgets_textbox_sprintf(tb, "enable:%d  bpp:%c  htiles:%d  vtiles:%d\naffine:%d  mode:%d  priority:%d\nsample a:%d  b:%d", bg->enable, bg->bpp8 ? '8' : '4', bg->htiles, bg->vtiles, is_affine, this->ppu.io.bg_mode, bg->priority, this->ppu.blend.targets_a[bg_num+1], this->ppu.blend.targets_b[bg_num+1]);

    for (i32 screen_y = 0; screen_y < 160; screen_y++) {
        struct GBA_DBG_line *dbgl = &this->dbg_info.line[screen_y];
        struct GBA_DBG_line_bg *bgd = &dbgl->bg[bg_num];
        u32 *lineptr = (outbuf + (screen_y * out_width));

        is_affine = (((dbgl->bg_mode == 2) && (bg_num == 2)) || ((dbgl->bg_mode == 3) && (bg_num >= 2)));

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
            hpos = bgd->hpos;
            vpos = bgd->vpos;
            sxadd = 0;
            syadd = 0;
        }

        // Each line is set up separately...
        for (i32 screen_x = 0; screen_x < 240; screen_x++) {
            i64 ix = screen_x + sxadd;
            i64 iy = screen_y + syadd;
            i64 px = ((pa*ix + pb*iy)+hpos)>>8;    // get x texture coordinate
            i64 py = ((pc*ix + pd*iy)+vpos)>>8;    // get y texture coordinate

            u32 no_px = 0; // set to 1 if we flunk outta here....
            // Deal with clipping
            if (is_affine) {
                if (bgd->display_overflow) {
                    px &= hpixels_mask;
                    py &= vpixels_mask;
                }
                else {
                    no_px = ((px < 0) || (px >= hpixels) || (py < 0) || (py >= vpixels));
                }
            }
            else {
                px &= hpixels_mask;
                py &= vpixels_mask;
            }
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
                    u32 tile_num = ((u8 *)this->ppu.VRAM)[screenblock_addr];
                    u32 tile_start_addr = bgd->character_base_block + (tile_num * 64);
                    u32 line_start_addr = tile_start_addr + (line_in_tile * 8);
                    u8 *tileptr = ((u8 *)this->ppu.VRAM) + line_start_addr;
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
                    u16 attr = *(u16 *)(((u8 *)this->ppu.VRAM) + screenblock_addr);
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
                        u8 *tileptr = ((u8 *)this->ppu.VRAM) + line_addr;
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
                if (has) color = this->ppu.palette_RAM[palette + color];
            }
            if (!has || no_px) {
                if (highlight_transparent) color = 0b111111111111111;
            }

            *lineptr = gba_to_screen(color);
            lineptr++;
        }
    }
}

static void render_image_view_window(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width, int win_num) {
    struct GBA *this = (struct GBA *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 160);
    u8 bit = 1 << win_num;

    struct debugger_widget_textbox *tb = &((struct debugger_widget *)cvec_get(&dview->options, 0))->textbox;
    struct GBA_PPU_window *w = &this->ppu.window[win_num];
    debugger_widgets_textbox_clear(tb);
    debugger_widgets_textbox_sprintf(tb, "enable:%d  color_effect:%d\nbg0:%d  bg1:%d  bg2:%d  bg3:%d  obj:%d", w->enable, w->active[5], w->active[1], w->active[2], w->active[3], w->active[4], w->active[0]);

    for (u32 y = 0; y < 160; y++) {
        u32 *rowptr = outbuf + (y * out_width);
        u8 *wptr = &this->dbg_info.line[y].window_coverage[0];
        for (u32 x = 0; x < 240; x++) {
            rowptr[x]= ((*wptr) & bit) ? 0xFFFFFFFF : 0xFF000000;
            wptr++;
        }
    }
}

static void render_image_view_window0(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_window1(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_window2(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 2);
}

static void render_image_view_window3(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_window(dbgr, dview, ptr, out_width, 3);
}

static void render_image_view_bg0(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 0);
}

static void render_image_view_bg1(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 1);
}

static void render_image_view_bg2(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 2);
}

static void render_image_view_bg3(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    render_image_view_bg(dbgr, dview, ptr, out_width, 3);
}


#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11
static void render_image_view_palette(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct GBA *this = (struct GBA *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2)); // Clear out at least 4 rows worth

    u32 offset = 0;
    u32 y_offset = 0;
    for (u32 lohi = 0; lohi < 0x200; lohi += 0x100) {
        for (u32 palette = 0; palette < 16; palette++) {
            for (u32 color = 0; color < 16; color++) {
                u32 y_top = y_offset + offset;
                u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
                u32 c = gba_to_screen(this->ppu.palette_RAM[lohi + (palette << 4) + color]);
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




static void get_disassembly_ARM7TDMI(void *genptr, struct debugger_interface *dbgr, struct disassembly_view *dview, struct disassembly_entry *entry)
{
    struct GBA* this = (struct GBA*)genptr;
    ARM7TDMI_disassemble_entry(&this->cpu, entry);
}

static struct disassembly_vars get_disassembly_vars_ARM7TDMI(void *macptr, struct debugger_interface *dbgr, struct disassembly_view *dv)
{
    struct GBA* this = (struct GBA*)macptr;
    struct disassembly_vars dvar;
    dvar.address_of_executing_instruction = this->cpu.trace.ins_PC;
    dvar.current_clock_cycle = this->clock.master_cycle_count;
    return dvar;
}

static void setup_cpu_trace(struct debugger_interface *dbgr, struct GBA *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_trace);
    struct debugger_view *dview = cpg(p);
    struct trace_view *tv = &dview->trace;
    snprintf(tv->name, sizeof(tv->name), "ARM7TDMI Trace");
    trace_view_add_col(tv, "Arch", 5);
    trace_view_add_col(tv, "Cycle#", 10);
    trace_view_add_col(tv, "Addr", 8);
    trace_view_add_col(tv, "Opcode", 8);
    trace_view_add_col(tv, "Disassembly", 45);
    trace_view_add_col(tv, "Context", 32);
    tv->autoscroll = 1;
    tv->display_end_top = 0;

    this->cpu.dbg.tvptr = tv;
}

static void setup_ARM7TDMI_disassembly(struct debugger_interface *dbgr, struct GBA* this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_disassembly);
    struct debugger_view *dview = cpg(p);
    struct disassembly_view *dv = &dview->disassembly;
    dv->mem_end = 0xFFFFFFFF;
    dv->addr_column_size = 8;
    dv->has_context = 1;
    jsm_string_sprintf(&dv->processor_name, "ARM7TDMI");

    create_and_bind_registers_ARM7TDMI(this, dv);
    dv->fill_view.ptr = (void *)this;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = (void *)this;
    dv->get_disassembly.func = &get_disassembly_ARM7TDMI;

    dv->get_disassembly_vars.ptr = (void *)this;
    dv->get_disassembly_vars.func = &get_disassembly_vars_ARM7TDMI;
}

static void setup_image_view_tiles(struct GBA* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.tiles = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.tiles);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 256;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 256 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_tiles;
    snprintf(iv->label, sizeof(iv->label), "Tile Viewer");
}

static void setup_image_view_sprites(struct GBA* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.sprites = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.sprites);
    struct image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ iv->width, iv->height };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_sprites;
    snprintf(iv->label, sizeof(iv->label), "Layer/Sprites Viewer");

    debugger_widgets_add_checkbox(&dview->options, "Draw transparencies", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw 4bpp", 1, 1, 0);
    debugger_widgets_add_checkbox(&dview->options, "Draw 8bpp", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw normal", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Draw affine", 1, 1, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight 4bpp", 1, 0, 0);
    debugger_widgets_add_checkbox(&dview->options, "Highlight 8bpp", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight normal", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight affine", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight window", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "White box 4bpp", 1, 0, 0);
    debugger_widgets_add_checkbox(&dview->options, "White box 8bpp", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "White box normal", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "White box affine", 1, 0, 1);
    debugger_widgets_add_checkbox(&dview->options, "White box window", 1, 0, 1);
}

static void setup_image_view_palettes(struct GBA* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.palettes = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.palettes);
    struct image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = ((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ iv->width, iv->height };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");
}

static void setup_image_view_bg(struct GBA* this, struct debugger_interface *dbgr, u32 wnum)
{
    struct debugger_view *dview;
    switch(wnum) {
        case 0:
            this->dbg.image_views.bg0 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.bg0);
            break;
        case 1:
            this->dbg.image_views.bg1 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.bg1);
            break;
        case 2:
            this->dbg.image_views.bg2 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.bg2);
            break;
        case 3:
            this->dbg.image_views.bg3 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.bg3);
            break;
    }
    struct image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 240, 160 };

    iv->update_func.ptr = this;

    debugger_widgets_add_textbox(&dview->options, "num:%d enable:0 bpp:4", 1);
    debugger_widgets_add_checkbox(&dview->options, "Highlight transparent pixels", 1, 0, 0);
    debugger_widgets_add_checkbox(&dview->options, "Highlight window-shaded pixels", 1, 0, 0);
    debugger_widgets_add_checkbox(&dview->options, "Highlight window-occluded pixels", 1, 0, 0);

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
    }

}

static void setup_events_view(struct GBA* this, struct debugger_interface *dbgr) {
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    ev->timing = ev_timing_master_clock,
    ev->master_clocks.per_line = 1232;
    ev->master_clocks.height = 228;
    ev->master_clocks.ptr = &this->clock.master_cycle_count;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 308;
        ev->display[i].height = 228;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }

    ev->associated_display = this->ppu.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_GBA_CATEGORY_PPU);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GBA_CATEGORY_CPU);
    cvec_grow_by(&ev->events, DBG_GBA_EVENT_MAX);
    cvec_lock_reallocs(&ev->events);
    DEBUG_REGISTER_EVENT("Affine regs write", 0xFFFF00, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_WRITE_AFFINE_REGS);
    DEBUG_REGISTER_EVENT("Set IF.HBlank", 0xFF00FF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_HBLANK_IRQ);
    DEBUG_REGISTER_EVENT("Set IF.VBlank", 0x00FF00, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_VBLANK_IRQ);
    DEBUG_REGISTER_EVENT("Set IF.LY=LYC", 0x00FFFF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_SET_LINECOUNT_IRQ);
    DEBUG_REGISTER_EVENT("Write VRAM", 0x0000FF, DBG_GBA_CATEGORY_PPU, DBG_GBA_EVENT_WRITE_VRAM);

    SET_EVENT_VIEW(this->cpu);
    debugger_report_frame(this->dbg.interface);
}


static void setup_image_view_window(struct GBA* this, struct debugger_interface *dbgr, u32 wnum)
{
    struct debugger_view *dview;
    switch(wnum) {
        case 0:
            this->dbg.image_views.window0 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window0);
            break;
        case 1:
            this->dbg.image_views.window1 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window1);
            break;
        case 2:
            this->dbg.image_views.window2 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window2);
            break;
        case 3:
            this->dbg.image_views.window3 = debugger_view_new(dbgr, dview_image);
            dview = cpg(this->dbg.image_views.window3);
            break;
    }
    struct image_view *iv = &dview->image;

    iv->width = 240;
    iv->height = 160;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 240, 160 };

    debugger_widgets_add_textbox(&dview->options, "enable:%d\nbg0:0  bg1:0  bg2:0  bg3:0  obj:0", 1);

    iv->update_func.ptr = this;
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

void GBAJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 0;
    dbgr->smallest_step = 1;

    //setup_ARM7TDMI_disassembly(dbgr, this);
    setup_events_view(this, dbgr);
    setup_cpu_trace(dbgr, this);
    setup_image_view_window(this, dbgr, 0);
    setup_image_view_window(this, dbgr, 1);
    setup_image_view_window(this, dbgr, 2);
    setup_image_view_window(this, dbgr, 3);
    setup_image_view_bg(this, dbgr, 0);
    setup_image_view_bg(this, dbgr, 1);
    setup_image_view_bg(this, dbgr, 2);
    setup_image_view_bg(this, dbgr, 3);
    setup_image_view_sprites(this, dbgr);
    setup_image_view_tiles(this, dbgr);
    setup_image_view_palettes(this, dbgr);
}
