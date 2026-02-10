//
// Created by . on 2/16/25.
//

#pragma once

enum PS1_DBLOG_CATEGORIES {
    PS1_CAT_UNKNOWN = 0,
    PS1_CAT_R3000_INSTRUCTION = 1,
    PS1_CAT_R3000_RFE = 2,
    PS1_CAT_R3000_EXCEPTION = 3
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, clock.master_cycle_count9+waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);
#define dbgloglog_bus(r_cat, r_severity, r_format, ...) if (bus->dbg.dvptr->ids_enabled[r_cat]) bus->dbg.dvptr->add_printf(r_cat, bus->clock.master_cycle_count9+bus->waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

