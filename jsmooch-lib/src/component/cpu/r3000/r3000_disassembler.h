//
// Created by . on 2/12/25.
//

#ifndef JSMOOCH_EMUS_R3000_DISASSEMBLER_H
#define JSMOOCH_EMUS_R3000_DISASSEMBLER_H
#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "helpers_c/debugger/debugger.h"

struct R3000ctxt {
    u64 regs; // bits 0-31: regs 0-31. bit 32: PC
    u64 gte; // bits 0-63: GTE regs
    u64 cop; // bits 0-63: COP regs
};

void R3000_disassemble(u32 opcode, struct jsm_string *out, i64 ins_addr, struct R3000ctxt *ct);

#endif //JSMOOCH_EMUS_R3000_DISASSEMBLER_H
