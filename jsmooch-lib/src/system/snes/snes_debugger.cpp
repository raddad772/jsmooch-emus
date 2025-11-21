//
// Created by . on 4/23/25.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>

#include "snes_debugger.h"

#include "helpers/color.h"
#include "snes_bus.h"
#include "snes_ppu.h"
#include "component/cpu/wdc65816/wdc65816_disassembler.h"
#include "component/cpu/spc700/spc700_disassembler.h"

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void render_image_view_tilemaps(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    SNES *th = static_cast<SNES *>(ptr);
    if (th->clock.master_frame == 0) return;
    debugger_widget_radiogroup *layernum = &dview->options.at(0).radiogroup;
    u32 ln = layernum->value;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 1024);

    for (u32 y = 0; y < 224; y++) {
    }
}

static void print_layer_info(SNES *th, u32 bgnum, debugger_widget_textbox *tb)
{
    if (bgnum < 4) {
        SNES_PPU::SNES_PPU_BG *bg = &th->ppu.pbg[bgnum];
        tb->sprintf("\n-BG%d:  bgmode:%d  ", bgnum, th->ppu.io.bg_mode);
        //if (th->ppu.io.bg_mode == 7) {
            tb->sprintf("  dc:%d  ", th->ppu.color_math.direct_color);
        //}
        if (bg->main_enable) tb->sprintf("main:on ");
        else tb->sprintf("main:off");
        if (bg->sub_enable) tb->sprintf(" sub:on ");
        else tb->sprintf(" sub:off");

        tb->sprintf("  bpp:");
        switch(bg->tile_mode) {
            case SNES_PPU::SNES_PPU_BG::BPP2:
                tb->sprintf("2  ");
                break;
            case SNES_PPU::SNES_PPU_BG::BPP4:
                tb->sprintf("4  ");
                break;
            case SNES_PPU::SNES_PPU_BG::BPP8:
                tb->sprintf("8  ");
                break;
            case SNES_PPU::SNES_PPU_BG::mode7:
                tb->sprintf("m7 ");
                break;
            case SNES_PPU::SNES_PPU_BG::inactive:
                tb->sprintf("inactive ");
                break;
        }

        tb->sprintf("  mosaic:%d", bg->mosaic.enable);
        tb->sprintf("\n---hscroll:%d  vscroll:%d  scr_adr:%04x  tile_addr:%04x  pal_base:%d", bg->io.hoffset, bg->io.voffset, bg->io.screen_addr, bg->io.tiledata_addr, th->ppu.io.bg_mode == 0 ? bgnum << 5 : 0);
        //tb->sprintf("  priority:%",  bg->priority);

    }
}

static void render_image_view_obj_tiles(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    SNES *th = static_cast<SNES *>(ptr);
    if (th->clock.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 256);

    u32 colormode = dview->options.at(0).radiogroup.value;

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
                    u32 data = th->ppu.VRAM[get_addr] | (th->ppu.VRAM[(get_addr + 8) & 0x7FFF] << 16);
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
                                    FALLTHROUGH;
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
                                    color = gba_to_screen(th->ppu.CGRAM[c]);
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

static void render_image_view_ppu_layers(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    SNES *th = (SNES *) ptr;
    if (th->clock.master_frame == 0) return;
    debugger_widget_radiogroup *layernum = &dview->options.at(0).radiogroup;

    debugger_widget_radiogroup *attrkind = &dview->options.at(1).radiogroup;

    static constexpr u32 tm_colors[8] = {
            0xFF000000, // no texture/color
            0xFFFF0000, // color 1, blue
            0xFF0000FF, // color 2, red
            0xFF00FF00, // color 3, green
            0xFFFFFF00, // color 4, teal
            0xFFFF00FF, // color 5, purple
            0xFF00FFFF, // color 6, yellow
            0xFFFFFFFF, // color 7, white
    };

    debugger_widget_textbox *tb = &dview->options.at(2).textbox;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 224);

    tb->clear();

    debugger_widget_colorkey *colorkey = &dview->options.at(3).colorkey;
    print_layer_info(th, layernum->value, tb);

    switch(attrkind->value) {
        case 0: // Color output
            colorkey->set_title("Output");
            break;
        case 1: // Has
            colorkey->set_title("Has: white");
            break;
        case 2: // Priority
            colorkey->set_title("Priority");
            colorkey->add_item("No pixel", tm_colors[0]);
            colorkey->add_item("Prio 0", tm_colors[1]);
            colorkey->add_item("Prio 1", tm_colors[2]);
            colorkey->add_item("Prio 2", tm_colors[3]);
            colorkey->add_item("Prio 3", tm_colors[4]);
            break;
        case 3: // BPP
            colorkey->set_title("BPP");
            colorkey->add_item("No pixel", tm_colors[0]);
            colorkey->add_item("2BPP", tm_colors[1]);
            colorkey->add_item("4BPP", tm_colors[2]);
            colorkey->add_item("8BPP", tm_colors[3]);
            break;
    }


    for (u32 y = 0; y < 224; y++) {
        u32 *out_line = outbuf + (y * out_width);
        SNES_PPU_px *px_line;
        if (layernum->value < 4) {
            px_line = th->dbg_info.line[y].bg[layernum->value].px;
        }
        else {
            px_line = th->dbg_info.line[y].sprite_px;
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
                default:
            }
        }
    }

}

