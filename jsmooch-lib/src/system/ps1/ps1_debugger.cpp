//
// Created by . on 2/16/25.
//

#include "helpers/color.h"
#include "ps1_debugger.h"
#include "ps1_bus.h"

namespace PS1 {

static void render_image_view_sysinfo(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    tb->sprintf("\nEnabled IRQs:");
    for (u32 i = 0; i < 11; i++) {
        u32 b = 1 << i;
        if (th->cpu.io.I_MASK & b) tb->sprintf("\n %s", IRQnames[i]);
    }
}

static void render_image_view_vram(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    if (th->clock.master_frame == 0) return;
    image_view *iv = &dview->image;
    iv->draw_which_buf ^= 1;
    u32 *outbuf = static_cast<u32 *>(iv->img_buf[iv->draw_which_buf].ptr);
    memset(outbuf, 0, 1280*512*4);

    u8 *a = th->gpu.VRAM;
    u16 *gbao = reinterpret_cast<u16 *>(a);
    u32 *img32 = outbuf;

    for (u32 ry = 0; ry < 512; ry++) {
        u32 y = ry;
        u32 *line_out_ptr = img32 + (ry * out_width);
        for (u32 rx = 0; rx < 1024; rx++) {
            u32 x = rx;
            u32 di = ((y * 1024) + x);

            u32 color = ps1_to_screen(gbao[di]);
            *line_out_ptr = color;
            line_out_ptr++;
        }
    }
}


void setup_console_view(core* th, debugger_interface *dbgr)
{
    th->dbg.console_view = dbgr->make_view(dview_console);
    debugger_view *dview = &th->dbg.console_view.get();

    console_view *cv = &dview->console;

    snprintf(cv->name, sizeof(cv->name), "System Console");

    th->cpu.dbg.console = cv;
}

static void setup_dbglog(debugger_interface *dbgr, core *th)
{
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th->dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = true;

    dbglog_category_node &root = dv.get_category_root();
    root.children.reserve(10);
    dbglog_category_node &r3000 = root.add_node(dv, "R3000", nullptr, 0, 0x80FF80);
    r3000.children.reserve(10);
    r3000.add_node(dv, "Instructions", "R3000", PS1D_R3000_INSTRUCTION, 0x80FF80);
    th->cpu.dbg.dvptr = &dv;
    th->cpu.dbg.dv_id = PS1D_R3000_INSTRUCTION;
    th->cpu.trace.rfe_id = PS1D_R3000_RFE;
    th->cpu.trace.exception_id = PS1D_R3000_EXCEPTION;
    th->cpu.trace.I_MASK_write =
    th->cpu.trace.I_STAT_write = PS1D_BUS_IRQ_ACK;;
    //r3000.add_node(dv, "IRQs", "IRQ", PS1D_R3000_IRQ, 0xFFFFFF);
    r3000.add_node(dv, "RFEs", "RFE", PS1D_R3000_RFE, 0xFFFFFF);
    r3000.add_node(dv, "Exceptions", "Except", PS1D_R3000_EXCEPTION, 0xFFFFFF);

    u32 dma_c = 0xFF2080;
    dbglog_category_node &dma = root.add_node(dv, "DMA", nullptr, 0, dma_c);
    dma.children.reserve(10);
    dma.add_node(dv, "DMA0 MDEC in", "DMA0", PS1D_DMA_CH0, dma_c);
    dma.add_node(dv, "DMA1 MDEC out", "DMA1", PS1D_DMA_CH1, dma_c);
    dma.add_node(dv, "DMA2 GPU", "DMA2", PS1D_DMA_CH2, dma_c);
    dma.add_node(dv, "DMA3 CDROM", "DMA3", PS1D_DMA_CH3, dma_c);
    dma.add_node(dv, "DMA4 SPU", "DMA4", PS1D_DMA_CH4, dma_c);
    dma.add_node(dv, "DMA5 PIO", "DMA5", PS1D_DMA_CH5, dma_c);
    dma.add_node(dv, "DMA6 OTC", "DMA6", PS1D_DMA_CH6, dma_c);

    // CDROM STAT IO, IF/IEs
    // DMA IF/IE
    // IF/IE
    // SPUCNT/STAT
    dma_c = 0x20A0A0;
    dbglog_category_node &cdrom = root.add_node(dv, "CDROM Drive", nullptr, 0, dma_c);
    cdrom.children.reserve(30);
    cdrom.add_node(dv, "Command", "CMD", PS1D_CDROM_CMD, dma_c);
    cdrom.add_node(dv, "CMD SetMode", "SetMode", PS1D_CDROM_SETMODE, dma_c);
    cdrom.add_node(dv, "CMD Read", "Read", PS1D_CDROM_READ, dma_c);
    cdrom.add_node(dv, "CMD Play", "Play", PS1D_CDROM_PLAY, dma_c);
    cdrom.add_node(dv, "CMD Pause", "Pause", PS1D_CDROM_PAUSE, dma_c);
    cdrom.add_node(dv, "CMD SetLoc", "SetLoc", PS1D_CDROM_SETLOC, dma_c);
    cdrom.add_node(dv, "CMD Reset", "Reset", PS1D_CDROM_CMD_RESET, dma_c);
    cdrom.add_node(dv, "Result", "Result", PS1D_CDROM_RESULT, dma_c);
    cdrom.add_node(dv, "IRQ Queued", "IRQ_Q", PS1D_CDROM_IRQ_QUEUE, dma_c);
    cdrom.add_node(dv, "IRQ Asserted", "IRQ_ASRT", PS1D_CDROM_IRQ_ASSERT, dma_c);
    cdrom.add_node(dv, "Sector read", "R.Sector", PS1D_CDROM_SECTOR_READS, dma_c);
    cdrom.add_node(dv, "RDDATA Start", "RDDATA_start", PS1D_CDROM_RDDATA_START, dma_c);
    cdrom.add_node(dv, "RDDATA Finish", "RDDATA_finish", PS1D_CDROM_RDDATA_FINISH, dma_c);

    dma_c = 0x7042FF;
    dbglog_category_node &bus = root.add_node(dv, "General", nullptr, 0, dma_c);
    bus.children.reserve(10);
    bus.add_node(dv, "IRQs", "IRQ", PS1D_BUS_IRQS, dma_c);
    bus.add_node(dv, "IRQ ACKs", "IRQ ACK", PS1D_BUS_IRQ_ACK, dma_c);
    bus.add_node(dv, "Console Logs", "Console", PS1D_BUS_CONSOLE, dma_c);
    th->cpu.trace.console_log_id = PS1D_BUS_CONSOLE;
}

static void setup_waveforms(core& th, debugger_interface *dbgr) {
    th.dbg.waveforms2.view = dbgr->make_view(dview_waveform2);
    auto *dview = &th.dbg.waveforms2.view.get();
    auto *wv = &dview->waveform2;
    snprintf(wv->name, sizeof(wv->name), "SPU");
    auto &root = wv->root;
    root.children.reserve(10);

    th.dbg.waveforms2.main = &root;
    th.dbg.waveforms2.main_cache = &root.data;
    snprintf(root.data.name, sizeof(root.data.name), "Stereo Out");
    root.data.kind = debug::waveform2::wk_big;
    root.data.samples_requested = 400;
    root.data.stereo = true;

    auto &main = root.add_child_category("Voices", 32);
    for (u32 i = 0; i < 24; i++) {
        auto *v = main.add_child_wf(debug::waveform2::wk_small, th.dbg.waveforms2.channels.chan[i]);
        auto *ch = &v->data;
        th.dbg.waveforms2.channels.chan_cache[i] = &v->data;
        snprintf(ch->name, sizeof(ch->name), "CH%d", i);
    }

    auto &cd = root.add_child_category("CD Audio", 2);
    auto *v = cd.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.cd.chan[0]);
    th.dbg.waveforms2.cd.chan_cache[0] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Output");

