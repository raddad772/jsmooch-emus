//
// Created by Dave on 2/4/2024.
//

#pragma once

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

struct M6502;
void M6502_disassemble_entry(M6502*, disassembly_entry& entry);

