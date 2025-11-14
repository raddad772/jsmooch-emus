//
// Created by RadDad772 on 3/20/24.
//
// This is just a C implementation of the common vector type

#ifndef JSMOOCH_EMUS_CVEC_H
#define JSMOOCH_EMUS_CVEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cassert>
#include "helpers/int.h"

struct cvec {
    void *data;
    u64 data_sz;
    u64 len;
    u64 len_allocated;
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

void cvec_init(cvec *, u32 data_size, u32 prealloc);
void cvec_delete(cvec *);
//u32 cvec_len(cvec *);
#define cvec_len(a) ((a)->len)
void *cvec_push_back(cvec *);
u32 cvec_index_of(cvec*, void* ptr);
void *cvec_pop_back(cvec *);
void cvec_clear(cvec *);
void *cvec_get(cvec *, u32 index);
void *cvec_get_unsafe(cvec *, u32 index);
void cvec_lock_reallocs(cvec *); // For locking reallocs to preserve pointers
void cvec_push_back_copy(cvec *, void *src);
void cvec_grow_by(cvec *, u32 num); // Grow a specific # of elements
void cvec_alloc_atleast(cvec*, u64 howmuch); // Make sure at least # elements are allocated

void cvec_iterator_init(cvec_iterator *);

struct cvec_ptr make_cvec_ptr(cvec *, u32 idx);
void cvec_ptr_init(cvec_ptr *vec);
void cvec_ptr_delete(cvec_ptr *vec);

void *cpg(cvec_ptr p);

#ifdef __cplusplus
}
#endif


#endif //JSMOOCH_EMUS_CVEC_H