    auto &reverb = root.add_child_category("Reverb Processing", 8);
    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[0]);
    th.dbg.waveforms2.reverb.chan_cache[0] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Reverb In");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[1]);
    th.dbg.waveforms2.reverb.chan_cache[1] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Reverb Out");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[2]);
    th.dbg.waveforms2.reverb.chan_cache[2] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Debug");

    v = reverb.add_child_wf(debug::waveform2::wk_medium, th.dbg.waveforms2.reverb.chan[2]);
    th.dbg.waveforms2.reverb.chan_cache[3] = &v->data;
    v->data.stereo = true;
    snprintf(v->data.name, sizeof(v->data.name), "Debug2");
}

static void setup_image_view_vram(core* th, debugger_interface *dbgr) {
    th->dbg.image_views.vram = dbgr->make_view(dview_image);
    auto *dview = &th->dbg.image_views.vram.get();
    image_view *iv = &dview->image;
    iv->width = 1280;
    iv->height = 512;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = ivec2( 0, 0 );
    iv->viewport.p[1] = ivec2( iv->width, iv->height );

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_vram;
    snprintf(iv->label, sizeof(iv->label), "VRAM view");
}

static void setup_image_view_sysinfo(core* th, debugger_interface *dbgr) {
    debugger_view *dview;
    th->dbg.image_views.sysinfo = dbgr->make_view(dview_image);
    dview = &th->dbg.image_views.sysinfo.get();
    image_view *iv = &dview->image;

    iv->width = 10;
    iv->height = 10;
    iv->viewport.exists = true;
    iv->viewport.enabled = true;
    iv->viewport.p[0] = (ivec2){ 0, 0 };
    iv->viewport.p[1] = (ivec2){ 10, 10 };

    iv->update_func.ptr = th;
    iv->update_func.func = &render_image_view_sysinfo;

    snprintf(iv->label, sizeof(iv->label), "Sys Info View");

    debugger_widgets_add_textbox(dview->options, "blah!", 1);
}

void core::setup_debugger_interface(debugger_interface &dbgr)
{
    dbg.interface = &dbgr;
    dbgr.views.reserve(15);

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 1;
    setup_console_view(this, &dbgr);
    setup_dbglog(&dbgr, this);
    setup_waveforms(*this, &dbgr);
    setup_image_view_sysinfo(this, &dbgr);
    setup_image_view_vram(this, &dbgr);

}
}