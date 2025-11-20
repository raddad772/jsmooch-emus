
#pragma once

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

namespace RCA1802 {
    struct core;
void disassemble_entry(core*, disassembly_entry& entry);
}
