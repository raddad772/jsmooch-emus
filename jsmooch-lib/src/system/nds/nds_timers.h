//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_TIMERS_H
#define JSMOOCH_EMUS_NDS_TIMERS_H

#include "helpers/int.h"

struct NDS;
void NDS_tick_timers7(struct NDS*, u32 num_ticks);
void NDS_tick_timers9(struct NDS*, u32 num_ticks);


#endif //JSMOOCH_EMUS_NDS_TIMERS_H
