//
// Created by Dave on 2/6/2024.
//

#ifndef JSMOOCH_EMUS_BUF_H
#define JSMOOCH_EMUS_BUF_H

#include "helpers/int.h"

struct buf {
    void *ptr;
    u64 size;
};

void buf_init(struct buf* this);
void buf_allocate(struct buf* this, u64 size);
void buf_delete(struct buf* this);
void buf_copy(struct buf* dst, struct buf* src);

#endif //JSMOOCH_EMUS_BUF_H
