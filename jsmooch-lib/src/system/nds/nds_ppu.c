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
        if ((this->ppu.io.vcount_at7 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable7) NDS_update_IF7(this, 2);
        if ((this->ppu.io.vcount_at9 == this->clock.ppu.y) && this->ppu.io.vcount_irq_enable9) NDS_update_IF9(this, 2);
    }
    else {
        if (this->ppu.io.hblank_irq_enable7) NDS_update_IF7(this, 1);
        if (this->ppu.io.hblank_irq_enable9) NDS_update_IF9(this, 1);
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
    if (eng_num ^ this->ppu.io.display_swap ^ 1) line_output += (192 * OUT_WIDTH);

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
            u32 y = this->clock.ppu.y & 0xFF;
            if (y == w->top) w->v_flag = 1;
            if (y == w->bottom) w->v_flag = 0;
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
        if (this->ppu.io.vblank_irq_enable7) NDS_update_IF7(this, 0);
        if (this->ppu.io.vblank_irq_enable9) NDS_update_IF9(this, 0);
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

static void calc_screen_sizeA(struct NDS *this, struct NDSENG2D *eng, u32 num) {
    struct NDS_PPU_bg *bg = &eng->bg[num];
    if ((num == 0) && bg->do3d) {
        bg->hpixels = 256;
        bg->vpixels = 192;
        return;
    }

    static const u32 sk[8][4] = {
            {1, 1, 1, 1}, // text  text  text  text
            {1, 1, 1, 2}, // text  text  text  affine
            {1, 1, 2, 2}, // text text affine affine
            {1, 1, 1, 3}, // text text text extend
            {1, 1, 2, 3}, // text text affine extend
            {1, 1, 3, 3}, // text text extend extend
            {1, 0, 4, 0},  // 3d empty big empty
            {0, 0, 0, 0},  // empty empty empty empty
    };
    u32 mk = sk[eng->io.bg_mode][num];
    if (mk == SK_extended) { // "extend" so look at other bits...
        switch(bg->bpp8) {
            case 0:
                mk = SK_rotscale_16bit; // rot/scale with 16bit bgmap entries
                break;
            case 1: {
                u32 b = (bg->character_base_block >> 14) & 1;
                if (!b) {
                    mk = SK_rotscale_8bit;
                }
                else {
                    mk = SK_rotscale_direct;
                }
                break; }
        }
    }
    if ((num == 0) && (mk == 1)) {
        if (bg->do3d) mk = SK_3donly;
    }

    bg->kind = mk;
    if (mk == 0) return;

/*
screen base however NOT used at all for Large screen bitmap mode
  bgcnt size  text     rotscal    bitmap   large bmp
  0           256x256  128x128    128x128  512x1024
  1           512x256  256x256    256x256  1024x512
  2           256x512  512x512    512x256  -
  3           512x512  1024x1024  512x512  - */
    if (mk == SK_text) {
        switch(bg->screen_size) {
            case 0: bg->htiles = 32; bg->vtiles = 32; break;
            case 1: bg->htiles = 64; bg->vtiles = 32; break;
            case 2: bg->htiles = 32; bg->vtiles = 64; break;
            case 3: bg->htiles = 64; bg->vtiles = 64; break;
        }
        bg->hpixels = bg->htiles * 8;
        bg->vpixels = bg->vtiles * 8;
    }
    else if (mk == SK_affine) {
        switch(bg->screen_size) {
            case 0: bg->htiles = 16; bg->vtiles = 16; break;
            case 1: bg->htiles = 32; bg->vtiles = 32; break;
            case 2: bg->htiles = 64; bg->vtiles = 64; break;
            case 3: bg->htiles = 128; bg->vtiles = 128; break;
        }
        bg->hpixels = bg->htiles * 8;
        bg->vpixels = bg->vtiles * 8;
    }
    else if ((mk == SK_rotscale_8bit) || (mk == SK_rotscale_16bit) || (mk == SK_rotscale_direct)) {
        switch(bg->screen_size) {
            case 0: bg->hpixels=128; bg->vpixels=128; break;
            case 1: bg->hpixels=256; bg->vpixels=256; break;
            case 2: bg->hpixels=512; bg->vpixels=256; break;
            case 3: bg->hpixels=512; bg->vpixels=512; break;
        }
    }

    bg->htiles_mask = bg->htiles - 1;
    bg->vtiles_mask = bg->vtiles - 1;
    bg->hpixels_mask = bg->hpixels - 1;
    bg->vpixels_mask = bg->vpixels - 1;
}

static void calc_screen_sizeB(struct NDS *this, struct NDSENG2D *eng, u32 num)
{
    struct NDS_PPU_bg *bg = &eng->bg[num];
    u32 mode = eng->io.bg_mode;
    static const u32 sk[8][4] = {
            { SK_text, SK_text, SK_text, SK_text },
            { SK_text, SK_text, SK_affine, SK_none },
            { SK_none, SK_none, SK_affine, SK_affine},
            { SK_none, SK_none, SK_gba_mode_3, SK_none },
            { SK_none, SK_none, SK_gba_mode_4, SK_none },
            { SK_none, SK_none, SK_gba_mode_5, SK_none },
            { SK_none, SK_none, SK_none, SK_none },
            { SK_none, SK_none, SK_none, SK_none },
    };
    bg->kind = sk[mode][num];
    if (mode >= 3) return;
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
}

static void calc_screen_size(struct NDS *this, struct NDSENG2D *eng, u32 num)
{
    if (eng->num == 0) calc_screen_sizeA(this, eng, num);
    else calc_screen_sizeB(this, eng, num);
}

static void update_bg_x(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, u32 which, u32 val)
{
    i32 v = eng->bg[bgnum].x & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    eng->bg[bgnum].x = v;
    eng->bg[bgnum].x_written = 1;
    //printf("\nline:%d X%d update:%08x", this->clock.ppu.y, bgnum, v);
}

static void update_bg_y(struct NDS *this, struct NDSENG2D *eng, u32 bgnum, u32 which, u32 val)
{
    u32 v = eng->bg[bgnum].y & 0x0FFFFFFF;
    val &= 0xFF;
    switch(which) {
        case 0: v = (v & 0x0FFFFF00) | val; break;
        case 1: v = (v & 0x0FFF00FF) | (val << 8); break;
        case 2: v = (v & 0x0F00FFFF) | (val << 16); break;
        case 3: v = (v & 0x00FFFFFF) | (val << 24); break;
    }
    if ((v >> 27) & 1) v |= 0xF0000000;
    eng->bg[bgnum].y = v;
    eng->bg[bgnum].y_written = 1;
    //printf("\nline:%d Y%d update:%08x", this->clock.ppu.y, bgnum, v);
}


void NDS_PPU_write7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val) {
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];
    switch(addr) {
        case R_DISPSTAT+0:
            this->ppu.io.vblank_irq_enable7 = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable7 = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable7 = (val >> 5) & 1;
            this->ppu.io.vcount_at7 = (this->ppu.io.vcount_at7 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            this->ppu.io.vcount_at7 = (this->ppu.io.vcount_at7 & 0x100) | val;
            return;
    }
    printf("\nUNKNOWN PPU WR7 ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void NDS_PPU_write9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];
    switch(addr) {
        case R_VCOUNT+0: // read-only register...
        case R_VCOUNT+1:
            return;
        case R9_DISP3DCNT+0:
            this->ppu.eng3d.io.texture_mapping = val & 1;
            this->ppu.eng3d.io.polygon_attr_shading = (val >> 1) & 1;
            this->ppu.eng3d.io.alpha_test = (val >> 2) & 1;
            this->ppu.eng3d.io.alpha_blending = (val >> 3) & 1;
            this->ppu.eng3d.io.anti_aliasing = (val >> 4) & 1;
            this->ppu.eng3d.io.edge_marking = (val >> 5) & 1;
            this->ppu.eng3d.io.fog_color_alpha_mode = (val >> 6) & 1;
            this->ppu.eng3d.io.fog_master_enable = (val >> 7) & 1;
            return;
        case R9_DISP3DCNT+1:
            this->ppu.eng3d.io.fog_depth_shift = val & 15;
            // These two are acknowledge
            if ((val >> 4) & 1) this->ppu.eng3d.io.color_buffer_rdlines_underflow = 0;
            if ((val >> 5) & 1) this->ppu.eng3d.io.polygon_vertex_ram_overflow = 0;
            this->ppu.eng3d.io.rear_plane_mode  = (val >> 6) & 1;
            return;

        case R9_DISP3DCNT+2:
        case R9_DISP3DCNT+3: return;

        case R9_DISPCAPCNT+0:
        case R9_DISPCAPCNT+1:
        case R9_DISPCAPCNT+2:
        case R9_DISPCAPCNT+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning DISPCAPCNT not implement!");
                doit = 1;
            }
            return; }
        case R9_DISP_MMEM_FIFO+0:
        case R9_DISP_MMEM_FIFO+1:
        case R9_DISP_MMEM_FIFO+2:
        case R9_DISP_MMEM_FIFO+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning DISP_MMEM_FIFO not implement!");
                doit = 1;
            }
            return; }
        case R9_MASTER_BRIGHT_A+0:
        case R9_MASTER_BRIGHT_A+1:
        case R9_MASTER_BRIGHT_A+2:
        case R9_MASTER_BRIGHT_A+3: {
            static int doit = 0;
            if (!doit) {
                printf("\nwarning MASTER_BRIGHT_A not implement!");
                doit = 1;
            }
            return; }
        case R9_SWAP_BUFFERS:
            this->ppu.eng3d.swap_buffers.on_next_vblank = 1;
            this->ppu.eng3d.swap_buffers.translucent_poly_y_sorting = val & 1;
            this->ppu.eng3d.swap_buffers.depth_buffering = (val >> 1) & 1;

        case R_DISPSTAT+0:
            this->ppu.io.vblank_irq_enable9 = (val >> 3) & 1;
            this->ppu.io.hblank_irq_enable9 = (val >> 4) & 1;
            this->ppu.io.vcount_irq_enable9 = (val >> 5) & 1;
            this->ppu.io.vcount_at9 = (this->ppu.io.vcount_at9 & 0xFF) | ((val & 0x80) << 1);
            return;
        case R_DISPSTAT+1:
            this->ppu.io.vcount_at9 = (this->ppu.io.vcount_at9 & 0x100) | val;
            return;
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
            calc_screen_size(this, eng, 0);
            calc_screen_size(this, eng, 1);
            calc_screen_size(this, eng, 2);
            calc_screen_size(this, eng, 3);
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
                printf("\nWARNING eng1 BAD DISPLAY MODE: %d", eng->io.display_mode);
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

        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            bg->priority = val & 3;
            bg->extrabits = (val >> 4) & 3;
            bg->character_base_block = ((val >> 2) & 3) << 14;
            bg->mosaic_enable = (val >> 6) & 1;
            bg->bpp8 = (val >> 7) & 1;
            if (eng->num == 0) calc_screen_size(this, eng, bgnum);
            return; }
        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: { // BGCNT lo
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            bg->screen_base_block = ((val >> 0) & 31) << 11;
            if (bgnum >= 2) bg->display_overflow = (val >> 5) & 1;
            bg->screen_size = (val >> 6) & 3;
            calc_screen_size(this, eng, bgnum);
            return; }

