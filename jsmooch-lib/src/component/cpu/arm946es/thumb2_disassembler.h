//
// Created by . on 1/22/25.
//

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

namespace ARM946ES {
struct ARMctxt;
void thumb_disassemble(u16 opc, jsm_string *out, i64 ins_addr, ARMctxt *ct);
}
