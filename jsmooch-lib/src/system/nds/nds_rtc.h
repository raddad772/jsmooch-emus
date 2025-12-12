//
// Created by . on 1/21/25.
//

#ifndef JSMOOCH_EMUS_NDS_RTC_H
#define JSMOOCH_EMUS_NDS_RTC_H

#include "helpers/int.h"



void NDS_RTC_reset(NDS *);
void NDS_RTC_init(NDS *);
void NDS_RTC_tick(void *ptr, u64 key, u64 clock, u32 jitter);
void NDS_write_RTC(NDS *, u8 sz, u32 val);
#endif //JSMOOCH_EMUS_NDS_RTC_H
