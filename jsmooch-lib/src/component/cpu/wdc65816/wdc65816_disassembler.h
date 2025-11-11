//
// Created by . on 4/19/25.
//

#ifndef JSMOOCH_EMUS_WDC65816_DISASSEMBLER_H
#define JSMOOCH_EMUS_WDC65816_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/jsm_string.h"
#include "helpers/debug.h"

#include "wdc65816.h"

u32 WDC65816_disassemble(u32 addr, WDC65816_regs *r, u32 e, u32 m, u32 x, jsm_debug_read_trace *rt, jsm_string *out, WDC65816_ctxt *ct);

#endif //JSMOOCH_EMUS_WDC65816_DISASSEMBLER_H
