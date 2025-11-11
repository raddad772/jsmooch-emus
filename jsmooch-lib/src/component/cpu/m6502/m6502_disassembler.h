//
// Created by Dave on 2/4/2024.
//

#ifndef JSMOOCH_EMUS_M6502_DISASSEMBLER_H
#define JSMOOCH_EMUS_M6502_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

struct M6502;
void M6502_disassemble_entry(M6502*, disassembly_entry* entry);

#endif //JSMOOCH_EMUS_M6502_DISASSEMBLER_H
