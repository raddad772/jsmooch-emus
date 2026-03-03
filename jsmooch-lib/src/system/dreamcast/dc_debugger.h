//
// Created by . on 3/2/26.
//

#pragma once

enum DC_DBGLOG_CATEGORIES {
    DCD_SH4_INSTRUCTION = 0,
    DCD_SH4_EXCEPTION = 1,

    DCD_MAINBUS_READ,
    DCD_MAINBUS_WRITE
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, trace_cycles, r_severity, r_format, __VA_ARGS__)
#define dbgloglog_bus(r_cat, r_severity, r_format, ...) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->trace_cycles, r_severity, r_format, __VA_ARGS__)
#define dbgloglog_thbus(r_cat, r_severity, r_format, ...) if (th->bus->dbg.dvptr->ids_enabled[r_cat]) th->bus->dbg.dvptr->add_printf(r_cat, th->bus->trace_cycles, r_severity, r_format, __VA_ARGS__)
#define dbgloglog_busn(r_cat, r_severity, r_format) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->trace_cycles, r_severity, r_format)


