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

static void setup_dbglog(struct debugger_interface *dbgr, struct SNES *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_dbglog);
    struct debugger_view *dview = cpg(p);
    struct dbglog_view *dv = &dview->dbglog;
    this->dbg.dvptr = dv;
    snprintf(dv->name, sizeof(dv->name), "Trace");
    dv->has_extra = 1;

    static const u32 wdc_color = 0x8080FF;
    static const u32 spc_color = 0x80FF80;

    struct dbglog_category_node *root = dbglog_category_get_root(dv);
    struct dbglog_category_node *wdc = dbglog_category_add_node(dv, root, "WDC65816", NULL, 0, 0);
    dbglog_category_add_node(dv, wdc, "Instruction Trace", "WDC65816", SNES_CAT_WDC_INSTRUCTION, wdc_color);
    this->r5a22.cpu.trace.dbglog.view = dv;
    this->r5a22.cpu.trace.dbglog.id = SNES_CAT_WDC_INSTRUCTION;
    dbglog_category_add_node(dv, wdc, "Reads", "wdc_read", SNES_CAT_WDC_READ, wdc_color);
    dbglog_category_add_node(dv, wdc, "Writes", "wdc_write", SNES_CAT_WDC_WRITE, wdc_color);

    struct dbglog_category_node *spc = dbglog_category_add_node(dv, root, "SPC700", NULL, 0, 0);
    dbglog_category_add_node(dv, spc, "Instruction Trace", "SPC700", SNES_CAT_SPC_INSTRUCTION, spc_color);
    this->apu.cpu.trace.dbglog.view = dv;
    this->apu.cpu.trace.dbglog.id = SNES_CAT_SPC_INSTRUCTION;
    dbglog_category_add_node(dv, spc, "Reads", "spc_read", SNES_CAT_SPC_READ, spc_color);
    this->apu.cpu.trace.dbglog.id_read = SNES_CAT_SPC_READ;
    dbglog_category_add_node(dv, spc, "Writes", "spc_write", SNES_CAT_SPC_WRITE, spc_color);
    this->apu.cpu.trace.dbglog.id_write = SNES_CAT_SPC_WRITE;
}


void SNESJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 4;
    cvec_lock_reallocs(&dbgr->views);

    setup_dbglog(dbgr, this);
    setup_image_view_palettes(this, dbgr);

}