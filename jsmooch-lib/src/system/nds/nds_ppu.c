//
// Created by . on 1/18/25.
//
#include <string.h>

#include "helpers/multisize_memaccess.c"

#include "nds_bus.h"
#include "nds_dma.h"
#include "nds_regs.h"
#include "nds_irq.h"

#define NDS_WIN0 0
#define NDS_WIN1 1
#define NDS_WINOBJ 2
#define NDS_WINOUTSIDE 3

void NDS_PPU_init(struct NDS *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));

    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
        p->num = en;
        p->mosaic.bg.hsize = p->mosaic.bg.vsize = 1;
        p->mosaic.obj.hsize = p->mosaic.obj.vsize = 1;
        p->bg[2].pa = 1 << 8; p->bg[2].pd = 1 << 8;
        p->bg[3].pa = 1 << 8; p->bg[3].pd = 1 << 8;
    }
}

void NDS_PPU_delete(struct NDS *this)
{
    
}

void NDS_PPU_reset(struct NDS *this)
{
    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
        p->mosaic.bg.hsize = p->mosaic.bg.vsize = 1;
        p->mosaic.obj.hsize = p->mosaic.obj.vsize = 1;
        p->bg[2].pa = 1 << 8; p->bg[2].pd = 1 << 8;
        p->bg[3].pa = 1 << 8; p->bg[3].pd = 1 << 8;
    }
}

