//
// Created by Dave on 1/30/2024.
//

#ifndef JSMOOCH_EMUS_SM83_DISASSEMBLER_H
#define JSMOOCH_EMUS_SM83_DISASSEMBLER_H

#include "helpers/int.h"

void SM83_disassemble(char **out, u32 PC, u32 (*peek)(u32));

#endif //JSMOOCH_EMUS_SM83_DISASSEMBLER_H