#define SET16L(thing, to) thing = (thing & 0xFF00) | to
#define SET16H(thing, to) thing = (thing & 0xFF) | (to << 8)

        case R9_BG0HOFS+0: SET16L(eng->bg[0].hscroll, val); return;
        case R9_BG0HOFS+1: SET16H(eng->bg[0].hscroll, val); return;
        case R9_BG0VOFS+0: SET16L(eng->bg[0].vscroll, val); return;
        case R9_BG0VOFS+1: SET16H(eng->bg[0].vscroll, val); return;
        case R9_BG1HOFS+0: SET16L(eng->bg[1].hscroll, val); return;
        case R9_BG1HOFS+1: SET16H(eng->bg[1].hscroll, val); return;
        case R9_BG1VOFS+0: SET16L(eng->bg[1].vscroll, val); return;
        case R9_BG1VOFS+1: SET16H(eng->bg[1].vscroll, val); return;
        case R9_BG2HOFS+0: SET16L(eng->bg[2].hscroll, val); return;
        case R9_BG2HOFS+1: SET16H(eng->bg[2].hscroll, val); return;
        case R9_BG2VOFS+0: SET16L(eng->bg[2].vscroll, val); return;
        case R9_BG2VOFS+1: SET16H(eng->bg[2].vscroll, val); return;
        case R9_BG3HOFS+0: SET16L(eng->bg[3].hscroll, val); return;
        case R9_BG3HOFS+1: SET16H(eng->bg[3].hscroll, val); return;
        case R9_BG3VOFS+0: SET16L(eng->bg[3].vscroll, val); return;
        case R9_BG3VOFS+1: SET16H(eng->bg[3].vscroll, val); return;

