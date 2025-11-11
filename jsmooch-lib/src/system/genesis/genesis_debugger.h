//
// Created by . on 10/20/24.
//

#ifndef JSMOOCH_EMUS_GENESIS_DEBUGGER_H
#define JSMOOCH_EMUS_GENESIS_DEBUGGER_H

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#define DBG_GEN_CATEGORY_VDP 0
#define DBG_GEN_CATEGORY_CPU 1

#define DBG_GEN_EVENT_WRITE_VRAM   0
#define DBG_GEN_EVENT_WRITE_VSRAM  1
#define DBG_GEN_EVENT_WRITE_CRAM   2
#define DBG_GEN_EVENT_HBLANK_IRQ   3
#define DBG_GEN_EVENT_VBLANK_IRQ   4
#define DBG_GEN_EVENT_DMA_FILL 5
#define DBG_GEN_EVENT_DMA_COPY 6
#define DBG_GEN_EVENT_DMA_LOAD 7

#define DBG_GEN_EVENT_MAX 8


void genesisJ_setup_debugger_interface(jsm_system *, debugger_interface *dbgr);

#endif //JSMOOCH_EMUS_GENESIS_DEBUGGER_H
