//
// Created by . on 8/8/24.
//

#ifndef JSMOOCH_EMUS_NES_DEBUGGER_H
#define JSMOOCH_EMUS_NES_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

void NESJ_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#define DBG_NES_CATEGORY_CPU 0
#define DBG_NES_CATEGORY_PPU 1

#define DBG_NES_EVENT_IRQ 0
#define DBG_NES_EVENT_NMI 1
#define DBG_NES_EVENT_W2006 2
#define DBG_NES_EVENT_W2007 3
#define DBG_NES_EVENT_OAM_DMA 4

#define DBG_NES_EVENT_MAX 5

#endif //JSMOOCH_EMUS_NES_DEBUGGER_H
