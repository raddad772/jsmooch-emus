//
// Created by . on 5/1/24.
//

#ifndef JSMOOCH_EMUS_SH4_INTERRUPTS_H
#define JSMOOCH_EMUS_SH4_INTERRUPTS_H

#include "helpers/int.h"
#include "sh4_interpreter.h"

void SH4_interrupt_IRL(SH4*, u32 level);
void SH4_exec_interrupt(SH4*);
void SH4_set_IRL_irq_level(SH4*, u32 level);
void SH4_init_interrupt_struct(SH4_interrupt_source interrupt_sources[], SH4_interrupt_source* interrupt_map[]);
void SH4_interrupt(SH4*);
void SH4_IPR_update(SH4*);
void SH4_interrupt_pend(SH4*, enum SH4_interrupt_sources source, u32 onoff);


#endif //JSMOOCH_EMUS_SH4_INTERRUPTS_H