static void render_image_view_palette(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    SNES *th = static_cast<SNES *>(ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    auto *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2)); // Clear out at least 4 rows worth

    u32 offset = 0;
    u32 y_offset = 0;
    for (u32 palette = 0; palette < 16; palette++) {
        for (u32 color = 0; color < 16; color++) {
            const u32 y_top = y_offset + offset;
            const u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
            const u32 c = gba_to_screen(th->ppu.CGRAM[(palette << 4) + color]);
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


static void setup_image_view_palettes(SNES& th, debugger_interface &dbgr)
{
    th.dbg.image_views.palettes = dbgr.make_view(dview_image);
    debugger_view *dview = &th.dbg.image_views.palettes.get();
    image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = ((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = ivec2(static_cast<i32>(iv->width), static_cast<i32>(iv->height));

    iv->update_func.ptr = &th;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");
}

static void setup_dbglog(SNES &th, debugger_interface &dbgr)
{
    cvec_ptr p = dbgr.make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th.dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = 1;

    static constexpr u32 wdc_color = 0x8080FF;
    static constexpr u32 spc_color = 0x80FF80;
    static constexpr u32 dma_color = 0xFF8080;
    static constexpr u32 ppu_color = 0xFFFF80;

    dbglog_category_node &root = dv.get_category_root();
    dbglog_category_node &wdc = root.add_node(dv, "R5A22", nullptr, 0, 0);
    wdc.add_node(dv, "Instruction Trace", "WDC65816", SNES_CAT_WDC_INSTRUCTION, wdc_color);
    th.r5a22.cpu.trace.dbglog.view = &dv;
    th.r5a22.cpu.trace.dbglog.id = SNES_CAT_WDC_INSTRUCTION;
    wdc.add_node(dv, "Reads", "wdc_read", SNES_CAT_WDC_READ, wdc_color);
    wdc.add_node(dv, "Writes", "wdc_write", SNES_CAT_WDC_WRITE, wdc_color);
    wdc.add_node(dv, "DMA Starts", "DMA", SNES_CAT_DMA_START, dma_color);
    wdc.add_node(dv, "DMA Writes", "DMA", SNES_CAT_DMA_WRITE, dma_color);
    wdc.add_node(dv, "HDMA Writes", "HDMA", SNES_CAT_HDMA_WRITE, dma_color);

    dbglog_category_node &spc = root.add_node(dv, "SPC700", nullptr, 0, 0);
    spc.add_node(dv, "Instruction Trace", "SPC700", SNES_CAT_SPC_INSTRUCTION, spc_color);
    th.apu.cpu.trace.dbglog.view = &dv;
    th.apu.cpu.trace.dbglog.id = SNES_CAT_SPC_INSTRUCTION;
    spc.add_node(dv, "Reads", "spc_read", SNES_CAT_SPC_READ, spc_color);
    th.apu.cpu.trace.dbglog.id_read = SNES_CAT_SPC_READ;
    spc.add_node(dv, "Writes", "spc_write", SNES_CAT_SPC_WRITE, spc_color);
    th.apu.cpu.trace.dbglog.id_write = SNES_CAT_SPC_WRITE;

    dbglog_category_node &ppu = root.add_node(dv, "PPU", nullptr, 0, 0);
    ppu.add_node(dv, "VRAM Write", "PPU", SNES_CAT_PPU_VRAM_WRITE, ppu_color);
}

static void setup_image_view_ppu_obj_tiles(SNES &th, debugger_interface &dbgr) {
    th.dbg.image_views.ppu_layers = dbgr.make_view(dview_image);
    debugger_view *dview = &th.dbg.image_views.ppu_layers.get();
    image_view *iv = &dview->image;

    iv->width = 542;
    iv->height = 256;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2) {0, 0};
    iv->viewport.p[1] = (ivec2) {542, 256};

    iv->update_func.ptr = &th;
    iv->update_func.func = &render_image_view_obj_tiles;
    snprintf(iv->label, sizeof(iv->label), "PPU OBJ Tiles");

    debugger_widget_radiogroup &rg = debugger_widgets_add_radiogroup(dview->options, "Color mode", 1, 0, 0).radiogroup;
    rg.add_button("B&W", 0, 1);
    rg.add_button("P0", 1, 0);
    rg.add_button("P1", 2, 1);
    rg.add_button("P2", 3, 1);
    rg.add_button("P3", 4, 1);
    rg.add_button("P4", 5, 0);
    rg.add_button("P5", 6, 1);
    rg.add_button("P6", 7, 1);
    rg.add_button("P7", 8, 1);


}

static void setup_image_view_ppu_tilemaps(SNES&th, debugger_interface &dbgr)
{
    th.dbg.image_views.ppu_layers = dbgr.make_view(dview_image);
    debugger_view *dview = &th.dbg.image_views.ppu_layers.get();
    image_view *iv = &dview->image;

    iv->width = 1024;
    iv->height = 1024;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2) {0, 0};
    iv->viewport.p[1] = (ivec2) {1024, 1024};

    iv->update_func.ptr = &th;
    iv->update_func.func = &render_image_view_tilemaps;
    snprintf(iv->label, sizeof(iv->label), "PPU Tilemaps");

    debugger_widget_radiogroup &rg = debugger_widgets_add_radiogroup(dview->options, "Layer", 1, 0, 0).radiogroup;
    rg.add_button("BG1", 0, 1);
    rg.add_button("BG2", 1, 1);
    rg.add_button("BG3", 2, 1);
    rg.add_button("BG4", 3, 1);
}

static void setup_image_view_ppu_layers(SNES &th, debugger_interface &dbgr)
{
    th.dbg.image_views.ppu_layers = dbgr.make_view(dview_image);
    debugger_view *dview = &th.dbg.image_views.ppu_layers.get();
    image_view *iv = &dview->image;

    iv->width = 256;
    iv->height = 224;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 256, 224 };

    iv->update_func.ptr = &th;
    iv->update_func.func = &render_image_view_ppu_layers;
    snprintf(iv->label, sizeof(iv->label), "PPU Layer View");

    debugger_widget_radiogroup &rg = debugger_widgets_add_radiogroup(dview->options, "Layer", 1, 0, 0).radiogroup;
    rg.add_button("BG1", 0, 1);
    rg.add_button("BG2", 1, 1);
    rg.add_button("BG3", 2, 1);
    rg.add_button("BG4", 3, 1);
    rg.add_button("OBJ", 4, 1);

    debugger_widget_radiogroup &nrg = debugger_widgets_add_radiogroup(dview->options, "View", 1, 0, 0).radiogroup;
    nrg.add_button("RGB", 0, 1);
    nrg.add_button("Has", 1, 1);
    nrg.add_button("Priority", 2, 1);
    nrg.add_button("BPP", 3, 1);

    debugger_widgets_add_textbox(dview->options, "Layer Info", 0);
    debugger_widget &key = debugger_widgets_add_color_key(dview->options, "Key", 1);
}


static void setup_waveforms(SNES& th, debugger_interface *dbgr)
{
    th.dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    debugger_view &dview = th.dbg.waveforms.view.get();
    waveform_view &wv = dview.waveform;
    snprintf(wv.name, sizeof(wv.name), "S-APU");

    debug_waveform *dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.main.make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;

    dw->default_clock_divider = 1008;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[0].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[1].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[2].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[3].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 4");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[4].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 5");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[5].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 6");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[6].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 7");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv.waveforms.emplace_back();
    th.dbg.waveforms.chan[7].make(wv.waveforms, wv.waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Voice 8");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}

static void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = static_cast<u8 *>(dest);
    SNES *snes = static_cast<SNES *>(ptr);
    for (u32 i = 0; i < 16; i++) {
        *out = snes->mem.read_bus_A((addr + i) & 0xFFFFFF, 0, 0);
        out++;
    }
}

