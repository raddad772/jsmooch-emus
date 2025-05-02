//
// Created by . on 5/1/25.
//

#ifndef JSMOOCH_EMUS_GBA_TIMERS_H
#define JSMOOCH_EMUS_GBA_TIMERS_H

#include "helpers/int.h"
struct GBA;
u32 GBA_timer_enabled(struct GBA *, u32 tn);
u32 GBA_read_timer(struct GBA *, u32 tn);
void GBA_timer_write_cnt(void *ptr, u64 tn_and_val, u64 clock, u32 jitter);

#endif //JSMOOCH_EMUS_GBA_TIMERS_H
