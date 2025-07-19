//
// Created by . on 6/18/25.
//

#include <string.h>

#include "helpers/color.h"

#include "tg16_debugger.h"
#include "tg16_bus.h"
#include "component/gpu/huc6260/huc6260.h"

#define JTHIS struct TG16* this = (struct TG16*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct TG16* this
#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11

static inline u16 read_VRAM(struct HUC6270 *this, u32 addr)
{
    return (addr & 0x8000) ? 0 : this->VRAM[addr];
}


static void render_image_view_bg(struct debugger_interface *dbgr, struct debugger_view *dview, void *ptr, u32 out_width) {
    struct TG16 *this = (struct TG16 *) ptr;
    if (this->vce.master_frame == 0) return;

    struct image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = iv->img_buf[iv->draw_which_buf].ptr;
    memset(outbuf, 0, out_width * 4 * 512);
    static const u32 screen_sizes[8][2] = { {32, 32}, {64, 32},
                                            {128, 32}, {128, 32},
                                            {32, 64}, {64, 64},
                                            {128, 64}, {128,64}
    };

    u32 xtsize = this->vdc0.bg.x_tiles;
    u32 ytsize = this->vdc0.bg.y_tiles;

    for (u32 ty = 0; ty < ytsize; ty++) {
        for (u32 tx = 0; tx < xtsize; tx++) {
            for (u32 in_tile_y = 0; in_tile_y < 8; in_tile_y++) {
                u32 screen_y = (ty << 3) + in_tile_y;
                u32 *screen_y_ptr = outbuf + (screen_y * out_width);
                u32 addr = (ty * xtsize) + tx;
                u32 entry = read_VRAM(&this->vdc0, addr);
                addr = ((entry & 0xFFF) << 4) + in_tile_y;
                u32 palette = (((entry >> 12) & 15) << 4);
                u32 plane12 = read_VRAM(&this->vdc0, addr);
                u32 plane34 = read_VRAM(&this->vdc0, addr+8);
                u32 pixel_shifter = tg16_decode_line(plane12, plane34);

                for (u32 in_tile_x = 0; in_tile_x < 8; in_tile_x++) {
                    u32 screen_x = (tx << 3) + in_tile_x;
                    u32 c = pixel_shifter & 15;
                    //if (c != 0) {
                        pixel_shifter >>= 4;
                        c |= palette;
                        c = this->vce.CRAM[c];
                        c = tg16_to_screen(c);
                    //}
                    //else {
                        //c = 0xFF000000;
                    //}
                    screen_y_ptr[screen_x] = c;
                }
            }

        }
    }
}

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

