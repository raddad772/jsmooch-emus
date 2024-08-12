//
// Created by RadDad772 on 3/20/24.
//
// This is just a C implementation of the common vector type

#ifndef JSMOOCH_EMUS_CVEC_H
#define JSMOOCH_EMUS_CVEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include "helpers/int.h"

struct cvec {
    void *data;
    u32 data_sz;
    u32 len;
    u32 len_allocated;
    u32 realloc_locked;

    // For use by outsiders
    i32 kind;
    void *app_ptr;
};

struct cvec_iterator {
    struct cvec *vec;
    u32 index;
};

struct cvec_ptr {
    struct cvec *vec;
    u32 index;
};

void cvec_init(struct cvec *, u32 data_size, u32 prealloc);
void cvec_delete(struct cvec *);
u32 cvec_len(struct cvec *);
void *cvec_push_back(struct cvec *);
void *cvec_pop_back(struct cvec *);
void cvec_clear(struct cvec *);
void *cvec_get(struct cvec *, u32 index);
void *cvec_get_unsafe(struct cvec *, u32 index);
void cvec_lock_reallocs(struct cvec *); // For locking reallocs to preserve pointers
void cvec_push_back_copy(struct cvec *, void *src);
void cvec_grow(struct cvec *, u32 num);

void cvec_iterator_init(struct cvec_iterator *);

struct cvec_ptr make_cvec_ptr(struct cvec *, u32 idx);
void cvec_ptr_init(struct cvec_ptr *vec);
void cvec_ptr_delete(struct cvec_ptr *vec);

void *cpg(struct cvec_ptr p);

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_CVEC_H
