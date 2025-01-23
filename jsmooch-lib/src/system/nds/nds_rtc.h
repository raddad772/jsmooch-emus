//
// Created by . on 1/21/25.
//

#ifndef JSMOOCH_EMUS_NDS_RTC_H
#define JSMOOCH_EMUS_NDS_RTC_H

#include "helpers/int.h"

struct NDS;

void NDS_RTC_reset(struct NDS *);
void NDS_RTC_init(struct NDS *);
void NDS_RTC_tick(struct NDS *);
void NDS_write_RTC(struct NDS *, u32 sz, u32 val);
#endif //JSMOOCH_EMUS_NDS_RTC_H
