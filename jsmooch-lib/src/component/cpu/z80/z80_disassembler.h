//
// Created by RadDad772 on 2/25/24.
//

#ifndef JSMOOCH_EMUS_Z80_DISASSEMBLER_H
#define JSMOOCH_EMUS_Z80_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

struct Z80;

u32 Z80_disassemble(u32 *PC, u32 IR, jsm_debug_read_trace *rt, char *w, size_t sz);
void Z80_disassemble_entry(struct Z80*, disassembly_entry* entry);

#endif //JSMOOCH_EMUS_Z80_DISASSEMBLER_H
