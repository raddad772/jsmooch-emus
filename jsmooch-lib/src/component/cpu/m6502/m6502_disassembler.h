//
// Created by Dave on 2/4/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

namespace M6502 {
void disassemble(u32 *PC, jsm_debug_read_trace &trace, jsm_string &outstr);
void disassemble_entry(core*, disassembly_entry& entry);
}

