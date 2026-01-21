//
// Created by . on 8/12/24.
//

//
// Created by . on 8/12/24.
//

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "component/cpu/sm83/sm83.h"
#include "component/cpu/sm83/sm83_disassembler.h"

#include "gb_bus.h"
#include "gb_debugger.h"
namespace GB {

void core::render_image_view_nametables_DMG(debugger_interface *dbgr, debugger_view *dview, u32 out_width, bool cgb_enable)
{
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);

    // There are TWO 32x32 nametables, but only one is used at a time.
    // So we will just render from the one that is active that line, for now.
    int nt_rows_to_lcdrow[256];
    for (u32 i = 0; i < 256; i++)
        nt_rows_to_lcdrow[i] = -1;
    for (u32 i = 0; i < 144; i++) {
        DBGGBROW *row = &dbg_data.rows[i];
        nt_rows_to_lcdrow[(i + row->io.SCY) % 256] = i;
    }


    u32 scr_x = 0, scr_y = 0;
    for (u32 nt_num = 0; nt_num < 2; nt_num++) {
        scr_y = 0;
        for (u32 get_tile_y = 0; get_tile_y < 32; get_tile_y++) {
            for (u32 tile_row = 0; tile_row < 8; tile_row++) {
                scr_x = 0;
                int snum = nt_rows_to_lcdrow[scr_y];
                DBGGBROW *row = &dbg_data.rows[snum == -1 ? 0 : snum];
                u32 *row_start = outbuf + (nt_num * 257 * out_width) + (scr_y * out_width);
                u32 nt_base_addr = (0x1800 | (nt_num << 10));

                u32 left = row->io.SCX;
                u32 right = (row->io.SCX + 159) & 0xFF;
                for (u32 get_tile_x = 0; get_tile_x < 32; get_tile_x++) {
                    u32 nt_addr = nt_base_addr | (((scr_y & 0xFF) >> 3) << 5) | ((scr_x & 0xFF) >> 3);
                    u32 tn = generic_mapper.VRAM[nt_addr];
                    u32 attr = 0;
                    if (cgb_enable)
                        attr = generic_mapper.VRAM[nt_addr+0x2000];
                    u32 b12 = row->io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
                    u32 hbits = 0;
                    u32 ybits = scr_y & 7;
                    if (cgb_enable) {
                        hbits = (attr & 8) << 10; // bank select for tile
                        if (attr & 0x40) ybits = (7 - ybits); // flip y
                    }

                    u32 pt_addr = (b12 |
                            (tn << 4) |
                            (ybits << 1)
                           ) + hbits;
                    u32 bp0 = generic_mapper.VRAM[pt_addr];
                    u32 bp1 = generic_mapper.VRAM[pt_addr+1];
                    for (u32 i = 0; i < 8; i++) {
                        u32 b, c, pal;
                        if (cgb_enable && (attr & 0x20)) { // flip x
                            b = ((bp0 & 1) | ((bp1 & 1) << 1));
                            bp0 >>= 1;
                            bp1 >>= 1;
                        }
                        else {
                            b = (((bp0 >> 7) & 1) | ((bp1 >> 6) & 2));
                            bp0 <<= 1;
                            bp1 <<= 1;
                        }
                        u32 cv;
                        if (!cgb_enable) {
                            cv = 0x555555 * (3 - ppu.bg_palette[b]);
                        }
                        else {
                            pal = attr & 7;
                            u32 n = ((pal * 4) + b) * 2;
                            u16 o = (((u16)ppu.bg_CRAM[n+1]) << 8) | ppu.bg_CRAM[n];
                            u32 cr = (o >> 10) & 0x1F;
                            u32 cg = (o >> 5) & 0x1F;
                            u32 cb = (o & 0x1F);
                            cr = (cr << 3) | (cr >> 2);
                            cg = (cg << 3) | (cg >> 2);
                            cb = (cb << 3) | (cb >> 2);
                            cv = (cr << 16) | (cg << 8) | cb;
                        }
                        c = 0xFF000000 | cv;

                        if ((snum != -1) && (scr_x == left)) c = 0xFF00FF00; // green left-hand side
                        if ((snum != -1) && (scr_x == right)) c = 0xFFFF0000; // blue right-hand side
                        if ((snum == 0) || (snum == 143)) {
                            if (((right < left) && ((scr_x <= right) || (scr_x >= left))) || ((scr_x <= right) && (scr_x >= left))) {
                                if (snum == 0) c = 0xFF00FF00;
                                if (snum == 143) c = 0xFFFF0000;
                            }
                        }
                        *row_start = c;

                        row_start++;
                        scr_x++;
                    }
                }
                scr_y++;
            }
        }
    }
    u32 *splitter = outbuf + (256 * out_width);
    for (u32 x = 0; x < 256; x++) {
        *splitter = 0xFF0000FF;
        splitter++;
    }
}

