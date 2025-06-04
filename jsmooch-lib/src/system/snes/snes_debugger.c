//
// Created by . on 4/23/25.
//

#include "snes_debugger.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "helpers/color.h"
#include "snes_bus.h"
#include "component/cpu/wdc65816/wdc65816_disassembler.h"
#include "component/cpu/spc700/spc700_disassembler.h"

#define JTHIS struct SNES* this = (struct SNES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this
#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void render_image_view_tilemaps(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct SNES *this = (struct SNES *) ptr;
    if (this->clock.master_frame == 0) return;
    struct debugger_widget_radiogroup *layernum = &((struct debugger_widget *) cvec_get(&dview->options,
                                                                                        0))->radiogroup;
    u32 ln = layernum->value;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 1024);

    for (u32 y = 0; y < 224; y++) {
    }
}

static void print_layer_info(struct SNES *this, u32 bgnum, struct debugger_widget_textbox *tb)
{
    if (bgnum < 4) {
        struct SNES_PPU_BG *bg = &this->ppu.bg[bgnum];
        debugger_widgets_textbox_sprintf(tb, "\n-BG%d:  bgmode:%d  ", bgnum, this->ppu.io.bg_mode);
        //if (this->ppu.io.bg_mode == 7) {
            debugger_widgets_textbox_sprintf(tb, "  dc:%d  ", this->ppu.color_math.direct_color);
        //}
        if (bg->main_enable) debugger_widgets_textbox_sprintf(tb, "main:on ");
        else debugger_widgets_textbox_sprintf(tb, "main:off");
        if (bg->sub_enable) debugger_widgets_textbox_sprintf(tb, " sub:on ");
        else debugger_widgets_textbox_sprintf(tb, " sub:off");

        debugger_widgets_textbox_sprintf(tb, "  bpp:");
        switch(bg->tile_mode) {
            case SPTM_BPP2:
                debugger_widgets_textbox_sprintf(tb, "2  ");
                break;
            case SPTM_BPP4:
                debugger_widgets_textbox_sprintf(tb, "4  ");
                break;
            case SPTM_BPP8:
                debugger_widgets_textbox_sprintf(tb, "8  ");
                break;
            case SPTM_mode7:
                debugger_widgets_textbox_sprintf(tb, "m7 ");
                break;
        }

        debugger_widgets_textbox_sprintf(tb, "  mosaic:%d", bg->mosaic.enable);
        debugger_widgets_textbox_sprintf(tb, "\n---hscroll:%d  vscroll:%d  scr_adr:%04x  tile_addr:%04x  pal_base:%d", bg->io.hoffset, bg->io.voffset, bg->io.screen_addr, bg->io.tiledata_addr, this->ppu.io.bg_mode == 0 ? bgnum << 5 : 0);
        //debugger_widgets_textbox_sprintf(tb, "  priority:%",  bg->priority);

    }
}

