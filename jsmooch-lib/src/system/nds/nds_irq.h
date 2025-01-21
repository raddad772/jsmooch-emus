//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_IRQ_H
#define JSMOOCH_EMUS_NDS_IRQ_H

#include "helpers/int.h"
struct NDS;
void NDS_update_IFs(struct NDS*, u32 bitnum);
void NDS_update_IF9(struct NDS*, u32 bitnum);
void NDS_update_IF7(struct NDS*, u32 bitnum);
void NDS_eval_irqs(struct NDS*);
void NDS_eval_irqs_9(struct NDS*);
void NDS_eval_irqs_7(struct NDS*);


#endif //JSMOOCH_EMUS_NDS_IRQ_H
