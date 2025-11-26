#pragma once

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#undef DBG_EVENT
#define DBG_EVENT(x) debugger_report_event(snes->dbg.events.view, x)

enum C64_DBLOG_CATEGORIES {
    C64_CAT_UNKNOWN = 0,
    C64_CAT_CPU_INSTRUCTION,
    C64_CAT_CPU_READ,
    C64_CAT_CPU_WRITE,
    C64_CAT_CPU_IRQ,
    C64_CAT_CPU_NMI
};

#define DBG_C64_CATEGORY_CPU 0
#define DBG_C64_CATEGORY_VIC2 1

#define DBG_C64_EVENT_IRQ 0

#define DBG_C64_EVENT_MAX 1

#define dbgloglog(wth, r_cat, r_severity, r_format, ...) if (wth->dbg.dvptr->ids_enabled[r_cat]) { wth->dbg.dvptr->add_printf(r_cat, wth->master_clock, r_severity, r_format, __VA_ARGS__); wth->dbg.dvptr->extra_printf(""); }
