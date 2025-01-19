//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H
#define JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H

#include "helpers/int.h"

struct ARMctxt {
    u32 regs; // bits 0-15: regs 0-15. bit 16: CPSR, etc...
};

#endif //JSMOOCH_EMUS_ARMV5_DISASSEMBLER_H
