//
// Created by . on 4/19/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/jsm_string.h"
#include "helpers/debug.h"

#include "wdc65816.h"

namespace WDC65816 {
u32 disassemble(u32 addr, regs &r, u32 e, u32 m, u32 x, jsm_debug_read_trace &rt, jsm_string &out, ctxt *ct);
}