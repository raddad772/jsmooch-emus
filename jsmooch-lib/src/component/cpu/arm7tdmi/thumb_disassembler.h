//
// Created by . on 12/21/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

namespace ARM7TDMI {
void thumb_disassemble(u16 opc, jsm_string *out, i64 ins_addr, struct ARMctxt *ct);
}

