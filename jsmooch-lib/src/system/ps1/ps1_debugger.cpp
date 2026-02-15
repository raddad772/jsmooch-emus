//
// Created by . on 2/16/25.
//

#include "ps1_debugger.h"
#include "ps1_bus.h"

namespace PS1 {

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
    dbglog_category_node &r3000 = root.add_node(dv, "R3000", nullptr, 0, 0);
    r3000.children.reserve(10);
    r3000.add_node(dv, "Instructions", "R3000", PS1_CAT_R3000_INSTRUCTION, 0x80FF80);
    th->cpu.dbg.dvptr = &dv;
    th->cpu.dbg.dv_id = PS1_CAT_R3000_INSTRUCTION;
    th->cpu.trace.rfe_id = PS1_CAT_R3000_RFE;
    th->cpu.trace.exception_id = PS1_CAT_R3000_EXCEPTION;
    //r3000.add_node(dv, "IRQs", "IRQ", PS1_CAT_R3000_IRQ, 0xFFFFFF);
    r3000.add_node(dv, "RFEs", "RFE", PS1_CAT_R3000_RFE, 0xFFFFFF);
    r3000.add_node(dv, "Exceptions", "Except", PS1_CAT_R3000_EXCEPTION, 0xFFFFFF);
}

static void setup_waveforms(core& th, debugger_interface *dbgr) {
    th.dbg.waveforms.view = dbgr->make_view(dview_waveforms);
    auto *dview = &th.dbg.waveforms.view.get();
    auto *wv = &dview->waveform;
    snprintf(wv->name, sizeof(wv->name), "SPU");
    wv->waveforms.reserve(8);

    wv->waveforms.reserve(32);

    debug_waveform *dw = &wv->waveforms.emplace_back();
    th.dbg.waveforms.main.make(wv->waveforms, wv->waveforms.size()-1);

    snprintf(dw->name, sizeof(dw->name), "Output (Mono)");
    dw->kind = dwk_main;
    dw->samples_requested = 400;
    dw->default_clock_divider = 768;

    for (u32 i = 0; i < 24; i++) {
        dw = &wv->waveforms.emplace_back();
        dw->kind = dwk_channel;
        dw->samples_requested = 200;
        snprintf(dw->name, sizeof(dw->name), "CH%d", i);

        th.dbg.waveforms.chan[i].make(wv->waveforms, wv->waveforms.size()-1);
    }
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
}
}