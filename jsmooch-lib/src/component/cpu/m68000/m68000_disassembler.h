//
// Created by . on 6/5/24.
//

#ifndef JSMOOCH_EMUS_M68000_DISASSEMBLER_H
#define JSMOOCH_EMUS_M68000_DISASSEMBLER_H

#include "helpers_c/int.h"
#include "helpers_c/jsm_string.h"
#include "helpers_c/debug.h"

void M68k_disassemble(u32 PC, u16 IR, struct jsm_debug_read_trace *rt, struct jsm_string *out);

#endif //JSMOOCH_EMUS_M68000_DISASSEMBLER_H
