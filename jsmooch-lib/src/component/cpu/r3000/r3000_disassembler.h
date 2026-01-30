//
// Created by . on 2/12/25.
//

#pragma once
#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"
namespace R3000 {
struct ctxt {
    u64 regs; // bits 0-31: regs 0-31. bit 32: PC
    u64 gte; // bits 0-63: GTE regs
    u64 cop; // bits 0-63: COP regs
};
}

void R3000_disassemble(u32 opcode, jsm_string &out, i64 ins_addr, R3000::ctxt *ct);
