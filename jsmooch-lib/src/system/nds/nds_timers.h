//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_TIMERS_H
#define JSMOOCH_EMUS_NDS_TIMERS_H

#include "helpers/int.h"

struct NDS;
void NDS_tick_timers7(struct NDS*, u32 num_ticks);
void NDS_tick_timers9(struct NDS*, u32 num_ticks);
u32 NDS_timer7_enabled(struct NDS *, u32 tn);
u32 NDS_timer9_enabled(struct NDS *, u32 tn);
u32 NDS_read_timer7(struct NDS *this, u32 tn);
u32 NDS_read_timer9(struct NDS *this, u32 tn);


#endif //JSMOOCH_EMUS_NDS_TIMERS_H
