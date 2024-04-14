//
// Created by RadDad772 on 3/20/24.
//

#ifndef JSMOOCH_EMUS_CVEC_H
#define JSMOOCH_EMUS_CVEC_H

#include "helpers/int.h"

struct cvec {
    void *data;
    u32 data_sz;
    u32 len;
    u32 len_allocated;

    // For use by outsiders
    i32 kind;
    void *app_ptr;
};

struct cvec_iterator {
    struct cvec* vec;
    u32 index;
};

void cvec_init(struct cvec* this, u32 data_size, u32 prealloc);
void cvec_delete(struct cvec* this);
u32 cvec_len(struct cvec* this);
void *cvec_push_back(struct cvec* this);
void *cvec_pop_back(struct cvec* this);
void cvec_clear(struct cvec* this);
void *cvec_get(struct cvec* this, u32 index);

void cvec_push_back_copy(struct cvec* this, void *src);

void cvec_iterator_init(struct cvec_iterator* this);

#endif //JSMOOCH_EMUS_CVEC_H
