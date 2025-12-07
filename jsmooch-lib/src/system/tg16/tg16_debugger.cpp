//
// Created by . on 6/18/25.
//

#include <cstring>

#include "helpers/color.h"

#include "tg16_debugger.h"
#include "tg16_bus.h"
#include "component/gpu/huc6260/huc6260.h"
#include "component/cpu/huc6280/huc6280_disassembler.h"

#define PAL_BOX_SIZE 10
#define PAL_BOX_SIZE_W_BORDER 11
namespace TG16 {
static inline u16 read_VRAM(HUC6270::chip *th, u32 addr)
{
    return (addr & 0x8000) ? 0 : th->VRAM[addr];
}


static void render_image_view_sys_info(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto *th = static_cast<TG16::core *>(ptr);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
#define tbp(...) tb->sprintf(__VA_ARGS__)
#define YN(x) (x) ? "yes" : "no"
    tbp("CPU\n~~~~~\n");
    tbp("VDP\n~~~~~~~\n");
    tbp("H: SyncWindow:%d(%02X)  WaitDisplay:%d(%02X)  Display:%d(%02X)  WaitSync:%d(%02X)\n",
        (th->vdc0.io.HSW+1)<<3, th->vdc0.io.HSW,
        (th->vdc0.io.HDS+1)<<3, th->vdc0.io.HDS,
        (th->vdc0.io.HDW+1)<<3, th->vdc0.io.HDW,
        (th->vdc0.io.HDE+1)<<3, th->vdc0.io.HDE
        );
    tbp("V: SyncWindow:%d(%02X)  WaitDisplay:%d(%02X)  Display:%d(%02X)  WaitSync:%d(%02X)\n",
        th->vdc0.io.VSW+1, th->vdc0.io.VSW,
        th->vdc0.io.VDS+2, th->vdc0.io.VDS,
        th->vdc0.io.VDW.u+2, th->vdc0.io.VDW.u,
        th->vdc0.io.VCR, th->vdc0.io.VCR
    );

    tbp("Enabled IRQs:  CollisionDetect:%c  SpriteOverflow:%c  ScanlineCount:%c  VRAM->SATB:%c  VRAM->VRAM:%c  vblank:%c\n",
        th->vdc0.regs.IE & 1 ? 'Y' : 'N',
        th->vdc0.regs.IE & 2 ? 'Y' : 'N',
        th->vdc0.regs.IE & 4 ? 'Y' : 'N',
        th->vdc0.regs.IE & 8 ? 'Y' : 'N',
        th->vdc0.regs.IE & 0x10 ? 'Y' : 'N',
        th->vdc0.regs.IE & 0x20 ? 'Y' : 'N'
        );
    tbp("RCR:%02x/%d(%d)", th->vdc0.io.RCR.u, th->vdc0.io.RCR.u, th->vdc0.io.RCR.u-64);
}


static void render_image_view_bg(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width)
{
    auto *th = static_cast<TG16::core *>(ptr);
    if (th->vce.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * 4 * 512);
    static const u32 screen_sizes[8][2] = { {32, 32}, {64, 32},
                                            {128, 32}, {128, 32},
                                            {32, 64}, {64, 64},
                                            {128, 64}, {128,64}
    };

    u32 xtsize = th->vdc0.bg.x_tiles;
    u32 ytsize = th->vdc0.bg.y_tiles;

    for (u32 ty = 0; ty < ytsize; ty++) {
        for (u32 tx = 0; tx < xtsize; tx++) {
            for (u32 in_tile_y = 0; in_tile_y < 8; in_tile_y++) {
                u32 screen_y = (ty << 3) + in_tile_y;
                u32 *screen_y_ptr = outbuf + (screen_y * out_width);
                u32 addr = (ty * xtsize) + tx;
                u32 entry = read_VRAM(&th->vdc0, addr);
                addr = ((entry & 0xFFF) << 4) + in_tile_y;
                u32 palette = (((entry >> 12) & 15) << 4);
                u32 plane12 = read_VRAM(&th->vdc0, addr);
                u32 plane34 = read_VRAM(&th->vdc0, addr+8);
                u32 pixel_shifter = tg16_decode_line(plane12, plane34);

                for (u32 in_tile_x = 0; in_tile_x < 8; in_tile_x++) {
                    u32 screen_x = (tx << 3) + in_tile_x;
                    u32 c = pixel_shifter & 15;
                    //if (c != 0) {
                        pixel_shifter >>= 4;
                        c |= palette;
                        c = th->vce.CRAM[c];
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

static void render_image_view_tiles(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<TG16::core *>(ptr);
    if (th->vce.master_frame == 0) return;

    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    u32 tn = 0;
    memset(outbuf, 0, out_width * 4 * 512);
    for (u32 ty = 0; ty < 64; ty++) {
        u32 tile_screen_y = ty * 8;
        for (u32 tx = 0; tx < 32; tx++) {
            tn = (ty * 32) + tx;
            u32 tile_screen_x = tx * 8;
            u32 addr = tn << 4;
            for (u32 inner_y = 0; inner_y < 8; inner_y++) {
                // plane 1 & 2 are at addr
                // 3&4 are at addr + 8
                u32 screen_y = tile_screen_y + inner_y;
                u32 *screen_y_ptr = outbuf + (screen_y * out_width);
                u32 plane12 = th->vdc0.VRAM[addr & 0x7FFF];
                u32 plane34 = th->vdc0.VRAM[(addr+8) & 0x7FFF];
                addr++;
                u32 screen_x = tile_screen_x;
                // lower byte is 1,3
                // higher byte is 2,4
                // lowest bit is leftmost pixel?
                u32 px = tg16_decode_line(plane12, plane34);
                for (u32 inner_x = 0; inner_x < 8; inner_x++) {
                    u32 v = 17 * (px & 15);
                    px >>= 4;
                    screen_y_ptr[screen_x] = v | (v << 8) | (v << 16) | 0xFF000000;
                    screen_x++;
                }
            }
        }
    }
}

static void render_image_view_palette(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<TG16::core *>(ptr);
    if (th->vce.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, out_width * (((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2)); // Clear out at least 4 rows worth

    u32 offset = 0;
    u32 y_offset = 0;
    for (u32 lohi = 0; lohi < 0x200; lohi += 0x100) {
        for (u32 palette = 0; palette < 16; palette++) {
            for (u32 color = 0; color < 16; color++) {
                u32 y_top = y_offset + offset;
                u32 x_left = color * PAL_BOX_SIZE_W_BORDER;
                u32 c = tg16_to_screen(th->vce.CRAM[lohi + (palette << 4) + color]);
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

static void setup_events_view(TG16::core &th, debugger_interface *dbgr)
{
    th.dbg.events.view = dbgr->make_view(dview_events);
    debugger_view *dview = &th.dbg.events.view.get();
    events_view &ev = dview->events;

    ev.timing = ev_timing_master_clock;
    ev.master_clocks.per_line = HUC6260::CYCLE_PER_LINE;
    ev.master_clocks.height = 262;
    ev.master_clocks.ptr = &th.clock.master_cycles;

    for (auto & i : ev.display) {
        i.width = HUC6260::CYCLE_PER_LINE; // 320 pixels + 104 hblank
        i.height = 262; // 262 pixels high

        i.buf = nullptr;
        i.frame_num = 0;
    }

    ev.associated_display = th.vce.display_ptr;

    DEBUG_REGISTER_EVENT_CATEGORY("CPU events", DBG_TG16_CATEGORY_CPU);
    DEBUG_REGISTER_EVENT_CATEGORY("VCE events", DBG_TG16_CATEGORY_VCE);
    DEBUG_REGISTER_EVENT_CATEGORY("VDC0 events", DBG_TG16_CATEGORY_VDC0);

    ev.events.reserve(ev.events.size() + DBG_TG16_EVENT_MAX);
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

    SET_EVENT_VIEW(th.cpu);
    SET_EVENT_VIEW(th.vce);
    SET_EVENT_VIEW(th.vdc0);

#define SET_EVENT_ID(x, y) x .dbg.events. y = DBG_TG16_EVENT_ ## y
    SET_EVENT_ID(th.cpu, IRQ1);
    SET_EVENT_ID(th.cpu, IRQ2);
    SET_EVENT_ID(th.cpu, TIQ);

    SET_EVENT_ID(th.vce, HSYNC_UP);
    SET_EVENT_ID(th.vce, VSYNC_UP);
    SET_EVENT_ID(th.vce, WRITE_CRAM);

    SET_EVENT_ID(th.vdc0, WRITE_VRAM);
    SET_EVENT_ID(th.vdc0, WRITE_RCR);
    SET_EVENT_ID(th.vdc0, HIT_RCR);
    SET_EVENT_ID(th.vdc0, WRITE_XSCROLL);
    SET_EVENT_ID(th.vdc0, WRITE_YSCROLL);
#undef SET_EVENT_ID

    debugger_report_frame(dbgr);
    th.vce.dbg.interface = dbgr;
    th.vdc0.dbg.interface = dbgr;
    th.vdc0.dbg.evptr = &ev;
}


static void setup_dbglog(debugger_interface *dbgr, TG16::core *th)
{
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th->dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = 1;

    u32 cpu_color = 0x8080FF;
    dbglog_category_node &root = dv.get_category_root();
    root.children.reserve(15);
    dbglog_category_node &cpu = root.add_node(dv, "CPU (HuC6280)", nullptr, 0, 0);
    cpu.children.reserve(15);
    cpu.add_node(dv, "Instruction Trace", "CPU", TG16_CAT_CPU_INSTRUCTION, cpu_color);
    th->cpu.dbg.dvptr = &dv;
    th->cpu.dbg.dv_id = TG16_CAT_CPU_INSTRUCTION;
    cpu.add_node(dv, "Reads", "cpu_read", TG16_CAT_CPU_READ, cpu_color);
    cpu.add_node(dv, "Writes", "cpu_write", TG16_CAT_CPU_WRITE, cpu_color);
    th->cpu.trace.dbglog.id_read = TG16_CAT_CPU_READ;
    th->cpu.trace.dbglog.id_write = TG16_CAT_CPU_WRITE;

    cpu.add_node(dv, "IRQs", "CPU", TG16_CAT_CPU_IRQS, 0xA0AF80);
}

static void setup_image_view_bg(TG16::core *th, debugger_interface *dbgr)
{
    // 1024x512 max size!
    debugger_view *dview;
    th->dbg.image_views.tiles = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.tiles.get();
    image_view *iv = &dview->image;

    iv->width = 1024;
    iv->height = 512;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 1024, 512 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_bg;
    snprintf(iv->label, sizeof(iv->label), "Tilemap Viewer");
}


static void setup_image_view_tiles(TG16::core *th, debugger_interface *dbgr)
{
    debugger_view *dview;
    th->dbg.image_views.tiles = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.tiles.get();
    image_view *iv = &dview->image;

    iv->width = 512;
    iv->height = 512;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 512, 512 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_tiles;
    snprintf(iv->label, sizeof(iv->label), "Tile Viewer");
}

static void setup_image_view_palettes(TG16::core *th, debugger_interface *dbgr) {
    debugger_view *dview;
    th->dbg.image_views.palettes = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.palettes.get();
    image_view *iv = &dview->image;

    iv->width = 16 * PAL_BOX_SIZE_W_BORDER;
    iv->height = ((16 * PAL_BOX_SIZE_W_BORDER) * 2) + 2;

    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = ivec2(iv->width, iv->height);

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_palette;
    snprintf(iv->label, sizeof(iv->label), "Palettes Viewer");

}

// 64x64 tiles
// aka 512x512px
static void readcpumembus(void *ptr, u32 addr, void *dest)
{
    // Read 16 bytes from addr into dest
    u8 *out = static_cast<u8 *>(dest);
    for (u32 i = 0; i < 16; i++) {
        *out = TG16::core::huc_read_mem(ptr, (addr + i) & 0x1FFFFF, 0, false);
        out++;
    }
}

static void readcpumemnative(void *ptr, u32 addr, void *dest)
{
    auto *th = static_cast<TG16::core *>(ptr);
    // Read 16 bytes from addr into dest
    u8 *out = static_cast<u8 *>(dest);
    for (u32 i = 0; i < 16; i++) {

        u32 raddr = (addr+i) & 0xFFFF;
        raddr = th->cpu.regs.MPR[raddr >> 13] | (raddr & 0x1FFF);
        *out = TG16::core::huc_read_mem(ptr, raddr & 0x1FFFFF, 0, false);
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

static void setup_waveforms_psg(TG16::core *th, debugger_interface *dbgr)
{
    th->dbg.waveforms_psg.view = dbgr->make_view(dview_waveforms);
    auto *dview = &th->dbg.waveforms_psg.view.get();
    auto *wv = &dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "Huc6280 PSG");

    wv->waveforms.reserve(10);

    debug_waveform *dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.main.make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Output");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[0].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 1");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[1].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 2");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[2].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 3");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[3].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 4");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[4].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 5");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;

    dw = &wv->waveforms.emplace_back();
    th->dbg.waveforms_psg.chan[5].make(wv->waveforms, wv->waveforms.size()-1);
    snprintf(dw->name, sizeof(dw->name), "Ch 6");
    dw->kind = dwk_channel;
    dw->samples_requested = 200;
    dw->is_unsigned = true;
    printf("\nWAVEFORMS INIT'D");
}


static void setup_memory_view(TG16::core *th, debugger_interface *dbgr) {
    th->dbg.memory = dbgr->make_view(dview_memory);
    debugger_view *dview = &th->dbg.memory.get();
    memory_view *mv = &dview->memory;
    mv->add_module("CPU Bus (21bit)", 0, 6, 0, 0x1FFFFF, th, &readcpumembus);
    mv->add_module("CPU View (16bit)", 1, 6, 0, 0xFFFF, th, &readcpumemnative);
    mv->add_module("VDC0 VRAM", 2, 4, 0, 0xFFFF, &th->vdc0.VRAM, &readvram);

}

static int render_p(cpu_reg_context *ctx, void *outbuf, size_t outbuf_sz)
{
    u32 val = ctx->int32_data;
    return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%c%c%c%c%c%c%c%c",
                    val & 0x80 ? 'N' : 'n',
                    val & 0x40 ? 'V' : 'v',
                    val & 0x20 ? 'T' : 't',
                    val & 0x10 ? 'B' : 'b',
                    val & 8 ? 'D' : 'd',
                    val & 4 ? 'I' : 'i',
                    val & 2 ? 'Z' : 'z',
                    val & 1 ? 'C' : 'c');
}

static int render_mpr(cpu_reg_context *ctx, void *outbuf, size_t outbuf_sz)
{
    switch (ctx->int32_data) {
        case 0xF7: // SRAM
            return snprintf(static_cast<char *>(outbuf), outbuf_sz, "F7 (SRAM)");
        case 0xF8: // RAM
        case 0xF9: // RAM
        case 0xFA: // RAM
        case 0xFB: // RAM
            return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%02X (RAM)", ctx->int32_data);
        case 0xFF: // CPUIO
            return snprintf(static_cast<char *>(outbuf), outbuf_sz, "FF (IO)");
        default: {// ROM/empty
            u32 lowaddr = ctx->int32_data << 13;
            return snprintf(static_cast<char *>(outbuf), outbuf_sz, "%02X (ROM $%06X-$%06X)", ctx->int32_data, lowaddr, lowaddr + 0xFFF); }
    }
}

static void fill_disassembly_view(void *tg16ptr, disassembly_view &dview)
{
    auto *th = static_cast<TG16::core *>(tg16ptr);

    th->dbg.dasm.A->int8_data = th->cpu.regs.A;
    th->dbg.dasm.X->int8_data = th->cpu.regs.X;
    th->dbg.dasm.Y->int8_data = th->cpu.regs.Y;
    th->dbg.dasm.PC->int16_data = th->cpu.regs.PC;
    th->dbg.dasm.S->int8_data = th->cpu.regs.S;
    th->dbg.dasm.P->int8_data = th->cpu.regs.P.u;
    for (u32 i = 0; i < 8; i++) {
        th->dbg.dasm.MPR[i]->int32_data = th->cpu.regs.MPR[i] >> 13;
    }

}

static void create_and_bind_registers(TG16::core *th, disassembly_view *dv)
{
    u32 tkindex = 0;
    dv->cpu.regs.reserve( 20);
    cpu_reg_context *rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "A");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "X");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "Y");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "PC");
    rg->kind = RK_int16;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "S");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = nullptr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "P");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_p;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR0");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR1");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR2");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR3");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR4");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR5");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR6");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;