static void setup_events_view(struct TG16* this, struct debugger_interface *dbgr, struct jsm_system *jsm)
{
    this->dbg.events.view = debugger_view_new(dbgr, dview_events);
    struct debugger_view *dview = cpg(this->dbg.events.view);
    struct events_view *ev = &dview->events;

    ev->timing = ev_timing_master_clock;
    ev->master_clocks.per_line = HUC6260_CYCLE_PER_LINE;
    ev->master_clocks.height = 262;
    ev->master_clocks.ptr = &this->clock.master_cycles;

    for (u32 i = 0; i < 2; i++) {
        ev->display[i].width = HUC6260_CYCLE_PER_LINE; // 320 pixels + 104 hblank
        ev->display[i].height = 262; // 262 pixels high

        ev->display[i].buf = NULL;
        ev->display[i].frame_num = 0;
    }

    ev->associated_display = this->vce.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_TG16_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("VCE events", DBG_TG16_CATEGORY_VCE);
    DEBUG_REGISTER_EVENT_CATEGORY("VDC0 events", DBG_TG16_CATEGORY_VDC0);

    cvec_grow_by(&ev->events, DBG_TG16_EVENT_MAX);
    cvec_lock_reallocs(&ev->events);
    DEBUG_REGISTER_EVENT("IRQ1", 0xFF4090, DBG_TG16_CATEGORY_CPU, DBG_TG16_EVENT_IRQ1, 1);
    DEBUG_REGISTER_EVENT("IRQ2", 0xFF4090, DBG_TG16_CATEGORY_CPU, DBG_TG16_EVENT_IRQ2, 1);
    DEBUG_REGISTER_EVENT("TIQ", 0xFF4090, DBG_TG16_CATEGORY_CPU, DBG_TG16_EVENT_TIQ, 1);
    DEBUG_REGISTER_EVENT("HSync Up", 0xFF0000, DBG_TG16_CATEGORY_VCE, DBG_TG16_EVENT_HSYNC_UP, 0);
    DEBUG_REGISTER_EVENT("VSync Up", 0x00FFFF, DBG_TG16_CATEGORY_VCE, DBG_TG16_EVENT_VSYNC_UP, 0);
    DEBUG_REGISTER_EVENT("CRAM write", 0x00FF00, DBG_TG16_CATEGORY_VCE, DBG_TG16_EVENT_WRITE_CRAM, 0);
    DEBUG_REGISTER_EVENT("VRAM write", 0x0000FF, DBG_TG16_CATEGORY_VDC0, DBG_TG16_EVENT_WRITE_VRAM, 0);
    DEBUG_REGISTER_EVENT("RCR write", 0xA0A020, DBG_TG16_CATEGORY_VDC0, DBG_TG16_EVENT_WRITE_RCR, 1);
    DEBUG_REGISTER_EVENT("RCR hit", 0x004090, DBG_TG16_CATEGORY_VDC0, DBG_TG16_EVENT_HIT_RCR, 1);
    DEBUG_REGISTER_EVENT("XScroll write", 0xFF00FF, DBG_TG16_CATEGORY_VDC0, DBG_TG16_EVENT_WRITE_XSCROLL, 1);
    DEBUG_REGISTER_EVENT("YScroll write", 0xFF00FF, DBG_TG16_CATEGORY_VDC0, DBG_TG16_EVENT_WRITE_YSCROLL, 1);

    SET_EVENT_VIEW(this->cpu);
    SET_EVENT_VIEW(this->vce);
    SET_EVENT_VIEW(this->vdc0);


#define SET_EVENT_ID(x, y) x .dbg.events. y = DBG_TG16_EVENT_ ## y
    SET_EVENT_ID(this->cpu, IRQ1);
    SET_EVENT_ID(this->cpu, IRQ2);
    SET_EVENT_ID(this->cpu, TIQ);

    SET_EVENT_ID(this->vce, HSYNC_UP);
    SET_EVENT_ID(this->vce, VSYNC_UP);
    SET_EVENT_ID(this->vce, WRITE_CRAM);

    SET_EVENT_ID(this->vdc0, WRITE_VRAM);
    SET_EVENT_ID(this->vdc0, WRITE_RCR);
    SET_EVENT_ID(this->vdc0, HIT_RCR);
    SET_EVENT_ID(this->vdc0, WRITE_XSCROLL);
    SET_EVENT_ID(this->vdc0, WRITE_YSCROLL);
#undef SET_EVENT_ID

    debugger_report_frame(dbgr);
    this->vce.dbg.interface = dbgr;
    this->vdc0.dbg.interface = dbgr;
    this->vdc0.dbg.evptr = ev;
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

static void setup_image_view_bg(struct TG16* this, struct debugger_interface *dbgr)
{
    // 1024x512 max size!
    struct debugger_view *dview;
    this->dbg.image_views.tiles = debugger_view_new(dbgr, dview_image);
    dview = cpg(this->dbg.image_views.tiles);
    struct image_view *iv = &dview->image;

    iv->width = 1024;
    iv->height = 512;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = (struct ivec2){ 0, 0 };
    iv->viewport.p[1] = (struct ivec2){ 1024, 512 };

    iv->update_func.ptr = this;
    iv->update_func.func = &render_image_view_bg;
    snprintf(iv->label, sizeof(iv->label), "Tilemap Viewer");
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
static void readcpumem(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = dest;
    for (u32 i = 0; i < 16; i++) {
        *out = TG16_bus_read(ptr, (addr + i) & 0x1FFFFF, 0, 0);
        out++;
    }
}

static void readvram(void *ptr, u32 addr, void *dest)
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



static void setup_memory_view(struct TG16* this, struct debugger_interface *dbgr) {
    this->dbg.memory = debugger_view_new(dbgr, dview_memory);
    struct debugger_view *dview = cpg(this->dbg.memory);
    struct memory_view *mv = &dview->memory;
    memory_view_add_module(dbgr, mv, "CPU Memory", 0, 6, 0, 0x1FFFFF, this, &readcpumem);
    memory_view_add_module(dbgr, mv, "VDC0 VRAM", 1, 4, 0, 0xFFFF, &this->vdc0.VRAM, &readvram);

}


void TG16J_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 0;
    dbgr->smallest_step = 1;

    setup_dbglog(dbgr, this);
    setup_events_view(this, dbgr, jsm);
    setup_memory_view(this, dbgr);
    setup_image_view_palettes(this, dbgr);
    setup_image_view_tiles(this, dbgr);
    setup_image_view_bg(this, dbgr);
}

