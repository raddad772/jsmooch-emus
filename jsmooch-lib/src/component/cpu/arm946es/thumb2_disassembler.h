//
// Created by . on 1/22/25.
//

#ifndef JSMOOCH_EMUS_THUMB2_DISASSEMBLER_H
#define JSMOOCH_EMUS_THUMB2_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debug.h"
#include "helpers/debugger/debugger.h"

struct ARMctxt;
void ARM946ES_thumb_disassemble(u16 opc, struct jsm_string *out, i64 ins_addr, struct ARMctxt *ct);


#endif //JSMOOCH_EMUS_THUMB2_DISASSEMBLER_H
