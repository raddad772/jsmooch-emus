//
// Created by RadDad772 on 2/25/24.
//

#ifndef JSMOOCH_EMUS_Z80_DISASSEMBLER_H
#define JSMOOCH_EMUS_Z80_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"

u32 Z80_disassemble(u32 PC, u32 IR, struct jsm_debug_read_trace *rt, char *w);

#endif //JSMOOCH_EMUS_Z80_DISASSEMBLER_H