static void hblank(struct NDS *this, u32 val)
{
    this->clock.ppu.hblank_active = 1;
    if (val == 0) { // beginning of scanline
        if ((this->ppu.io.vcount_at == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable) NDS_update_IFs(this, 2);
    }
    else {
        if (this->ppu.io.hblank_irq_enable) NDS_update_IFs(this, 1);
    }
}

void NDS_PPU_start_scanline(struct NDS *this) // Called on scanline start
{
    if (this->dbg.events.view.vec) {
        debugger_report_line(this->dbg.interface, this->clock.ppu.y);
    }
    hblank(this, 0);
    this->clock.ppu.scanline_start = this->clock.master_cycle_count7;
    struct NDS_PPU_bg *bg;

    if (this->clock.ppu.y == 0) { // Reset some mosaic counter
        for (u32 pn = 0; pn < 2; pn++) {
            struct NDSENG2D *p = &this->ppu.eng2d[pn];
            for (u32 i = 0; i < 4; i++) {
                bg = &p->bg[i];
                bg->mosaic_y = 0;
                bg->last_y_rendered = 159;
            }
            this->ppu.mosaic.bg.y_counter = 0;
            this->ppu.mosaic.bg.y_current = 0;
            this->ppu.mosaic.obj.y_counter = 0;
            this->ppu.mosaic.obj.y_current = 0;

            for (u32 bgnum = 0; bgnum < 4; bgnum++) {
                for (u32 line = 0; line < 1024; line++) {
                    memset(&this->dbg_info.bg_scrolls[bgnum].lines[0], 0, 1024 * 128);
                }
            }
        }
    }
    if (this->clock.ppu.y < 192) {
        /*struct NDS_DBG_line *dbgl = &this->dbg_info.line[this->clock.ppu.y];
        for (u32 i = 2; i < 4; i++) {
            bg = &this->ppu.bg[i];
            struct NDS_DBG_line_bg *dbgbg = &dbgl->bg[i];
            if ((this->clock.ppu.y == 0) || bg->x_written) {
                bg->x_lerp = bg->x;
                dbgbg->reset_x = 1;
            }
            dbgbg->x_lerp = bg->x_lerp;

            if ((this->clock.ppu.y == 0) || (bg->y_written)) {
                bg->y_lerp = bg->y;
                dbgbg->reset_y = 1;
            }
            dbgbg->y_lerp = bg->y_lerp;
            bg->x_written = bg->y_written = 0;
        }*/
    }
}

#define OUT_WIDTH 256

static void draw_line(struct NDS *this, u32 eng_num)
{
    struct NDSENG2D *eng = &this->ppu.eng2d[eng_num];
    struct NDS_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
    l->bg_mode = eng->io.bg_mode;
    // We have 2-4 display modes. They can be: WHITE, NORMAL, VRAM display, and "main memory display"
    // During this time, the 2d engine runs like normal!
    // So we will draw our lines...
    switch(eng->io.bg_mode) {
        // TODO: this
    }

    // Then we will pixel output it to the screen...
    u16 *line_output = this->ppu.cur_output + (this->clock.ppu.y * OUT_WIDTH);
    if (eng_num ^ this->ppu.io.display_swap) line_output += (192 * OUT_WIDTH);

    u32 display_mode = eng->io.display_mode;
    if (eng_num == 1) display_mode &= 1;
    switch(display_mode) {
        case 0: // WHITE!
            memset(line_output, 0xFF, 2*OUT_WIDTH);
            break;
        case 1: // NORMAL!
            memcpy(line_output, eng->line_px, 2*OUT_WIDTH);
            break;
        case 2: { // VRAM!
            u32 block_num = this->ppu.io.display_block << 17;
            u16 *line_input = (u16 *)(this->mem.vram.data + block_num);
            line_input += this->clock.ppu.y << 8; // * 256
            memcpy(line_output,  line_input, 2*OUT_WIDTH);
            break; }
        case 3: { //Main RAM
            u16 *line_input = (u16 *)this->mem.RAM;
            line_input += this->clock.ppu.y << 8; // * 256
            memcpy(line_output,  line_input, 2*OUT_WIDTH);
            break; }
    }
}

static void calculate_windows_vflags(struct NDS *this)
{
    for (u32 ppun = 0; ppun < 2; ppun++) {
        struct NDSENG2D *p = &this->ppu.eng2d[ppun];
        for (u32 wn = 0; wn < 2; wn++) {
            struct NDS_PPU_window *w = &p->window[wn];
            if (this->clock.ppu.y == w->top) w->v_flag = 1;
            if (this->clock.ppu.y == w->bottom) w->v_flag = 0;
        }
    }
}

void NDS_PPU_hblank(struct NDS *this) // Called at hblank time
{
    // Now draw line!
    if (this->clock.ppu.y < 192) {
        draw_line(this, 0);
        draw_line(this, 1);
    }
    else {
        calculate_windows_vflags(this);
    }
    hblank(this, 1);
    
    NDS_check_dma9_at_hblank(this);
}

static void vblank(struct NDS *this, u32 val)
{
    this->clock.ppu.vblank_active = val;
    if (val) {
        if (this->ppu.io.vblank_irq_enable) NDS_update_IFs(this, 0);
    }
}

static void new_frame(struct NDS *this)
{
    //debugger_report_frame(this->dbg.interface);
    this->clock.ppu.y = 0;
    this->ppu.cur_output = ((u16 *)this->ppu.display->output[this->ppu.display->last_written ^ 1]);
    this->ppu.display->last_written ^= 1;
    this->clock.master_frame++;
    vblank(this, 0);
}

void NDS_PPU_finish_scanline(struct NDS *this) // Called on scanline end, to render and do housekeeping
{
    this->clock.ppu.hblank_active = 0;
    this->clock.ppu.y++;

    if (this->clock.ppu.y == 192) {
        vblank(this, 1);
        NDS_check_dma7_at_vblank(this);
        NDS_check_dma9_at_vblank(this);
    }
    if (this->clock.ppu.y == 263) {
        this->clock.ppu.vblank_active = 0;
        new_frame(this);
    }

}

u32 NDS_PPU_read_2d_bg_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz)
{
    return cR[sz](this->ppu.eng2d[eng_num].mem.bg_palette, addr);
}

u32 NDS_PPU_read_2d_obj_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz)
{
    return cR[sz](this->ppu.eng2d[eng_num].mem.obj_palette, addr);
}

void NDS_PPU_write_2d_bg_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz, u32 val)
{
    cW[sz](this->ppu.eng2d[eng_num].mem.bg_palette, addr, val);
}

void NDS_PPU_write_2d_obj_palette(struct NDS *this, u32 eng_num, u32 addr, u32 sz, u32 val)
{
    cW[sz](this->ppu.eng2d[eng_num].mem.obj_palette, addr, val);
}

