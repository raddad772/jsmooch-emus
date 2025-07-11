//
// Created by . on 7/11/25.
//

#ifndef JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
#define JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H

#include "helpers/int.h"
#include "helpers/debugger/debugger.h"

struct HUC6280;

void HUC6280_disassemble_entry(struct HUC6280*, struct disassembly_entry* entry);

#endif //JSMOOCH_EMUS_HUC6280_DISASSEMBLER_H
