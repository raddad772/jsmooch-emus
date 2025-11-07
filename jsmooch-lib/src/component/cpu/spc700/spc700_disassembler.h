//
// Created by . on 4/19/25.
//

#ifndef JSMOOCH_EMUS_SPC700_DISASSEMBLER_H
#define JSMOOCH_EMUS_SPC700_DISASSEMBLER_H

#include "helpers_c/int.h"
#include "helpers_c/debug.h"
#include "helpers_c/jsm_string.h"

u32 SPC700_disassemble(u32 PC, struct jsm_debug_read_trace *rt, struct jsm_string *out, u32 p_p);

#endif //JSMOOCH_EMUS_SPC700_DISASSEMBLER_H
