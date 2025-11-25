//
// Created by RadDad772 on 2/25/24.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

namespace Z80 {
struct core;
u32 disassemble(u32 &PC, u32 IR, jsm_debug_read_trace &rt, jsm_string &out);
void disassemble_entry(core*, disassembly_entry& entry);
}
