//
// Created by . on 6/18/25.
//

#include <string.h>

#include "helpers/color.h"

#include "tg16_debugger.h"
#include "tg16_bus.h"

#define JTHIS struct TG16* this = (struct TG16*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct TG16* this
#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static void render_image_view_tiles(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct TG16 *this = (struct TG16 *) ptr;
    if (this->vce.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    u32 tn = 0;
    memset(outbuf, 0, out_width * 4 * 512);
    for (u32 ty = 0; ty < 64; ty++) {
        u32 tile_screen_y = ty * 8;
        for (u32 tx = 0; tx < 64; tx++) {
            tn = (ty * 64) + tx;
            u32 tile_screen_x = tx * 8;
            u32 addr = tn << 4;
            for (u32 inner_y = 0; inner_y < 8; inner_y++) {
                // plane 1 & 2 are at addr
                // 3&4 are at addr + 8
                u32 screen_y = tile_screen_y + inner_y;
                u32 *screen_y_ptr = outbuf + (screen_y * out_width);
                u32 plane12 = this->vdc0.VRAM[addr & 0x7FFF];
                u32 plane34 = this->vdc0.VRAM[(addr+8) & 0x7FFF];
                addr++;
                u32 screen_x = tile_screen_x;
                // lower byte is 1,3
                // higher byte is 2,4
                // lowest bit is leftmost pixel?
                u32 px = tg16_decode_line(plane12, plane34);
                for  (u32 inner_x = 0; inner_x < 8; inner_x++) {
                    u32 v = 17 * (px & 15);
                    px >>= 4;
                    screen_y_ptr[screen_x] = v | (v << 8) | (v << 16) | 0xFF000000;
                    screen_x++;
                }
            }
        }
    }
}

static void render_image_view_palette(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct TG16 *this = (struct TG16 *) ptr;
    if (this->vce.master_frame == 0) return;
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
                u32 c = tg16_to_screen(this->vce.CRAM[lohi + (palette << 4) + color]);
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

static void setup_dbglog(struct debugger_interface *dbgr, struct TG16 *this)
{
    struct cvec_ptr p = debugger_view_new(dbgr, dview_dbglog);
    struct debugger_view *dview = cpg(p);
    struct dbglog_view *dv = &dview->dbglog;
    this->dbg.dvptr = dv;
    snprintf(dv->name, sizeof(dv->name), "Trace");
    dv->has_extra = 1;

    u32 cpu_color = 0x8080FF;
    struct dbglog_category_node *root = dbglog_category_get_root(dv);
    struct dbglog_category_node *cpu = dbglog_category_add_node(dv, root, "CPU (HuC6280)", NULL, 0, 0);
    dbglog_category_add_node(dv, cpu, "Instruction Trace", "CPU", TG16_CAT_CPU_INSTRUCTION, cpu_color);
    this->cpu.dbg.dvptr = dv;
    this->cpu.dbg.dv_id = TG16_CAT_CPU_INSTRUCTION;
    dbglog_category_add_node(dv, cpu, "Reads", "cpu_read", TG16_CAT_CPU_READ, cpu_color);
    dbglog_category_add_node(dv, cpu, "Writes", "cpu_write", TG16_CAT_CPU_WRITE, cpu_color);
    this->cpu.trace.dbglog.id_read = TG16_CAT_CPU_READ;
    this->cpu.trace.dbglog.id_write = TG16_CAT_CPU_WRITE;

    dbglog_category_add_node(dv, cpu, "IRQs", "CPU", TG16_CAT_CPU_IRQS, 0xA0AF80);
}

static void setup_image_view_tiles(struct TG16* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.image_views.tiles = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.tiles);
    struct image_view *iv = &dview->image;

    iv->width = 512;
    iv->height = 512;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 512, 512 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_tiles;
    snprintf(iv->label, sizeof(iv->label), "Tile Viewer");
}

static void setup_image_view_palettes(struct TG16* this, struct debugger_interface *dbgr) {
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

// 64x64 tiles
// aka 512x512px

void TG16J_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 0;
    dbgr->smallest_step = 1;

    setup_dbglog(dbgr, this);
    setup_image_view_palettes(this, dbgr);
    setup_image_view_tiles(this, dbgr);
}