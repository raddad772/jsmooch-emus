//
// Created by . on 7/11/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
#define JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H

#include "helpers_c/int.h"
#include "helpers_c/debugger/debugger.h"

struct HUC6280;

void HUC6280_disassemble_entry(struct HUC6280*, struct disassembly_entry* entry);
void HUC6280_disassemble(struct HUC6280 *cpu, u32 *PC, struct jsm_debug_read_trace *trace, struct jsm_string *outstr);

#endif //JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
