//
// Created by . on 4/23/25.
//

#ifndef JSMOOCH_EMUS_SNES_DEBUGGER_H
#define JSMOOCH_EMUS_SNES_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

enum SNES_DBLOG_CATEGORIES {
    SNES_CAT_UNKNOWN = 0,
    SNES_CAT_WDC_INSTRUCTION,
    SNES_CAT_WDC_READ,
    SNES_CAT_WDC_WRITE,
    SNES_CAT_SPC_INSTRUCTION,
    SNES_CAT_SPC_READ,
    SNES_CAT_SPC_WRITE
};

#define dbgloglog(wth, r_cat, r_severity, r_format, ...) if (wth->dbg.dvptr->ids_enabled[r_cat]) { dbglog_view_add_printf(wth->dbg.dvptr, r_cat, wth->clock.master_cycle_count, r_severity, r_format, __VA_ARGS__); dbglog_view_extra_printf(wth->dbg.dvptr, ""); }

void SNESJ_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_SNES_DEBUGGER_H
