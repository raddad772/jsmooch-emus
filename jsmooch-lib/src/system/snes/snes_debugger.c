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


void SNESJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 4;

    setup_image_view_palettes(this, dbgr);

}