static void render_image_view_obj_tiles(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct SNES *this = (struct SNES *) ptr;
    if (this->clock.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 256);

    u32 colormode = ((struct debugger_widget *) cvec_get(&dview->options,0))->radiogroup.value;

    // 16x32 tiles per thing
    // 4bpp, 8 pixels = 32 bits
    // addr and addr+8

    u32 x_start = 0;
    for (u32 base_addr = 0; base_addr < 0x8000; base_addr += 0x2000) {
        for (u32 row = 0; row < 32; row++) {
            for (u32 col = 0; col < 16; col++) {
                u32 tile_addr = base_addr + (((row << 4) | col) << 4);
                u32 screen_x = x_start + (col * 8);
                u32 base_y = row * 8;

                // 8 lines now
                for (u32 inner_y = 0; inner_y < 8; inner_y++) {
                    u32 screen_y = inner_y + base_y;
                    u32 get_addr = (tile_addr & 0x7FF0) + inner_y;
                    u32 data = this->ppu.VRAM[get_addr] | (this->ppu.VRAM[(get_addr + 8) & 0x7FFF] << 16);
                    u32 *bufpos = outbuf + (screen_y * out_width) + screen_x;
                    for (u32 mpx = 0; mpx < 8; mpx++) {
                        u32 color = 0;
                        u32 c = (data >> mpx) & 1;
                        c += (data >> (mpx + 7)) & 2;
                        c += (data >> (mpx + 14)) & 4;
                        c += (data >> (mpx + 21)) & 8;
                        if (c == 0) {
                            //
                        }
                        else { // c range here is 1-15
                            switch(colormode) {
                                default: {
                                    static int a = 0;
                                    if (a != colormode) {
                                        printf("\nUNSUPPORT COLORMODE %d", colormode);
                                        a = colormode;
                                    }}
                                    __attribute__ ((fallthrough));
                                case 0: {// B&W
                                    float r = ((float)c / 15.0f) * 255.0f;
                                    c = (u32)r;
                                    if (c > 0xFF) c = 0xFF;
                                    color = (c << 16) | (c << 8) | c;
                                    break; }

                                case 1:
                                case 2:
                                case 3:
                                case 4:
                                case 5:
                                case 6:
                                case 7:
                                case 8: {
                                    u32 palnum = 8 + (colormode - 1);
                                    c = (palnum << 4) | c;
                                    color = gba_to_screen(this->ppu.CGRAM[c]);
                                    break;
                                }
                            }
                        }


                        bufpos[mpx] = 0xFF000000 | color;
                    }
                }
            }
        }

        x_start += (128 + 10);
    }
}

static void render_image_view_ppu_layers(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct SNES *this = (struct SNES *) ptr;
    if (this->clock.master_frame == 0) return;
    struct debugger_widget_radiogroup *layernum = &((struct debugger_widget *) cvec_get(&dview->options,
                                                                                        0))->radiogroup;
    struct debugger_widget_radiogroup *attrkind = &((struct debugger_widget *) cvec_get(&dview->options,
                                                                                        1))->radiogroup;

    static const u32 tm_colors[8] = {
            0xFF000000, // no texture/color
            0xFFFF0000, // color 1, blue
            0xFF0000FF, // color 2, red
            0xFF00FF00, // color 3, green
            0xFFFFFF00, // color 4, teal
            0xFFFF00FF, // color 5, purple
            0xFF00FFFF, // color 6, yellow
            0xFFFFFFFF, // color 7, white
    };

    struct debugger_widget_textbox *tb = &((struct debugger_widget *) cvec_get(&dview->options, 2))->textbox;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 224);

    debugger_widgets_textbox_clear(tb);

    struct debugger_widget_colorkey *colorkey = &((struct debugger_widget *)cvec_get(&dview->options, 3))->colorkey;
    print_layer_info(this, layernum->value, tb);

    switch(attrkind->value) {
        case 0: // Color output
            debugger_widgets_colorkey_set_title(colorkey, "Output");
            break;
        case 1: // Has
            debugger_widgets_colorkey_set_title(colorkey, "Has: white");
            break;
        case 2: // Priority
            debugger_widgets_colorkey_set_title(colorkey, "Priority");
            debugger_widgets_colorkey_add_item(colorkey, "No pixel", tm_colors[0]);
            debugger_widgets_colorkey_add_item(colorkey, "Prio 0", tm_colors[1]);
            debugger_widgets_colorkey_add_item(colorkey, "Prio 1", tm_colors[2]);
            debugger_widgets_colorkey_add_item(colorkey, "Prio 2", tm_colors[3]);
            debugger_widgets_colorkey_add_item(colorkey, "Prio 3", tm_colors[4]);
            break;
        case 3: // BPP
            debugger_widgets_colorkey_set_title(colorkey, "BPP");
            debugger_widgets_colorkey_add_item(colorkey, "No pixel", tm_colors[0]);
            debugger_widgets_colorkey_add_item(colorkey, "2BPP", tm_colors[1]);
            debugger_widgets_colorkey_add_item(colorkey, "4BPP", tm_colors[2]);
            debugger_widgets_colorkey_add_item(colorkey, "8BPP", tm_colors[3]);
            break;
    }


    for (u32 y = 0; y < 224; y++) {
        u32 *out_line = outbuf + (y * out_width);
        union SNES_PPU_px *px_line;
        if (layernum->value < 4) {
            px_line = this->dbg_info.line[y].bg[layernum->value].px;
        }
        else {
            px_line = this->dbg_info.line[y].sprite_px;
        }
        for (u32 x = 0; x < 256; x++) {
            switch(attrkind->value) {
                case 0: // color
                    if (px_line[x].has) {
                        u32 c = px_line[x].color;
                        out_line[x] = gba_to_screen(c);
                    }
                    else {
                        out_line[x] = 0xFF000000;
                    }
                    break;
                case 1: // has
                    out_line[x] = 0xFF000000 | (px_line[x].has * 0xFFFFFF);
                    break;
                case 2: { // priority
                    if (px_line[x].has) {
                        u32 v = px_line[x].dbg_priority + 1;
                        out_line[x] = tm_colors[v];
                    }
                    else {
                        out_line[x] = 0xFF000000;
                    }
                    break; }
                case 3: { // BPP
                    if (px_line[x].has) {
                        u32 v = px_line[x].bpp;
                        assert(v < 3);
                        out_line[x] = tm_colors[v + 1];
                    }
                    else {
                        out_line[x] = 0xFF000000;
                    }
                    break; }

            }
        }
    }

}

