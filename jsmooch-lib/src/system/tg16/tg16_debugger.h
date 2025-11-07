//
// Created by . on 6/18/25.
//

#ifndef JSMOOCH_EMUS_TG16_DEBUGGER_H
#define JSMOOCH_EMUS_TG16_DEBUGGER_H

#include "helpers_c/debugger/debugger.h"
#include "helpers_c/sys_interface.h"

enum TG16_DBLOG_CATEGORIES {
    TG16_CAT_UNKNOWN = 0,
    TG16_CAT_CPU_INSTRUCTION = 1,
    TG16_CAT_CPU_READ,
    TG16_CAT_CPU_WRITE,
    TG16_CAT_CPU_IRQS
};

#define DBG_TG16_CATEGORY_CPU 0
#define DBG_TG16_CATEGORY_VCE 1
#define DBG_TG16_CATEGORY_VDC0 2

enum TG16_EVENTS {
    DBG_TG16_EVENT_IRQ1 = 0,
    DBG_TG16_EVENT_IRQ2,
    DBG_TG16_EVENT_TIQ,
    DBG_TG16_EVENT_HSYNC_UP,
    DBG_TG16_EVENT_VSYNC_UP,
    DBG_TG16_EVENT_WRITE_CRAM,
    DBG_TG16_EVENT_WRITE_VRAM,
    DBG_TG16_EVENT_WRITE_RCR,
    DBG_TG16_EVENT_HIT_RCR,
    DBG_TG16_EVENT_WRITE_XSCROLL,
    DBG_TG16_EVENT_WRITE_YSCROLL,
    DBG_TG16_EVENT_MAX
};


#define dbgloglog(r_cat, r_severity, r_format, ...) if (this->dbg.dvptr->ids_enabled[r_cat]) dbglog_view_add_printf(this->dbg.dvptr, r_cat, this->clock.master_cycle_count9+this->waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

void TG16J_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_TG16_DEBUGGER_H
