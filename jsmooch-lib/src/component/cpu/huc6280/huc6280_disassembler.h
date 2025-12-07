//
// Created by . on 7/11/25.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"
namespace HUC6280 {
struct core;
void disassemble_entry(core&, disassembly_entry &entry);
void disassemble(core &cpu, u32 &PC, jsm_debug_read_trace &trace, jsm_string &outstr);

}