static void render_image_view_sprites(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    GB::core *self = (GB::core *) ptr;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    for (u32 rownum = 0; rownum < 144; rownum++) {
        DBGGBROW *row = &self->dbg_data.rows[rownum];
        u32 *row_start = outbuf + (rownum * out_width);
        for (u32 x = 0; x < 160; x++) {
            u32 c;
            u32 cv = (row->sprite_pixels[x] == 0xFFFF ? 0 : 1) * 0xFFFFFF;
            c = 0xFF000000 | cv;

            *row_start = c;
            row_start++;
        }
    }
}

static void render_image_view_nametables(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    GB::core *self = (GB::core *) ptr;
    switch(self->variant) {
        case DMG:
            self->render_image_view_nametables_DMG(dbgr, dview, out_width, false);
            break;
        case GBC:
            if (self->clock.cgb_enable)
                self->render_image_view_nametables_DMG(dbgr, dview, out_width, true);
            else
                self->render_image_view_nametables_DMG(dbgr, dview, out_width, false);
            break;
        default:
            assert(1==2);
            break;
    }
}

static void setup_events_view(GB::core& th, debugger_interface *dbgr)
{
    th.dbg.events.view = dbgr->make_view(dview_events);
    debugger_view *dview = &th.dbg.events.view.get();
    events_view &ev = dview->events;

    for (auto & i : ev.display) {
        i.width = 456;
        i.height = 155;
        i.buf = nullptr;
        i.frame_num = 0;
    }
    ev.associated_display = th.ppu.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("Timer events", DBG_GB_CATEGORY_TIMER);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GB_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_GB_CATEGORY_PPU);

    ev.events.reserve(DBG_GB_EVENT_MAX+10);
    DEBUG_REGISTER_EVENT("IRQ: joypad", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_JOYPAD, 1);
    DEBUG_REGISTER_EVENT("IRQ: vblank", 0x00FF00, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_VBLANK, 1);
    DEBUG_REGISTER_EVENT("IRQ: stat", 0x802060, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_STAT, 1);
    DEBUG_REGISTER_EVENT("IRQ: timer", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_TIMER, 1);
    DEBUG_REGISTER_EVENT("IRQ: serial", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_SERIAL, 1);
    DEBUG_REGISTER_EVENT("HALT end", 0xFF00FF, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_HALT_END, 1);
    DEBUG_REGISTER_EVENT("OAM DMA start", 0x004090, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_OAM_DMA_START, 1);

    DEBUG_REGISTER_EVENT("tick", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_TICK, 1);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_IRQ, 1);

    DEBUG_REGISTER_EVENT("VRAM write", 0x0000FF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_VRAM_WRITE, 1);
    DEBUG_REGISTER_EVENT("SCX write", 0x00FFFF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCX_WRITE, 1);
    DEBUG_REGISTER_EVENT("SCY write", 0xFFFF00, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCY_WRITE, 1);
    DEBUG_REGISTER_EVENT("STAT write", 0x4580FF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_STAT_WRITE, 1);
    DEBUG_REGISTER_EVENT("LCDC write", 0x2080FF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_LCDC_WRITE, 1);
    DEBUG_REGISTER_EVENT("BGP write", 0x208020, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_BGP_WRITE, 1);


    SET_EVENT_VIEW(th.cpu.cpu);
    SET_EVENT_VIEW(th.cpu);
    SET_EVENT_VIEW(th.ppu);
    SET_EVENT_VIEW(th.cpu.timer);
    SET_EVENT_VIEW(th);

    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_JOYPAD, IRQ_joypad);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_VBLANK, IRQ_vblank);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_STAT, IRQ_stat);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_TIMER, IRQ_timer);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_SERIAL, IRQ_serial);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_HALT_END, HALT_end);

    debugger_report_frame(th.dbg.interface);
}

