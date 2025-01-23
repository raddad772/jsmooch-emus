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


void NDSJ_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_NDS_DEBUGGER_H
