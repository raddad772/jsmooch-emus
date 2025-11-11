//
// Created by . on 4/23/25.
//

#ifndef JSMOOCH_EMUS_SNES_DEBUGGER_H
#define JSMOOCH_EMUS_SNES_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#undef DBG_EVENT
#define DBG_EVENT(x) debugger_report_event(snes->dbg.events.view, x)


#define BPP_2 0
#define BPP_4 1
#define BPP_8 2

enum SNES_DBLOG_CATEGORIES {
    SNES_CAT_UNKNOWN = 0,
    SNES_CAT_WDC_INSTRUCTION,
    SNES_CAT_WDC_READ,
    SNES_CAT_WDC_WRITE,
    SNES_CAT_DMA_START,
    SNES_CAT_DMA_WRITE,
    SNES_CAT_HDMA_WRITE,
    SNES_CAT_SPC_INSTRUCTION,
    SNES_CAT_SPC_READ,
    SNES_CAT_SPC_WRITE,
    SNES_CAT_PPU_VRAM_WRITE
};

#define DBG_SNES_CATEGORY_R5A22 0
#define DBG_SNES_CATEGORY_SPC700 1
#define DBG_SNES_CATEGORY_PPU 2


#define DBG_SNES_EVENT_IRQ 0
#define DBG_SNES_EVENT_NMI 1
#define DBG_SNES_EVENT_HDMA_START 2
#define DBG_SNES_EVENT_WRITE_VRAM 3
#define DBG_SNES_EVENT_WRITE_SCROLL 4
#define DBG_SNES_EVENT_WRITE_COLDATA 5
#define DBG_SNES_EVENT_HIRQ 6


#define DBG_SNES_EVENT_MAX 7


#define dbgloglog(wth, r_cat, r_severity, r_format, ...) if (wth->dbg.dvptr->ids_enabled[r_cat]) { dbglog_view_add_printf(wth->dbg.dvptr, r_cat, wth->clock.master_cycle_count, r_severity, r_format, __VA_ARGS__); dbglog_view_extra_printf(wth->dbg.dvptr, ""); }

void SNESJ_setup_debugger_interface(jsm_system *, debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_SNES_DEBUGGER_H
