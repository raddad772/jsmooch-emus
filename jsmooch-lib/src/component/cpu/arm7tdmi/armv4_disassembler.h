//
// Created by . on 12/21/24.
//

#ifndef JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H
#define JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"


struct ARM7TDMI;
struct ARMctxt {
    u32 regs; // bits 0-15: regs 0-15. bit 16: CPSR, etc...
};


//void ARMv4_disassemble(u32 opc, struct jsm_string *out, i64 ins_addr);
void ARMv4_disassemble(u32 opcode, struct jsm_string *out, i64 ins_addr, struct ARMctxt *ct);


#endif //JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H
