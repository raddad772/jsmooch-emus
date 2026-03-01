//
// Created by . on 5/1/24.
//

#pragma once
#include "helpers/int.h"
#include "sh4_interpreter.h"

namespace SH4 {
void init_interrupt_struct(IRQ_SOURCE interrupt_sources[], IRQ_SOURCE* interrupt_map[]);
}