static void render_image_view_palette(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct SNES *this = (struct SNES *) ptr;
    if (this->clock.master_frame == 0) return;
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2)); // Clear out at least 4 rows worth

    u32 offset = 0;
    u32 y_offset = 0;
    for (u32 palette = 0; palette < 16; palette++) {
        for (u32 color = 0; color < 16; color++) {
            u32 y_top = y_offset + offset;
            u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
            u32 c = gba_to_screen(this->ppu.CGRAM[(palette << 4) + color]);
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


static void setup_image_view_palettes(struct SNES* this, struct debugger_interface *dbgr)
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

static void setup_dbglog(struct SNES *this, struct debugger_interface *dbgr)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_dbglog);
    struct debugger_view *dview = cpg(p);
    struct dbglog_view *dv = &dview->dbglog;
    this->dbg.dvptr = dv;
    snprintf(dv->name, sizeof(dv->name), "Trace");
    dv->has_extra = 1;

    static const u32 wdc_color = 0x8080FF;
    static const u32 spc_color = 0x80FF80;
    static const u32 dma_color = 0xFF8080;
    static const u32 ppu_color = 0xFFFF80;

    struct dbglog_category_node *root = dbglog_category_get_root(dv);
    struct dbglog_category_node *wdc = dbglog_category_add_node(dv, root, "R5A22", NULL, 0, 0);
    dbglog_category_add_node(dv, wdc, "Instruction Trace", "WDC65816", SNES_CAT_WDC_INSTRUCTION, wdc_color);
    this->r5a22.cpu.trace.dbglog.view = dv;
    this->r5a22.cpu.trace.dbglog.id = SNES_CAT_WDC_INSTRUCTION;
    dbglog_category_add_node(dv, wdc, "Reads", "wdc_read", SNES_CAT_WDC_READ, wdc_color);
    dbglog_category_add_node(dv, wdc, "Writes", "wdc_write", SNES_CAT_WDC_WRITE, wdc_color);
    dbglog_category_add_node(dv, wdc, "DMA Starts", "DMA", SNES_CAT_DMA_START, dma_color);
    dbglog_category_add_node(dv, wdc, "DMA Writes", "DMA", SNES_CAT_DMA_WRITE, dma_color);
    dbglog_category_add_node(dv, wdc, "HDMA Writes", "HDMA", SNES_CAT_HDMA_WRITE, dma_color);

    struct dbglog_category_node *spc = dbglog_category_add_node(dv, root, "SPC700", NULL, 0, 0);
    dbglog_category_add_node(dv, spc, "Instruction Trace", "SPC700", SNES_CAT_SPC_INSTRUCTION, spc_color);
    this->apu.cpu.trace.dbglog.view = dv;
    this->apu.cpu.trace.dbglog.id = SNES_CAT_SPC_INSTRUCTION;
    dbglog_category_add_node(dv, spc, "Reads", "spc_read", SNES_CAT_SPC_READ, spc_color);
    this->apu.cpu.trace.dbglog.id_read = SNES_CAT_SPC_READ;
    dbglog_category_add_node(dv, spc, "Writes", "spc_write", SNES_CAT_SPC_WRITE, spc_color);
    this->apu.cpu.trace.dbglog.id_write = SNES_CAT_SPC_WRITE;

    struct dbglog_category_node *ppu = dbglog_category_add_node(dv, root, "PPU", NULL, 0, 0);
    dbglog_category_add_node(dv, ppu, "VRAM Write", "PPU", SNES_CAT_PPU_VRAM_WRITE, ppu_color);


}

