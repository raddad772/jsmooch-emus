//
// Created by . on 1/18/25.
//

#ifndef JSMOOCH_EMUS_ARM946ES_DECODE_H
#define JSMOOCH_EMUS_ARM946ES_DECODE_H

#include "helpers/int.h"

struct ARM946ES;
struct thumb2_instruction;
void ARM946ES_fill_arm_table(struct ARM946ES *this);
void decode_thumb2(u16 opc, thumb2_instruction *ins);
#endif //JSMOOCH_EMUS_ARM946ES_DECODE_H
