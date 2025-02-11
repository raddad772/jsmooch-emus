//
// Created by . on 2/11/25.
//

#ifndef JSMOOCH_EMUS_GTE_H
#define JSMOOCH_EMUS_GTE_H

#include "helpers/int.h"

struct R3000;
void GTE_command(struct R3000 *, u32 opcode);
void GTE_write_reg(struct R3000 *, u32 num, u32 val);
u32 GTE_read_reg(struct R3000 *, u32 num);

#endif //JSMOOCH_EMUS_GTE_H
