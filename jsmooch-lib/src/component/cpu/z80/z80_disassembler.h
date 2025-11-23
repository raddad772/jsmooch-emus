//
// Created by RadDad772 on 2/25/24.
//

#ifndef JSMOOCH_EMUS_Z80_DISASSEMBLER_H
#define JSMOOCH_EMUS_Z80_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

namespace Z80 {
struct core;
u32 disassemble(u32 *PC, u32 IR, jsm_debug_read_trace *rt, char *w, size_t sz);
void disassemble_entry(core*, disassembly_entry* entry);
}

#endif //JSMOOCH_EMUS_Z80_DISASSEMBLER_H
