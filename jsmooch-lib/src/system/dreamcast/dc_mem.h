//
// Created by Dave on 2/13/2024.
//

#ifndef JSMOOCH_EMUS_DC_MEM_H
#define JSMOOCH_EMUS_DC_MEM_H

#include "dreamcast.h"


u32 DCread8(void *ptr, u32 addr);
u32 DCread16(void *ptr, u32 addr);
u32 DCread32(void *ptr, u32 addr);
void DCwrite8(void *ptr, u32 addr, u32 val);
void DCwrite16(void *ptr, u32 addr, u32 val);
void DCwrite32(void *ptr, u32 addr, u32 val);
u32 DCfetch_ins(void *ptr, u32 addr);

#endif //JSMOOCH_EMUS_DC_MEM_H
