//
// Created by . on 7/11/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
#define JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

struct HUC6280;

void HUC6280_disassemble_entry(HUC6280*, disassembly_entry* entry);
void HUC6280_disassemble(HUC6280 *cpu, u32 *PC, jsm_debug_read_trace *trace, jsm_string *outstr);

#endif //JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
