//
// Created by . on 6/18/25.
//

#ifndef JSMOOCH_EMUS_TG16_DEBUGGER_H
#define JSMOOCH_EMUS_TG16_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

enum TG16_DBLOG_CATEGORIES {
    TG16_CAT_UNKNOWN = 0,
    TG16_CAT_CPU_INSTRUCTION = 1,
    TG16_CAT_CPU_READ,
    TG16_CAT_CPU_WRITE,
    TG16_CAT_CPU_IRQS
};


#define dbgloglog(r_cat, r_severity, r_format, ...) if (this->dbg.dvptr->ids_enabled[r_cat]) dbglog_view_add_printf(this->dbg.dvptr, r_cat, this->clock.master_cycle_count9+this->waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

void TG16J_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_TG16_DEBUGGER_H
