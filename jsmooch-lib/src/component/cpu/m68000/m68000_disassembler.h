//
// Created by . on 6/5/24.
//

#ifndef JSMOOCH_EMUS_M68000_DISASSEMBLER_H
#define JSMOOCH_EMUS_M68000_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/jsm_string.h"
#include "helpers/debug.h"

void M68k_disassemble(u32 PC, u16 IR, jsm_debug_read_trace *rt, jsm_string *out);

#endif //JSMOOCH_EMUS_M68000_DISASSEMBLER_H
