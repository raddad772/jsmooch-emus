//
// Created by . on 2/16/25.
//

#include "ps1_debugger.h"
#include "ps1_bus.h"

#define JTHIS struct PS1* this = (struct PS1*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct PS1* this

static void setup_console_view(struct PS1* this, struct debugger_interface *dbgr)
{
    struct debugger_view *dview;
    this->dbg.console_view = debugger_view_new(dbgr, dview_console);
    dview = (struct debugger_view *)cvec_get(this->dbg.console_view.vec, this->dbg.console_view.index);

    struct console_view *cv = &dview->console;

    snprintf(cv->name, sizeof(cv->name), "System Console");

    this->cpu.dbg.console = cv;
}

void PS1J_setup_debugger_interface(struct jsm_system *jsm, struct debugger_interface *dbgr)
{
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 1;
    setup_console_view(this, dbgr);
}
