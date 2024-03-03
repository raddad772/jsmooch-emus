//
// Created by Dave on 2/13/2024.
//

#ifndef JSMOOCH_EMUS_DC_MEM_H
#define JSMOOCH_EMUS_DC_MEM_H

#include "dreamcast.h"


void DCwrite(void *ptr, u32 addr, u32 val, u32 sz);
u32 DCread(void *ptr, u32 addr, u32 sz);
u32 DCfetch_ins(void *ptr, u32 addr);

#endif //JSMOOCH_EMUS_DC_MEM_H