static void calc_screen_sizes(struct NDS *this, struct NDSENG2D *eng, u32 num)
{
    u32 mode = eng->io.bg_mode;
    printf("\nIMPLEMENT calc_screen_sizes");
    /*
    if (mode >= 3) return;
    struct GBA_PPU_bg *bg = &this->ppu.bg[num];
    // mode0 they're all the same no scaling
    u32 scales = mode >= 1 ? (num > 1) :  0;
    // mode1 bg 0-1 no scaling, 2 scaling
    // mode2 is 2 and 3
#define T(scales, ssize, hrs, vrs) case (((scales) << 2) | (ssize)): bg->htiles = hrs; bg->vtiles = vrs; break
    switch((scales << 2) | bg->screen_size) {
        T(0, 0, 32, 32);
        T(1, 0, 16, 16);
        T(0, 1, 64, 32);
        T(1, 1, 32, 32);
        T(0, 2, 32, 64);
        T(1, 2, 64, 64);
        T(0, 3, 64, 64);
        T(1, 3, 128, 128);
    }
#undef T
    bg->htiles_mask = bg->htiles - 1;
    bg->vtiles_mask = bg->vtiles - 1;
    bg->hpixels = bg->htiles << 3;
    bg->vpixels = bg->vtiles << 3;
    bg->hpixels_mask = bg->hpixels - 1;
    bg->vpixels_mask = bg->vpixels - 1;
}*/

}

void NDS_PPU_write_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    u32 en = 0;
    if (addr >= 0x04010000) {
        addr -= 0x10000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];
    switch(addr) {
        case R9_DISPCNT+0:
            if ((val & 7) != eng->io.bg_mode) {
                printf("\neng%d NEW BG MODE:%d", en, val & 7);
            }
            eng->io.bg_mode = val & 7;
            if (en == 0) eng->bg[0].do3d = (val >> 3) & 1;
            eng->io.tile_obj_map_1d = (val >> 4) & 1;
            eng->io.bitmap_obj_2d_dim = (val >> 5) & 1;
            eng->io.bitmap_obj_map_1d = (val >> 6) & 1;
            eng->io.force_blank = (val >> 7) & 1;
            calc_screen_sizes(this, eng, 0);
            calc_screen_sizes(this, eng, 1);
            calc_screen_sizes(this, eng, 2);
            calc_screen_sizes(this, eng, 3);
            return;
        case R9_DISPCNT+1:
            eng->bg[0].enable = (val >> 0) & 1;
            eng->bg[1].enable = (val >> 1) & 1;
            eng->bg[2].enable = (val >> 2) & 1;
            eng->bg[3].enable = (val >> 3) & 1;
            eng->obj.enable = (val >> 4) & 1;
            eng->window[0].enable = (val >> 5) & 1;
            eng->window[1].enable = (val >> 6) & 1;
            eng->window[NDS_WINOBJ].enable = (val >> 7) & 1;
            return;
        case R9_DISPCNT+2:
            if (eng->io.display_mode != (val & 3)) {
                printf("\neng%d NEW DISPLAY MODE: %d", en, val & 3);
            }
            eng->io.display_mode = val & 3;
            if ((en == 1) && (eng->io.display_mode > 1)) {
                printf("\nWARNING eng1 BAD DISPLAY MODE: %d", this->ppu.io.display_mode);
            }
            eng->io.tile_obj_id_boundary = (val >> 4) & 3;
            eng->io.hblank_free = (val >> 7) & 1;
            if (en == 0) {
                this->ppu.io.display_block = (val >> 2) & 3;
                eng->io.bitmap_obj_id_boundary = (val >> 6) & 1;
            }
            return;
        case R9_DISPCNT+3:
            if (en == 0) {
                eng->io.character_base = (val & 7) << 16;
                eng->io.screen_base = ((val >> 3) & 7) << 16;
            }
            eng->io.bg_extended_palettes = (val >> 6) & 1;
            eng->io.obj_extended_palettes = (val >> 7) & 1;
            return;
    }
    printf("\nUNKNOWN PPU WR ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void NDS_PPU_write_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    NDS_PPU_write_io8(this, addr, sz, access, val & 0xFF);
    if (sz >= 2) NDS_PPU_write_io8(this, addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        NDS_PPU_write_io8(this, addr+2, sz, access, (val >> 16) & 0xFF);
        NDS_PPU_write_io8(this, addr+3, sz, access, (val >> 24) & 0xFF);
    }
}

u32 NDS_PPU_read_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    printf("\nUNKNOWN PPU RD ADDR:%08x sz:%d", addr, sz);
}

u32 NDS_PPU_read_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = NDS_PPU_read_io8(this, addr, sz, access, has_effect);
    if (sz >= 2) v |= NDS_PPU_read_io8(this, addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= NDS_PPU_read_io8(this, addr+2, sz, access, has_effect) << 16;
        v |= NDS_PPU_read_io8(this, addr+3, sz, access, has_effect) << 24;
    }
    return v;
}
