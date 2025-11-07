//
// Created by . on 1/20/25.
//

#ifndef JSMOOCH_EMUS_NDS_TIMERS_H
#define JSMOOCH_EMUS_NDS_TIMERS_H

#include "helpers_c/int.h"

struct NDS;
u32 NDS_timer7_enabled(struct NDS *, u32 tn);
u32 NDS_timer9_enabled(struct NDS *, u32 tn);
u32 NDS_read_timer7(struct NDS *, u32 tn);
u32 NDS_read_timer9(struct NDS *, u32 tn);
void NDS_timer7_write_cnt(struct NDS *, u32 tn, u32 val);
void NDS_timer9_write_cnt(struct NDS *, u32 tn, u32 val);
#endif //JSMOOCH_EMUS_NDS_TIMERS_H
