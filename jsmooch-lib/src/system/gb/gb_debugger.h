//
// Created by . on 8/12/24.
//

#ifndef JSMOOCH_EMUS_GB_DEBUGGER_H
#define JSMOOCH_EMUS_GB_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#define DBG_GB_CATEGORY_TIMER 0
#define DBG_GB_CATEGORY_CPU 1
#define DBG_GB_CATEGORY_PPU 2

#define DBG_GB_EVENT_IRQ_JOYPAD 0
#define DBG_GB_EVENT_IRQ_VBLANK 1
#define DBG_GB_EVENT_IRQ_STAT 2
#define DBG_GB_EVENT_IRQ_TIMER 3
#define DBG_GB_EVENT_IRQ_SERIAL 4
#define DBG_GB_EVENT_HALT_END 5
#define DBG_GB_EVENT_OAM_DMA_START 6

#define DBG_GB_EVENT_TIMER_TICK 7
#define DBG_GB_EVENT_TIMER_IRQ 8

#define DBG_GB_EVENT_VRAM_WRITE 9
#define DBG_GB_EVENT_SCX_WRITE 10
#define DBG_GB_EVENT_SCY_WRITE 11
#define DBG_GB_EVENT_LCDC_WRITE 12

#define DBG_GB_EVENT_MAX 13

void GBJ_setup_debugger_interface(struct jsm_system *, struct debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_GB_DEBUGGER_H
