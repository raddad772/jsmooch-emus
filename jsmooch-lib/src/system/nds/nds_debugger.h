//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_NDS_DEBUGGER_H
#define JSMOOCH_EMUS_NDS_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#define DBG_NDS_CATEGORY_PPU 0
#define DBG_NDS_CATEGORY_CPU 1
#define DBG_NDS_CATEGORY_GENERAL 2

#define DBG_NDS_EVENT_WRITE_AFFINE_REGS 0
#define DBG_NDS_EVENT_SET_HBLANK_IRQ 1
#define DBG_NDS_EVENT_SET_VBLANK_IRQ 2
#define DBG_NDS_EVENT_SET_LINECOUNT_IRQ 3
#define DBG_NDS_EVENT_WRITE_VRAM 4

#define DBG_NDS_EVENT_MAX 5

enum NDS_DBLOG_CATEGORIES {
    NDS_CAT_UNKNOWN = 0,
    NDS_CAT_ARM7_INSTRUCTION = 1,
    NDS_CAT_ARM7_HALT,
    NDS_CAT_ARM9_INSTRUCTION,

    NDS_CAT_CART_READ_START,
    NDS_CAT_CART_READ_COMPLETE,

    NDS_CAT_DMA_START,

    NDS_CAT_PPU_REG_WRITE,
    NDS_CAT_PPU_MISC,
    NDS_CAT_PPU_BG_MODE,
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, clock.master_cycle_count9+waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);
#define dbgloglog_bus(r_cat, r_severity, r_format, ...) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->clock.master_cycle_count9+bus->waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

void NDSJ_setup_debugger_interface(jsm_system *, debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_NDS_DEBUGGER_H
