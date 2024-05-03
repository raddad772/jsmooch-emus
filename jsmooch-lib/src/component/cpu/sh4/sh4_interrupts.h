//
// Created by . on 5/1/24.
//

#ifndef JSMOOCH_EMUS_SH4_INTERRUPTS_H
#define JSMOOCH_EMUS_SH4_INTERRUPTS_H

#include "helpers/int.h"
#include "sh4_interpreter.h"

void SH4_interrupt_IRL(struct SH4* this, u32 level);
void SH4_exec_interrupt(struct SH4* this);
void SH4_IPR_update(struct SH4* this);



#endif //JSMOOCH_EMUS_SH4_INTERRUPTS_H
