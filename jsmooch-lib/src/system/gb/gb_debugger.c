//
// Created by . on 8/12/24.
//

//
// Created by . on 8/12/24.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "helpers/debugger/debugger.h"

#include "component/cpu/sm83/sm83.h"
#include "component/cpu/sm83/sm83_disassembler.h"

#include "gb.h"
#include "gb_debugger.h"

#define JTHIS struct GB* this = (struct GB*)jsm->ptr
#define JSM struct jsm_system* jsm


static void render_image_view_nametables_DMG(struct GB* this, struct debugger_interface *dbgr, struct debugger_view *dview, u32 out_width)
{
    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;

    // There are TWO 32x32 nametables, but only one is used at a time.
    // So we will just render from the one that is active that line, for now.
    int nt_rows_to_lcdrow[256];
    for (u32 i = 0; i < 256; i++)
        nt_rows_to_lcdrow[i] = -1;
    for (u32 i = 0; i < 144; i++) {
        struct DBGGBROW *row = &this->dbg_data.rows[i];
        nt_rows_to_lcdrow[(i + row->io.SCY) % 256] = i;
    }


    u32 scr_x = 0, scr_y = 0;
    for (u32 nt_num = 0; nt_num < 2; nt_num++) {
        scr_y = 0;
        for (u32 get_tile_y = 0; get_tile_y < 32; get_tile_y++) {
            for (u32 tile_row = 0; tile_row < 8; tile_row++) {
                scr_x = 0;
                int snum = nt_rows_to_lcdrow[scr_y];
                struct DBGGBROW *row = &this->dbg_data.rows[snum == -1 ? 0 : snum];
                u32 *row_start = outbuf + (nt_num * 257 * out_width) + (scr_y * out_width);
                u32 nt_base_addr = (0x1800 | (nt_num << 10));

                u32 left = row->io.SCX;
                u32 right = (row->io.SCX + 160) & 0xFF;
                for (u32 get_tile_x = 0; get_tile_x < 32; get_tile_x++) {
                    u32 nt_addr = nt_base_addr | (((scr_y & 0xFF) >> 3) << 5) | ((scr_x & 0xFF) >> 3);
                    u32 tn = this->bus.generic_mapper.VRAM[nt_addr];
                    u32 b12 = row->io.bg_window_tile_data_base ? 0 : ((tn & 0x80) ^ 0x80) << 5;
                    u32 hbits = 0;
                    u32 ybits = scr_y & 7;
                    u32 pt_addr = (b12 |
                            (tn << 4) |
                            (ybits << 1)
                           ) + hbits;
                    u32 bp0 = this->bus.generic_mapper.VRAM[pt_addr];
                    u32 bp1 = this->bus.generic_mapper.VRAM[pt_addr+1];
                    for (u32 i = 0; i < 8; i++) {
                        u32 b = 3 - (((bp0 >> 7) & 1) | ((bp1 >> 6) & 2));
                        bp0 <<= 1;
                        bp1 <<= 1;
                        u32 c = 0xFF000000 | (0x555555 * b);
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

static void render_image_view_nametables(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width)
{
    struct GB *this = (struct GB *) ptr;
    switch(this->variant) {
        case DMG:
            render_image_view_nametables_DMG(this, dbgr, dview, out_width);
            break;
        default:
            assert(1==2);
            break;
    }
}

static void setup_events_view(struct GB* this, struct debugger_interface *dbgr)
{
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = 456;
        ev->display[i].height = 154;
        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }
    ev->associated_display = this->ppu.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("Timer events", DBG_GB_CATEGORY_TIMER);
    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_GB_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("PPU events", DBG_GB_CATEGORY_PPU);

    cvec_grow(&ev->events, DBG_GB_EVENT_MAX);
    DEBUG_REGISTER_EVENT("IRQ: joypad", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_JOYPAD);
    DEBUG_REGISTER_EVENT("IRQ: vblank", 0x00FF00, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_VBLANK);
    DEBUG_REGISTER_EVENT("IRQ: stat", 0x802060, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_STAT);
    DEBUG_REGISTER_EVENT("IRQ: timer", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_TIMER);
    DEBUG_REGISTER_EVENT("IRQ: serial", 0xFF0000, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_IRQ_SERIAL);
    DEBUG_REGISTER_EVENT("HALT end", 0xFF00FF, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_HALT_END);
    DEBUG_REGISTER_EVENT("OAM DMA start", 0x004090, DBG_GB_CATEGORY_CPU, DBG_GB_EVENT_OAM_DMA_START);

    DEBUG_REGISTER_EVENT("tick", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_TICK);
    DEBUG_REGISTER_EVENT("IRQ", 0xFF4090, DBG_GB_CATEGORY_TIMER, DBG_GB_EVENT_TIMER_IRQ);

    DEBUG_REGISTER_EVENT("VRAM write", 0x0000FF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_VRAM_WRITE);
    DEBUG_REGISTER_EVENT("SCX write", 0x00FFFF, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCX_WRITE);
    DEBUG_REGISTER_EVENT("SCY write", 0xFFFF00, DBG_GB_CATEGORY_PPU, DBG_GB_EVENT_SCY_WRITE);

    SET_EVENT_VIEW(this->cpu.cpu);
    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->ppu);
    SET_EVENT_VIEW(this->cpu.timer);
    SET_EVENT_VIEW(this->bus);

    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_JOYPAD, IRQ_joypad);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_VBLANK, IRQ_vblank);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_STAT, IRQ_stat);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_TIMER, IRQ_timer);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_IRQ_SERIAL, IRQ_serial);
    SET_CPU_CPU_EVENT_ID(DBG_GB_EVENT_HALT_END, HALT_end);

    event_view_begin_frame(this->dbg.events.view);
}

static void setup_debugger_view_nametables(struct GB* this, struct debugger_interface *dbgr)
{
    this->dbg.image_views.nametables = debugger_view_new(dbgr, dview_image);
    struct debugger_view *dview = cpg(this->dbg.image_views.nametables);
    struct image_view *iv = &dview->image;
    // each 32x32 is 256x256. Since we have 2, we're going to do 256x512

    iv->width = 256;
    iv->height = 513;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_nametables;

    sprintf(iv->label, "Nametable Viewer");
}


void GBJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;

    setup_events_view(this, dbgr);
    setup_debugger_view_nametables(this, dbgr);
}
