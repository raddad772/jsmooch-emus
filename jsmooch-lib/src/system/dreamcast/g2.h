//
// Created by RadDad772 on 3/18/24.
//

#ifndef JSMOOCH_EMUS_G2_H
#define JSMOOCH_EMUS_G2_H

#include "dreamcast.h"

void G2_write(struct DC* this, u32 addr, u64 val, u32 bits, u32* success);
u64 G2_read(struct DC* this, u32 addr, u32 sz, u32* success);

void G2_write_ADST(struct DC* this, u64 val);
void G2_write_E1ST(struct DC* this, u64 val);
void G2_write_E2ST(struct DC* this, u64 val);
void G2_write_DDST(struct DC* this, u64 val);


#endif //JSMOOCH_EMUS_G2_H
