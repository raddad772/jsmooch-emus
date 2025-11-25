//
// Created by . on 8/12/24.
//

#pragma once

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

void SMSGGJ_setup_debugger_interface(jsm_system *, debugger_interface *dbgr);

#define DBG_SMSGG_CATEGORY_CPU 0
#define DBG_SMSGG_CATEGORY_VDP 1

#define DBG_SMSGG_EVENT_IRQ 0
#define DBG_SMSGG_EVENT_NMI 1
#define DBG_SMSGG_EVENT_WRITE_HSCROLL 2
#define DBG_SMSGG_EVENT_WRITE_VSCROLL 3
#define DBG_SMSGG_EVENT_WRITE_VRAM 4

#define DBG_SMSGG_EVENT_MAX 5

enum SMSGG_DBLOG_CATEGORIES {
    SMSGG_CAT_UNKNOWN = 0,
    SMSGG_CAT_CPU_INSTRUCTION,
    SMSGG_CAT_CPU_READ,
    SMSGG_CAT_CPU_WRITE,
    SMSGG_CAT_CPU_IN,
    SMSGG_CAT_CPU_OUT
    //SMSGG_CAT_VDP_VRAM_WRITE
};

#define dbgloglog(wth, r_cat, r_severity, r_format, ...) if (wth->dbg.dvptr->ids_enabled[r_cat]) { wth->dbg.dvptr->add_printf(r_cat, wth->clock.master_cycles, r_severity, r_format, __VA_ARGS__); wth->dbg.dvptr->extra_printf(""); }

