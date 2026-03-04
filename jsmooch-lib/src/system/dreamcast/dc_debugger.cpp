//
// Created by . on 3/2/26.
//

#include "dc_bus.h"
#include "dc_debugger.h"

namespace DC {

static void render_image_view_sysinfo(debugger_interface *dbgr, debugger_view *dview, void *ptr, u32 out_width) {
    auto *th = static_cast<core *>(ptr);
    //memset(ptr, 0, out_width * 4 * 10);
    debugger_widget_textbox *tb = &dview->options[0].textbox;
    tb->clear();
    tb->sprintf("\nHELLO");
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

static void setup_dbglog(debugger_interface *dbgr, core *th) {
    cvec_ptr p = dbgr->make_view(dview_dbglog);
    debugger_view *dview = &p.get();
    dbglog_view &dv = dview->dbglog;
    th->dbg.dvptr = &dv;
    snprintf(dv.name, sizeof(dv.name), "Trace");
    dv.has_extra = true;

    dbglog_category_node &root = dv.get_category_root();
    root.children.reserve(10);
    dbglog_category_node &sh4 = root.add_node(dv, "SH4", nullptr, 0, 0x80FF80);
    sh4.children.reserve(10);
    sh4.add_node(dv, "Instructions", "SH4", DCD_SH4_INSTRUCTION, 0x80FF80);
    th->sh4.dbg.dvptr = &dv;
    th->sh4.dbg.dv_id = DCD_SH4_INSTRUCTION;
    th->sh4.trace.exception_id = DCD_SH4_EXCEPTION;
    //sh4.add_node(dv, "IRQs", "IRQ", PS1D_R3000_IRQ, 0xFFFFFF);
    sh4.add_node(dv, "Exceptions", "Except", DCD_SH4_EXCEPTION, 0xFFFFFF);

    u32 dma_c = 0x20E0A0;
    dbglog_category_node &bus = root.add_node(dv, "Main Bus", nullptr, 0, dma_c);
    bus.children.reserve(10);
    bus.add_node(dv, "Reads", "Read", DCD_MAINBUS_READ, dma_c);
    bus.add_node(dv, "Writes", "Write", DCD_MAINBUS_WRITE, dma_c);
}

void core::setup_debugger_interface(debugger_interface &dbgr) {
    printf("\nDEBUGGER SETUP!");
    dbg.interface = &dbgr;
    dbgr.views.reserve(15);

    dbgr.supported_by_core = true;
    dbgr.smallest_step = 1;

    setup_dbglog(&dbgr, this);
    setup_image_view_sysinfo(this, &dbgr);

}

}