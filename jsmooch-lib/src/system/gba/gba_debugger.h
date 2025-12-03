//
// Created by . on 12/4/24.
//

#pragma once

#include "helpers/debugger/debugger.h"
#include "helpers/sys_interface.h"

#define DBG_GBA_CATEGORY_PPU 0
#define DBG_GBA_CATEGORY_CPU 1
#define DBG_GBA_CATEGORY_GENERAL 2

#define DBG_GBA_EVENT_WRITE_AFFINE_REGS 0
#define DBG_GBA_EVENT_SET_HBLANK_IRQ 1
#define DBG_GBA_EVENT_SET_VBLANK_IRQ 2
#define DBG_GBA_EVENT_SET_LINECOUNT_IRQ 3
#define DBG_GBA_EVENT_WRITE_VRAM 4

#define DBG_GBA_EVENT_MAX 5

namespace GBA {
enum DBLOG_CATEGORIES {
    CAT_UNKNOWN = 0,
    CAT_ARM7_INSTRUCTION = 1,
    CAT_ARM7_HALT,

    CAT_CART_READ_START,
    CAT_CART_READ_COMPLETE,

    CAT_DMA_START,

    CAT_PPU_REG_WRITE,
    CAT_PPU_MISC,
    CAT_PPU_BG_MODE,
};

#define dbgloglog(r_cat, r_severity, r_format, ...) if (dbg.dvptr->ids_enabled[r_cat]) dbg.dvptr->add_printf(r_cat, clock.master_cycle_count9+waitstates.current_transaction, r_severity, r_format, __VA_ARGS__);

}