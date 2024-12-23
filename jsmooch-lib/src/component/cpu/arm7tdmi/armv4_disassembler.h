//
// Created by . on 12/21/24.
//

#ifndef JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H
#define JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

struct ARM7TDMI;
void ARMv4_disassemble(u32 opc, struct jsm_string *out, i64 ins_addr);


#endif //JSMOOCH_EMUS_ARMV4_DISASSEMBLER_H