static void setup_image_view_ppu_obj_tiles(struct SNES *this, struct debugger_interface *dbgr) {
    struct debugger_view *dview;
    this->dbg.image_views.ppu_layers = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ppu_layers);
    struct image_view *iv = &dview->image;

    iv->width = 542;
    iv->height = 256;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2) {0, 0};
    iv->viewport.p[1] = (struct ivec2) {542, 256};

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_obj_tiles;
    snprintf(iv->label, sizeof(iv->label), "PPU OBJ Tiles");

    struct debugger_widget *rg;
    rg = debugger_widgets_add_radiogroup(&dview->options, "Color mode", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "B&W", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "P0", 1, 0);
    debugger_widget_radiogroup_add_button(rg, "P1", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "P2", 3, 1);
    debugger_widget_radiogroup_add_button(rg, "P3", 4, 1);
    debugger_widget_radiogroup_add_button(rg, "P4", 5, 0);
    debugger_widget_radiogroup_add_button(rg, "P5", 6, 1);
    debugger_widget_radiogroup_add_button(rg, "P6", 7, 1);
    debugger_widget_radiogroup_add_button(rg, "P7", 8, 1);


}

    static void setup_image_view_ppu_tilemaps(struct SNES *this, struct debugger_interface *dbgr) {
    struct debugger_view *dview;
    this->dbg.image_views.ppu_layers = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ppu_layers);
    struct image_view *iv = &dview->image;

    iv->width = 1024;
    iv->height = 1024;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2) {0, 0};
    iv->viewport.p[1] = (struct ivec2) {1024, 1024};

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_tilemaps;
    snprintf(iv->label, sizeof(iv->label), "PPU Tilemaps");

    struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Layer", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "BG1", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "BG2", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "BG3", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "BG4", 3, 1);

}

static void setup_image_view_ppu_layers(struct SNES *this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.ppu_layers = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.ppu_layers);
    struct image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 224;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 256, 224 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_ppu_layers;
    snprintf(iv->label, sizeof(iv->label), "PPU Layer View");

    struct debugger_widget *rg = debugger_widgets_add_radiogroup(&dview->options, "Layer", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "BG1", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "BG2", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "BG3", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "BG4", 3, 1);
    debugger_widget_radiogroup_add_button(rg, "OBJ", 4, 1);

    rg = debugger_widgets_add_radiogroup(&dview->options, "View", 1, 0, 0);
    debugger_widget_radiogroup_add_button(rg, "RGB", 0, 1);
    debugger_widget_radiogroup_add_button(rg, "Has", 1, 1);
    debugger_widget_radiogroup_add_button(rg, "Priority", 2, 1);
    debugger_widget_radiogroup_add_button(rg, "BPP", 3, 1);

    debugger_widgets_add_textbox(&dview->options, "Layer Info", 0);
    struct debugger_widget *key = debugger_widgets_add_color_key(&dview->options, "Key", 1);
}