static void render_image_view_tiles(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto *self = static_cast<GB::core *>(ptr);
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    u32 banks = 1;
    if (self->variant == GBC) banks = 2;
    for (u32 bank = 0; bank < banks; bank++) {
        for (u32 tile_set = 0; tile_set < 3; tile_set++) {
            u32 tilenum = 0;
            u32 tile_base_addr = (bank * 8192) + (0x800 * tile_set);
            for (u32 tile_macro_y = 0; tile_macro_y < 16; tile_macro_y++) {
                for (u32 tile_macro_x = 0; tile_macro_x < 16; tile_macro_x++) {
                    for (u32 tile_y = 0; tile_y < 8; tile_y++) {
                        u32 *outptr = outbuf + (((tile_macro_y * 8) + (tile_set * 128) + tile_y) * out_width) + (tile_macro_x * 8) + (bank * 128);
                        u32 addr = tile_base_addr + ((tilenum << 4) + (tile_y << 1));
                        u32 bp0 = self->generic_mapper.VRAM[addr];
                        u32 bp1 = self->generic_mapper.VRAM[addr+1];
                        for (u32 tile_x = 0; tile_x < 8; tile_x++) {
                            u32 cv = ((bp0 >> 7) & 1) | ((bp1 >> 6) & 2);
                            bp0 <<= 1;
                            bp1 <<= 1;
                            cv *= 0x555555;
                            *outptr = 0xFF000000 | cv;
                            outptr++;
                        }
                    }
                    tilenum++;
                }
            }
        }
    }
}

static void setup_debugger_view_tiles(GB::core* self, debugger_interface *dbgr) {
    self->dbg.image_views.tiles = dbgr->make_view(dview_image);
    debugger_view *dview = &self->dbg.image_views.tiles.get();
    image_view *iv = &dview->image;

    // 384 tiles. 128 (16 tiles) wide, 16 tiles tall,
    // Color gameboy has a second bank
    iv->width = 8 * 16;
    iv->height = 8 * 16 * 3;
    if (self->variant == GBC) iv->width = (iv->width * 2) + 4;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;

    iv->update_func.ptr = self;
    iv->update_func.func = &render_image_view_tiles;

    snprintf(iv->label, sizeof(iv->label), "Tile View");
}

static void setup_debugger_view_sprites(GB::core* self, debugger_interface *dbgr)
{
    self->dbg.image_views.sprites = dbgr->make_view(dview_image);
    debugger_view *dview = &self->dbg.image_views.sprites.get();
    image_view *iv = &dview->image;
    iv->width = 160;
    iv->height = 144;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;

    iv->update_func.ptr = self;
    iv->update_func.func = &render_image_view_sprites;

    snprintf(iv->label, sizeof(iv->label), "Sprite Position Viewer");
}

static void setup_debugger_view_nametables(GB::core* self, debugger_interface *dbgr)
{
    self->dbg.image_views.nametables = dbgr->make_view(dview_image);
    debugger_view *dview = &self->dbg.image_views.nametables.get();
    image_view *iv = &dview->image;
    // each 32x32 is 256x256. Since we have 2, we're going to do 256x512

    iv->width = 256;
    iv->height = 513;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;

    iv->update_func.ptr = self;
    iv->update_func.func = &render_image_view_nametables;

    snprintf(iv->label, sizeof(iv->label), "Nametable Viewer");
}

static void setup_waveforms(GB::core* self, debugger_interface *dbgr)
{
    self->dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    debugger_view *dview = &self->dbg.waveforms.view.get();
    waveform_view *wv = (waveform_view *)&dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "Audio");

    // 384 8x8 tiles, or 2x for CGB
    debug_waveform *dw = &wv->waveforms.emplace_back();
    self->dbg.waveforms.main.make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;

    dw = &wv->waveforms.emplace_back();
    self->dbg.waveforms.chan[0].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Channel 0");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    self->dbg.waveforms.chan[1].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Channel 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    self->dbg.waveforms.chan[2].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Channel 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;

    dw = &wv->waveforms.emplace_back();
    self->dbg.waveforms.chan[3].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Channel 3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
}


void core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;
    dbgr.views.reserve(15);

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 1;

    setup_events_view(*this, &dbgr);
    setup_debugger_view_nametables(this, &dbgr);
    setup_debugger_view_sprites(this, &dbgr);
    setup_debugger_view_tiles(this, &dbgr);
    setup_waveforms(this, &dbgr);
}
}