    rg= & dv->cpu.regs.emplace_back();
    snprintf(rg->name, sizeof(rg->name), "MPR7");
    rg->kind = RK_int8;
    rg->index = tkindex++;
    rg->custom_render = &render_mpr;


#define BIND(dn, index) th->dbg.dasm. dn = &dv->cpu.regs[index]
    BIND(A, 0);
    BIND(X, 1);
    BIND(Y, 2);
    BIND(PC, 3);
    BIND(S, 4);
    BIND(P, 5);
    BIND(MPR[0], 6);
    BIND(MPR[1], 7);
    BIND(MPR[2], 8);
    BIND(MPR[3], 9);
    BIND(MPR[4], 10);
    BIND(MPR[5], 11);
    BIND(MPR[6], 12);
    BIND(MPR[7], 13);
#undef BIND
}

static disassembly_vars get_disassembly_vars(void *tg16ptr, disassembly_view &dv)
{
    auto *th = static_cast<TG16::core *>(tg16ptr);
    disassembly_vars dvar;
    dvar.address_of_executing_instruction = th->cpu.PCO;
    dvar.current_clock_cycle = th->clock.master_cycles;
    return dvar;
}

static void get_dissasembly(void *tg16ptr, disassembly_view &dview, disassembly_entry &entry)
{
    auto *th = static_cast<TG16::core *>(tg16ptr);
    HUC6280::disassemble_entry(th->cpu, entry);
}