static void setup_waveforms(struct SNES* this, struct debugger_interface *dbgr)
{
    this->dbg.waveforms.view = debugger_view_new(dbgr, dview_waveforms);
    struct debugger_view *dview = cpg(this->dbg.waveforms.view);
    struct waveform_view *wv = (struct waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "S-APU");

    struct debug_waveform *dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.main = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 1008;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[0] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[1] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[2] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[3] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 4");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[4] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 5");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[5] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 6");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[6] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 7");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = cvec_push_back(&wv->waveforms);
    debug_waveform_init(dw);
    this->dbg.waveforms.chan[7] = make_cvec_ptr(&wv->waveforms, cvec_len(&wv->waveforms)-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 8");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

}

void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = dest;
    for (u32 i = 0; i < 16; i++) {
        *out = SNES_wdc65816_read(ptr, (addr + i) & 0xFFFFFF, 0, 0);
        out++;
    }
}

void readvram(void *ptr, u32 addr, void *dest)
{
    u8 *vramptr = ptr;
    addr &= 0xFFFF;
    if (addr <= 0xFFF0) {
        memcpy(dest, vramptr+addr, 16);
    }
    else {
        u8 *out = dest;
        for (u32 i = 0; i < 16; i++) {
            *out = vramptr[(addr + i) & 0xFFFF];
            out++;
        }
    }
}

static void setup_memory_view(struct SNES* this, struct debugger_interface *dbgr) {
    this->dbg.memory = debugger_view_new(dbgr, dview_memory);
    struct debugger_view *dview = cpg(this->dbg.memory);
    struct memory_view *mv = &dview->memory;
    memory_view_add_module(dbgr, mv, "CPU Memory", 0, 6, 0, 0xFFFFFF, this, &readcpumem);
    memory_view_add_module(dbgr, mv, "VRAM", 1, 4, 0, 0xFFFF, &this->ppu.VRAM, &readvram);
}

static void setup_events_view(struct SNES* this, struct debugger_interface *dbgr) {
    // Setup events view
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    ev->timing = ev_timing_master_clock;
    ev->master_clocks.per_line = 1364;
    ev->master_clocks.height = 262;
    ev->master_clocks.ptr = &this->clock.master_cycle_count;

    ev->associated_display = this->ppu.display_ptr;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 341;
        ev->display[i].height = 262;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }
    ev->associated_display = this->ppu.display_ptr;
    cvec_grow_by(&ev->events, DBG_SNES_EVENT_MAX);
    DEBUG_REGISTER_EVENT_CATEGORY("R5A22 events", DBG_SNES_CATEGORY_R5A22);
    DEBUG_REGISTER_EVENT_CATEGORY("SPC700 events", DBG_SNES_CATEGORY_SPC700);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_SNES_CATEGORY_PPU);

    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_IRQ);
    DEBUG_REGISTER_EVENT("NMI", 0x00FF00, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_NMI);
    DEBUG_REGISTER_EVENT("HDMA start", 0xC0B030, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_HDMA_START);

    DEBUG_REGISTER_EVENT("HIRQ", 0x00FFFF, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_HIRQ);
    DEBUG_REGISTER_EVENT("VRAM write", 0xC030C0, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_VRAM);
    DEBUG_REGISTER_EVENT("COLDATA write", 0xFFFF00, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_COLDATA);
    DEBUG_REGISTER_EVENT("SCROLL write", 0xFFFF00, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_SCROLL);

    SET_EVENT_VIEW(this->r5a22.cpu);
    this->r5a22.cpu.dbg.events.IRQ = DBG_SNES_EVENT_IRQ;
    this->r5a22.cpu.dbg.events.NMI = DBG_SNES_EVENT_NMI;

    debugger_report_frame(this->dbg.interface);
}




    void SNESJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 4;
    cvec_lock_reallocs(&dbgr->views);

    setup_dbglog(this, dbgr);
    setup_events_view(this, dbgr);
    setup_memory_view(this, dbgr);
    setup_waveforms(this, dbgr);
    setup_image_view_palettes(this, dbgr);
    setup_image_view_ppu_layers(this, dbgr);
    setup_image_view_ppu_tilemaps(this, dbgr);
    setup_image_view_ppu_obj_tiles(this, dbgr);
}