static void readvram(void *ptr, u32 addr, void *dest)
{
    u8 *vramptr = static_cast<u8 *>(ptr);
    addr &= 0xFFFF;
    if (addr <= 0xFFF0) {
        memcpy(dest, vramptr+addr, 16);
    }
    else {
        u8 *out = static_cast<u8 *>(dest);
        for (u32 i = 0; i < 16; i++) {
            *out = vramptr[(addr + i) & 0xFFFF];
            out++;
        }
    }
}

static void setup_memory_view(SNES& th, debugger_interface &dbgr) {
    th.dbg.memory = dbgr.make_view(dview_memory);
    debugger_view *dview = &th.dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Memory", 0, 6, 0, 0xFFFFFF, &th, &readcpumem);
    mv->add_module("VRAM", 1, 4, 0, 0xFFFF, &th.ppu.VRAM, &readvram);
}

static void setup_events_view(SNES& th, debugger_interface &dbgr) {
    // Setup events view
    th.dbg.events.view = dbgr.make_view(dview_events);
    debugger_view *dview = &th.dbg.events.view.get();
    events_view &ev = dview->events;

    ev.timing = ev_timing_master_clock;
    ev.master_clocks.per_line = 1364;
    ev.master_clocks.height = 262;
    ev.master_clocks.ptr = &th.clock.master_cycle_count;

    ev.associated_display = th.ppu.display_ptr;

    for (auto & disp : ev.display) {
        disp.width = 341;
        disp.height = 262;
        disp.buf = nullptr;
        disp.frame_num = 0;
    }
    ev.associated_display = th.ppu.display_ptr;
    ev.events.reserve(ev.events.capacity() + DBG_SNES_EVENT_MAX);
    DEBUG_REGISTER_EVENT_CATEGORY("R5A22 events", DBG_SNES_CATEGORY_R5A22);
    DEBUG_REGISTER_EVENT_CATEGORY("SPC700 events", DBG_SNES_CATEGORY_SPC700);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_SNES_CATEGORY_PPU);

    DEBUG_REGISTER_EVENT("IRQ", 0xFF0000, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_IRQ, 1);
    DEBUG_REGISTER_EVENT("NMI", 0x00FF00, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_NMI, 1);
    DEBUG_REGISTER_EVENT("HDMA start", 0xC0B030, DBG_SNES_CATEGORY_R5A22, DBG_SNES_EVENT_HDMA_START, 1);

    DEBUG_REGISTER_EVENT("HIRQ", 0x00FFFF, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_HIRQ, 1);
    DEBUG_REGISTER_EVENT("VRAM write", 0xC030C0, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_VRAM, 1);
    DEBUG_REGISTER_EVENT("COLDATA write", 0xFFFF00, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_COLDATA, 1);
    DEBUG_REGISTER_EVENT("SCROLL write", 0xFFFF00, DBG_SNES_CATEGORY_PPU, DBG_SNES_EVENT_WRITE_SCROLL, 1);

    SET_EVENT_VIEW(th.r5a22.cpu);
    th.r5a22.cpu.dbg.events.IRQ = DBG_SNES_EVENT_IRQ;
    th.r5a22.cpu.dbg.events.NMI = DBG_SNES_EVENT_NMI;

    debugger_report_frame(th.dbg.interface);
}

void SNES::setup_debugger_interface(debugger_interface &intf) {
    dbg.interface = &intf;
    auto *dbgr = dbg.interface;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 4;
    dbgr->views.reserve(15);

    setup_dbglog(*this, *dbgr);
    setup_events_view(*this, *dbgr);
    setup_memory_view(*this, *dbgr);
    setup_image_view_palettes(*this, *dbgr);
    setup_image_view_ppu_layers(*this, *dbgr);
    setup_image_view_ppu_tilemaps(*this, *dbgr);
    setup_image_view_ppu_obj_tiles(*this, *dbgr);
    setup_waveforms(*this, dbgr);
}