//
// Created by . on 1/18/25.
//
#include <string.h>

#include "helpers/multisize_memaccess.c"

#include "nds_bus.h"
#include "nds_dma.h"
#include "nds_irq.h"

void NDS_PPU_init(struct NDS *this)
{
    memset(&this->ppu, 0, sizeof(this->ppu));

    for (u32 en=0; en<2; en++) {
        struct NDSENG2D *p = &this->ppu.eng2d[en];
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
    struct NDSENG2D *p = &this->ppu.eng2d[eng_num];
    struct NDS_DBG_line *l = &this->dbg_info.line[this->clock.ppu.y];
    l->bg_mode = p->io.bg_mode;
    /*for (u32 bgn = 0; bgn < 4; bgn++) {
        struct NDS_PPU_bg *bg = &p->bg[bgn];
        struct NDS_DBG_line_bg *mdbg = &l->bg[bgn];
        mdbg->htiles = bg->htiles;
        mdbg->vtiles = bg->vtiles;
        mdbg->hscroll = bg->hscroll;
        mdbg->vscroll = bg->vscroll;
        mdbg->display_overflow = bg->display_overflow;
        mdbg->screen_base_block = bg->screen_base_block;
        mdbg->character_base_block = bg->character_base_block;
        mdbg->bpp8 = bg->bpp8;
        mdbg->priority = bg->priority;
        if (bgn >= 2) {
            mdbg->hpos = bg->x;
            mdbg->vpos = bg->y;
            mdbg->pa = bg->pa;
            mdbg->pb = bg->pb;
            mdbg->pc = bg->pc;
            mdbg->pd = bg->pd;
        }
    }*/

    // Actually draw the line
    u16 *line_output = this->ppu.cur_output + (192 * OUT_WIDTH * eng_num) + (this->clock.ppu.y * OUT_WIDTH);
    if (p->io.force_blank) {
        memset(line_output, 0xFF, 480);
        return;
    }
    memset(line_output, 0, 480);
    switch (p->io.bg_mode) {
        default:
            assert(1 == 2);
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