static int print_disassembly_addr(void *ptr, u32 addr, char *out, size_t sz_out)
{
    auto *th = static_cast<TG16::core *>(ptr);
    addr &= 0xFFFF;
    u32 mprnum = addr >> 13;
    u32 bank = th->cpu.regs.MPR[mprnum] >> 13;
    switch (bank) {
        case 0xF7: // SRAM
            return snprintf(out, sz_out, "F7:%04X (SRAM)", addr);
        case 0xF8: // RAM
        case 0xF9: // RAM
        case 0xFA: // RAM
        case 0xFB: // RAM
            return snprintf(out, sz_out, "%02X:%04X (RAM)", bank, addr);
        case 0xFF: // CPUIO
            return snprintf(out, sz_out, "FF:%04X (IO)", addr);
        default: {// ROM/empty
            u32 lowaddr = bank << 13;
            return snprintf(out, sz_out, "%02X:%04X (ROM $%06X)", bank, addr, lowaddr + (addr & 0xFFF)); }
    }
}

static void setup_disassembly_view(TG16::core *th, debugger_interface *dbgr)
{
    cvec_ptr p = dbgr->make_view(dview_disassembly);
    debugger_view *dview = &p.get();
    disassembly_view *dv = &dview->disassembly;
    dv->addr_column_size = 4;
    dv->has_context = 0;
    dv->processor_name.sprintf("HuC6280");

    create_and_bind_registers(th, dv);
    dv->mem_start = 0;
    dv->mem_end = 0xFFFF;

    dv->print_addr.ptr = th;
    dv->print_addr.func = &print_disassembly_addr;

    dv->fill_view.ptr = th;
    dv->fill_view.func = &fill_disassembly_view;

    dv->get_disassembly.ptr = th;
    dv->get_disassembly.func = &get_dissasembly;

    dv->get_disassembly_vars.ptr = th;
    dv->get_disassembly_vars.func = &get_disassembly_vars;
}

static void setup_image_view_sys_info(TG16::core *th, debugger_interface *dbgr)
{
    th->dbg.image_views.sys_info = dbgr->make_view(dview_image);
    debugger_view *dview = &th->dbg.image_views.sys_info.get();;
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = 1;
    iv->viewport.enabled = 1;
    iv->viewport.p[0] = ivec2(0, 0);
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sys_info;

    snprintf(iv->label, sizeof(iv->label), "Sys Info");

    debugger_widgets_add_textbox(dview->options, "blah!", 1);

}

void TG16::core::setup_debugger_interface(debugger_interface &dbgr) {
    dbg.interface = &dbgr;
    dbgr.views.reserve(15);

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 1;

    setup_disassembly_view(this, &dbgr);
    setup_dbglog(&dbgr, this);
    setup_events_view(*this, &dbgr);
    setup_memory_view(this, &dbgr);
    setup_waveforms_psg(this, &dbgr);
    setup_image_view_palettes(this, &dbgr);
    setup_image_view_tiles(this, &dbgr);
    setup_image_view_bg(this, &dbgr);
    setup_image_view_sys_info(this, &dbgr);
}

}