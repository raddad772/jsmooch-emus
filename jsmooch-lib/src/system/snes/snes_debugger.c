//
// Created by . on 4/23/25.
//

#include "snes_debugger.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "snes_bus.h"
#include "component/cpu/wdc65816/wdc65816_disassembler.h"
#include "component/cpu/spc700/spc700_disassembler.h"

#define JTHIS struct SNES* this = (struct SNES*)jsm->ptr
#define JSM struct jsm_system* jsm

#define THIS struct genesis* this

void SNESJ_setup_debugger_interface(JSM, struct debugger_interface *dbgr) {
    JTHIS;
    this->dbg.interface = dbgr;

    dbgr->supported_by_core = 1;
    dbgr->smallest_step = 4;
}