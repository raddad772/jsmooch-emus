//
// Created by . on 12/21/24.
//

#ifndef JSMOOCH_EMUS_THUMB_DISASSEMBLER_H
#define JSMOOCH_EMUS_THUMB_DISASSEMBLER_H

#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "helpers_c/debugger/debugger.h"

struct ARM7TDMI;
struct ARMctxt;
void ARM7TDMI_thumb_disassemble(u16 opc, struct jsm_string *out, i64 ins_addr, struct ARMctxt *ct);

#endif //JSMOOCH_EMUS_THUMB_DISASSEMBLER_H
