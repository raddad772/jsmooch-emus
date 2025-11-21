#pragma once

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#undef DBG_EVENT
#define DBG_EVENT(x) debugger_report_event(snes->dbg.events.view, x)


#define BPP_2 0
#define BPP_4 1
#define BPP_8 2

enum VIP_DBLOG_CATEGORIES {
    VIP_CAT_UNKNOWN = 0,
    VIP_CAT_CPU_INSTRUCTION,
    VIP_CAT_CPU_READ,
    VIP_CAT_CPU_WRITE,
    VIP_CAT_CPU_DMA,
    VIP_CAT_CPU_IRQ,
};

#define DBG_VIP_CATEGORY_CPU 0
#define DBG_VIP_CATEGORY_PIXIE 1

#define DBG_VIP_EVENT_IRQ 0


#define DBG_VIP_EVENT_MAX 1

#define dbgloglog(wth, r_cat, r_severity, r_format, ...) if (wth->dbg.dvptr->ids_enabled[r_cat]) { wth->dbg.dvptr->add_printf(r_cat, wth->clock.master_cycle_count, r_severity, r_format, __VA_ARGS__); wth->dbg.dvptr->extra_printf(""); }
