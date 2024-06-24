//
// Created by Dave on 1/30/2024.
//

#ifndef JSMOOCH_EMUS_SM83_DISASSEMBLER_H
#define JSMOOCH_EMUS_SM83_DISASSEMBLER_H

#include "helpers/int.h"

u32 SM83_disassemble(u32 PC, u32 (*peek)(u32), char *w);

#endif //JSMOOCH_EMUS_SM83_DISASSEMBLER_H
