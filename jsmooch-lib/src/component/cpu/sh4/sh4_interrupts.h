//
// Created by . on 5/1/24.
//

#ifndef JSMOOCH_EMUS_SH4_INTERRUPTS_H
#define JSMOOCH_EMUS_SH4_INTERRUPTS_H

#include "helpers/int.h"
#include "sh4_interpreter.h"

void SH4_interrupt_IRL(struct SH4*, u32 level);
void SH4_exec_interrupt(struct SH4*);
void SH4_set_IRL_irq_level(struct SH4*, u32 level);
void SH4_init_interrupt_struct(struct SH4_interrupt_source interrupt_sources[], struct SH4_interrupt_source* interrupt_map[]);
void SH4_interrupt(struct SH4*);
void SH4_IPR_update(struct SH4*);
void SH4_interrupt_pend(struct SH4*, enum SH4_interrupt_sources source, u32 onoff);


#endif //JSMOOCH_EMUS_SH4_INTERRUPTS_H
