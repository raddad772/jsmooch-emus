//
// Created by . on 8/12/24.
//

#ifndef JSMOOCH_EMUS_SMSGG_DEBUGGER_H
#define JSMOOCH_EMUS_SMSGG_DEBUGGER_H

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
#endif //JSMOOCH_EMUS_SMSGG_DEBUGGER_H
