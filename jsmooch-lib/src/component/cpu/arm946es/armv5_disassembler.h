//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H
#define JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/jsm_string.h"

//struct ARM946ES;
struct ARMctxt {
    u32 regs; // bits 0-15: regs 0-15. bit 16: CPSR, etc...
};


//void ARMv4_disassemble(u32 opc, jsm_string *out, i64 ins_addr);
void ARMv5_disassemble(u32 opcode, jsm_string *out, i64 ins_addr, ARMctxt *ct);

#endif //JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H
