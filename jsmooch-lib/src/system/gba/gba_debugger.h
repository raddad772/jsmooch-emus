//
// Created by . on 12/4/24.
//

#ifndef JSMOOCH_EMUS_GBA_DEBUGGER_H
#define JSMOOCH_EMUS_GBA_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#define DBG_GBA_CATEGORY_PPU 0
#define DBG_GBA_CATEGORY_CPU 1
#define DBG_GBA_CATEGORY_GENERAL 2

#define DBG_GBA_EVENT_WRITE_AFFINE_REGS 0
#define DBG_GBA_EVENT_SET_HBLANK_IRQ 1
#define DBG_GBA_EVENT_SET_VBLANK_IRQ 2
#define DBG_GBA_EVENT_SET_LINECOUNT_IRQ 3
#define DBG_GBA_EVENT_WRITE_VRAM 4

#define DBG_GBA_EVENT_MAX 5


enum GBA_DBLOG_CATEGORIES {
    GBA_CAT_UNKNOWN = 0,
    GBA_CAT_ARM7_INSTRUCTION = 1,
    GBA_CAT_ARM7_HALT,

    GBA_CAT_CART_READ_START,
    GBA_CAT_CART_READ_COMPLETE,

    GBA_CAT_DMA_START,

    GBA_CAT_PPU_REG_WRITE,
    GBA_CAT_PPU_MISC,
    GBA_CAT_PPU_BG_MODE,
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (this->dbg.dvptr->ids_enabled[r_cat]) dbglog_view_add_printf(this->dbg.dvptr, r_cat, this->clock.master_cycle_count9+this->waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

void GBAJ_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_GBA_DEBUGGER_H