#define BG2 2
#define BG3 3
        case R9_BG2PA+0: eng->bg[2].pa = (eng->bg[2].pa & 0xFFFFFF00) | val; return;
        case R9_BG2PA+1: eng->bg[2].pa = (eng->bg[2].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PB+0: eng->bg[2].pb = (eng->bg[2].pb & 0xFFFFFF00) | val; return;
        case R9_BG2PB+1: eng->bg[2].pb = (eng->bg[2].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PC+0: eng->bg[2].pc = (eng->bg[2].pc & 0xFFFFFF00) | val; return;
        case R9_BG2PC+1: eng->bg[2].pc = (eng->bg[2].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2PD+0: eng->bg[2].pd = (eng->bg[2].pd & 0xFFFFFF00) | val; return;
        case R9_BG2PD+1: eng->bg[2].pd = (eng->bg[2].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG2X+0: update_bg_x(this, eng, BG2, 0, val); return;
        case R9_BG2X+1: update_bg_x(this, eng, BG2, 1, val); return;
        case R9_BG2X+2: update_bg_x(this, eng, BG2, 2, val); return;
        case R9_BG2X+3: update_bg_x(this, eng, BG2, 3, val); return;
        case R9_BG2Y+0: update_bg_y(this, eng, BG2, 0, val); return;
        case R9_BG2Y+1: update_bg_y(this, eng, BG2, 1, val); return;
        case R9_BG2Y+2: update_bg_y(this, eng, BG2, 2, val); return;
        case R9_BG2Y+3: update_bg_y(this, eng, BG2, 3, val); return;

        case R9_BG3PA+0: eng->bg[3].pa = (eng->bg[3].pa & 0xFFFFFF00) | val; return;
        case R9_BG3PA+1: eng->bg[3].pa = (eng->bg[3].pa & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PB+0: eng->bg[3].pb = (eng->bg[3].pb & 0xFFFFFF00) | val; return;
        case R9_BG3PB+1: eng->bg[3].pb = (eng->bg[3].pb & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PC+0: eng->bg[3].pc = (eng->bg[3].pc & 0xFFFFFF00) | val; return;
        case R9_BG3PC+1: eng->bg[3].pc = (eng->bg[3].pc & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3PD+0: eng->bg[3].pd = (eng->bg[3].pd & 0xFFFFFF00) | val; return;
        case R9_BG3PD+1: eng->bg[3].pd = (eng->bg[3].pd & 0xFF) | (val << 8) | (((val >> 7) & 1) * 0xFFFF0000); return;
        case R9_BG3X+0: update_bg_x(this, eng, BG3, 0, val); return;
        case R9_BG3X+1: update_bg_x(this, eng, BG3, 1, val); return;
        case R9_BG3X+2: update_bg_x(this, eng, BG3, 2, val); return;
        case R9_BG3X+3: update_bg_x(this, eng, BG3, 3, val); return;
        case R9_BG3Y+0: update_bg_y(this, eng, BG3, 0, val); return;
        case R9_BG3Y+1: update_bg_y(this, eng, BG3, 1, val); return;
        case R9_BG3Y+2: update_bg_y(this, eng, BG3, 2, val); return;
        case R9_BG3Y+3: update_bg_y(this, eng, BG3, 3, val); return;

        case R9_WIN0H:   eng->window[0].right = val; return;
        case R9_WIN0H+1: eng->window[0].left = val; return;
        case R9_WIN1H:   eng->window[1].right = val; return;
        case R9_WIN1H+1: eng->window[1].left = val; return;
        case R9_WIN0V:   eng->window[0].bottom = val; return;
        case R9_WIN0V+1: eng->window[0].top = val; return;
        case R9_WIN1V:   eng->window[1].bottom = val; return;
        case R9_WIN1V+1: eng->window[1].top = val; return;

        case R9_WININ:
            eng->window[0].active[1] = (val >> 0) & 1;
            eng->window[0].active[2] = (val >> 1) & 1;
            eng->window[0].active[3] = (val >> 2) & 1;
            eng->window[0].active[4] = (val >> 3) & 1;
            eng->window[0].active[0] = (val >> 4) & 1;
            eng->window[0].active[5] = (val >> 5) & 1;
            return;
        case R9_WININ+1:
            eng->window[1].active[1] = (val >> 0) & 1;
            eng->window[1].active[2] = (val >> 1) & 1;
            eng->window[1].active[3] = (val >> 2) & 1;
            eng->window[1].active[4] = (val >> 3) & 1;
            eng->window[1].active[0] = (val >> 4) & 1;
            eng->window[1].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT:
            eng->window[NDS_WINOUTSIDE].active[1] = (val >> 0) & 1;
            eng->window[NDS_WINOUTSIDE].active[2] = (val >> 1) & 1;
            eng->window[NDS_WINOUTSIDE].active[3] = (val >> 2) & 1;
            eng->window[NDS_WINOUTSIDE].active[4] = (val >> 3) & 1;
            eng->window[NDS_WINOUTSIDE].active[0] = (val >> 4) & 1;
            eng->window[NDS_WINOUTSIDE].active[5] = (val >> 5) & 1;
            return;
        case R9_WINOUT+1:
            eng->window[NDS_WINOBJ].active[1] = (val >> 0) & 1;
            eng->window[NDS_WINOBJ].active[2] = (val >> 1) & 1;
            eng->window[NDS_WINOBJ].active[3] = (val >> 2) & 1;
            eng->window[NDS_WINOBJ].active[4] = (val >> 3) & 1;
            eng->window[NDS_WINOBJ].active[0] = (val >> 4) & 1;
            eng->window[NDS_WINOBJ].active[5] = (val >> 5) & 1;
            return;
        case R9_MOSAIC+0:
            eng->mosaic.bg.hsize = (val & 15) + 1;
            eng->mosaic.bg.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_MOSAIC+1:
            eng->mosaic.obj.hsize = (val & 15) + 1;
            eng->mosaic.obj.vsize = ((val >> 4) & 15) + 1;
            return;
        case R9_BLDCNT:
            eng->blend.mode = (val >> 6) & 3;
            // sp, bg0, bg1, bg2, bg3, bd
            eng->blend.targets_a[0] = (val >> 4) & 1;
            eng->blend.targets_a[1] = (val >> 0) & 1;
            eng->blend.targets_a[2] = (val >> 1) & 1;
            eng->blend.targets_a[3] = (val >> 2) & 1;
            eng->blend.targets_a[4] = (val >> 3) & 1;
            eng->blend.targets_a[5] = (val >> 5) & 1;
            return;
        case R9_BLDCNT+1:
            eng->blend.targets_b[0] = (val >> 4) & 1;
            eng->blend.targets_b[1] = (val >> 0) & 1;
            eng->blend.targets_b[2] = (val >> 1) & 1;
            eng->blend.targets_b[3] = (val >> 2) & 1;
            eng->blend.targets_b[4] = (val >> 3) & 1;
            eng->blend.targets_b[5] = (val >> 5) & 1;
            return;

        case R9_BLDALPHA:
            eng->blend.eva_a = val & 31;
            eng->blend.use_eva_a = eng->blend.eva_a;
            if (eng->blend.use_eva_a > 16) eng->blend.use_eva_a = 16;
            return;
        case R9_BLDALPHA+1:
            eng->blend.eva_b = val & 31;
            eng->blend.use_eva_b = eng->blend.eva_b;
            if (eng->blend.use_eva_b > 16) eng->blend.use_eva_b = 16;
            return;
        case R9_BLDY+0:
            eng->blend.bldy = val & 31;
            eng->blend.use_bldy = eng->blend.bldy;
            if (eng->blend.use_bldy > 16) eng->blend.use_bldy = 16;
            return;
        case R9_BLDY+1:
            return;
        case 0x0400004e: // not used
        case 0x0400004f: // not used
        case 0x04000056: // not used
        case 0x04000057: // not used
        case 0x04000058: // not used
        case 0x04000059: // not used
        case 0x0400005A: // not used
        case 0x0400005B: // not used
        case 0x0400005C: // not used
        case 0x0400005D: // not used
        case 0x0400005E: // not used
        case 0x0400005F: // not used
            return;
    }
    printf("\nUNKNOWN PPU WR9 ADDR:%08x sz:%d VAL:%08x", addr, sz, val);
}

void NDS_PPU_write9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    NDS_PPU_write9_io8(this, addr, sz, access, val & 0xFF);
    if (sz >= 2) NDS_PPU_write9_io8(this, addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        NDS_PPU_write9_io8(this, addr+2, sz, access, (val >> 16) & 0xFF);
        NDS_PPU_write9_io8(this, addr+3, sz, access, (val >> 24) & 0xFF);
    }
}

u32 NDS_PPU_read9_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 en = 0;
    if (addr >= 0x04001000) {
        addr -= 0x1000;
        en = 1;
    }
    struct NDSENG2D *eng = &this->ppu.eng2d[en];

    u32 v;

    switch(addr) {
        case R9_DISPCNT+0:
            v = eng->io.bg_mode;
            v |= eng->bg[0].do3d << 3;
            v |= eng->io.tile_obj_map_1d << 4;
            v |= eng->io.bitmap_obj_2d_dim << 5;
            v |= eng->io.bitmap_obj_map_1d << 6;
            v |= eng->io.force_blank << 7;
            return v;
        case R9_DISPCNT+1:
            v = eng->bg[0].enable;
            v |= eng->bg[1].enable << 1;
            v |= eng->bg[2].enable << 2;
            v |= eng->bg[3].enable << 3;
            v |= eng->obj.enable << 4;
            v |= eng->window[0].enable << 5;
            v |= eng->window[1].enable << 6;
            v |= eng->window[NDS_WINOBJ].enable << 7;
            return v;
        case R9_DISPCNT+2:
            v = eng->io.display_mode;
            v |= eng->io.tile_obj_id_boundary << 4;
            v |= eng->io.hblank_free << 7;
            if (en == 0) {
                v |= this->ppu.io.display_block << 2;
                v |= eng->io.bitmap_obj_id_boundary << 6;
            }
            return v;
        case R9_DISPCNT+3:
            v = eng->io.bg_extended_palettes << 6;
            v |= eng->io.obj_extended_palettes << 7;
            if (en == 0) {
                v |= eng->io.character_base >> 16;
                v |= eng->io.screen_base >> 13;
            }
            return v;
        case R_DISPSTAT+0: // DISPSTAT lo
            v = this->clock.ppu.vblank_active;
            v |= this->clock.ppu.hblank_active << 1;
            v |= (this->clock.ppu.y == this->ppu.io.vcount_at9) << 2;
            v |= this->ppu.io.vblank_irq_enable9 << 3;
            v |= this->ppu.io.hblank_irq_enable9 << 4;
            v |= this->ppu.io.vcount_irq_enable9 << 5;
            v |= (this->ppu.io.vcount_at9 & 0x100) >> 1; // hi bit of vcount_at
            return v;
        case R_DISPSTAT+1: // DISPSTAT hi
            v = this->ppu.io.vcount_at9 & 0xFF;
            return v;
        case R_VCOUNT+0: // VCOunt lo
            return this->clock.ppu.y & 0xFF;
        case R_VCOUNT+1:
            return this->clock.ppu.y >> 8;

        case R9_BG0CNT+0:
        case R9_BG1CNT+0:
        case R9_BG2CNT+0:
        case R9_BG3CNT+0: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            v = bg->priority;
            v |= bg->extrabits << 4;
            v |= (bg->character_base_block >> 12);
            v |= bg->mosaic_enable << 6;
            v |= bg->bpp8 << 7;
            return v; }

        case R9_BG0CNT+1:
        case R9_BG1CNT+1:
        case R9_BG2CNT+1:
        case R9_BG3CNT+1: {
            u32 bgnum = (addr & 0b0110) >> 1;
            struct NDS_PPU_bg *bg = &eng->bg[bgnum];
            v = bg->screen_base_block >> 11;
            v |= bg->display_overflow << 5;
            v |= bg->screen_size << 6;
            return v; }

        case R9_WININ+0:
            v = eng->window[0].active[1] << 0;
            v |= eng->window[0].active[2] << 1;
            v |= eng->window[0].active[3] << 2;
            v |= eng->window[0].active[4] << 3;
            v |= eng->window[0].active[0] << 4;
            v |= eng->window[0].active[5] << 5;
            return v;
        case R9_WININ+1:
            v = eng->window[1].active[1] << 0;
            v |= eng->window[1].active[2] << 1;
            v |= eng->window[1].active[3] << 2;
            v |= eng->window[1].active[4] << 3;
            v |= eng->window[1].active[0] << 4;
            v |= eng->window[1].active[5] << 5;
            return v;
        case R9_WINOUT+0:
            v = eng->window[NDS_WINOUTSIDE].active[1] << 0;
            v |= eng->window[NDS_WINOUTSIDE].active[2] << 1;
            v |= eng->window[NDS_WINOUTSIDE].active[3] << 2;
            v |= eng->window[NDS_WINOUTSIDE].active[4] << 3;
            v |= eng->window[NDS_WINOUTSIDE].active[0] << 4;
            v |= eng->window[NDS_WINOUTSIDE].active[5] << 5;
            return v;
        case R9_WINOUT+1:
            v = eng->window[NDS_WINOBJ].active[1] << 0;
            v |= eng->window[NDS_WINOBJ].active[2] << 1;
            v |= eng->window[NDS_WINOBJ].active[3] << 2;
            v |= eng->window[NDS_WINOBJ].active[4] << 3;
            v |= eng->window[NDS_WINOBJ].active[0] << 4;
            v |= eng->window[NDS_WINOBJ].active[5] << 5;
            return v;
        case R9_BLDCNT+0:
            v = eng->blend.targets_a[0] << 4;
            v |= eng->blend.targets_a[1] << 0;
            v |= eng->blend.targets_a[2] << 1;
            v |= eng->blend.targets_a[3] << 2;
            v |= eng->blend.targets_a[4] << 3;
            v |= eng->blend.targets_a[5] << 5;
            v |= eng->blend.mode << 6;
            return v;
        case R9_BLDCNT+1:
            v = eng->blend.targets_b[0] << 4;
            v |= eng->blend.targets_b[1] << 0;
            v |= eng->blend.targets_b[2] << 1;
            v |= eng->blend.targets_b[3] << 2;
            v |= eng->blend.targets_b[4] << 3;
            v |= eng->blend.targets_b[5] << 5;
            return v;
        case R9_BLDALPHA+0:
            return eng->blend.eva_a;
        case R9_BLDALPHA+1:
            return eng->blend.eva_b;

    }

    printf("\nUNKNOWN PPU RD9 ADDR:%08x sz:%d", addr, sz);
    return 0;
}

u32 NDS_PPU_read7_io8(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v;
   switch(addr) {
       case R_DISPSTAT+0: // DISPSTAT lo
           v = this->clock.ppu.vblank_active;
           v |= this->clock.ppu.hblank_active << 1;
           v |= (this->clock.ppu.y == this->ppu.io.vcount_at7) << 2;
           v |= this->ppu.io.vblank_irq_enable7 << 3;
           v |= this->ppu.io.hblank_irq_enable7 << 4;
           v |= this->ppu.io.vcount_irq_enable7 << 5;
           v |= (this->ppu.io.vcount_at7 & 0x100) >> 1; // hi bit of vcount_at
           return v;
       case R_DISPSTAT+1: // DISPSTAT hi
           v = this->ppu.io.vcount_at7 & 0xFF;
           return v;
       case R_VCOUNT+0: // VCOunt lo
           return this->clock.ppu.y & 0xFF;
       case R_VCOUNT+1:
           return this->clock.ppu.y >> 8;
   }
   printf("\nUnknown PPURD7IO8 addr:%08x", addr);
   return 0;
}

u32 NDS_PPU_read7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = NDS_PPU_read7_io8(this, addr, sz, access, has_effect);
    if (sz >= 2) v |= NDS_PPU_read7_io8(this, addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= NDS_PPU_read7_io8(this, addr+2, sz, access, has_effect) << 16;
        v |= NDS_PPU_read7_io8(this, addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

u32 NDS_PPU_read9_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 has_effect)
{
    u32 v = NDS_PPU_read9_io8(this, addr, sz, access, has_effect);
    if (sz >= 2) v |= NDS_PPU_read9_io8(this, addr+1, sz, access, has_effect) << 8;
    if (sz == 4) {
        v |= NDS_PPU_read9_io8(this, addr+2, sz, access, has_effect) << 16;
        v |= NDS_PPU_read9_io8(this, addr+3, sz, access, has_effect) << 24;
    }
    return v;
}

void NDS_PPU_write7_io(struct NDS *this, u32 addr, u32 sz, u32 access, u32 val)
{
    NDS_PPU_write7_io8(this, addr, sz, access, val & 0xFF);
    if (sz >= 2) NDS_PPU_write7_io8(this, addr+1, sz, access, (val >> 8) & 0xFF);
    if (sz == 4) {
        NDS_PPU_write7_io8(this, addr+2, sz, access, (val >> 16) & 0xFF);
        NDS_PPU_write7_io8(this, addr+3, sz, access, (val >> 24) & 0xFF);
    }
}
