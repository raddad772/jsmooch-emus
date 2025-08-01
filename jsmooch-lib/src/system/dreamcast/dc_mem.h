//
// Created by Dave on 2/13/2024.
//

#ifndef JSMOOCH_EMUS_DC_MEM_H
#define JSMOOCH_EMUS_DC_MEM_H

#include "dreamcast.h"


void DCwrite(void *ptr, u32 addr, u64 val, u32 sz);
u64 DCread(void *ptr, u32 addr, u32 sz, bool is_ins_fetch);
u32 DCfetch_ins(void *ptr, u32 addr);
u64 DCread_noins(void *ptr, u32 addr, u32 sz);

#endif //JSMOOCH_EMUS_DC_MEM_H
