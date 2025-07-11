//
// Created by RadDad772 on 3/20/24.
//

#include <string.h>
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"

#include "cvec.h"

void cvec_lock_reallocs(struct cvec* this)
{
    this->realloc_locked = 1;
}

void cvec_init(struct cvec* this, u32 data_size, u32 prealloc)
{
    this->data = NULL;
    this->data_sz = data_size;
    this->len_allocated = 0;
    this->realloc_locked = 0;
    this->len = 0;

    if (prealloc != 0) {
        this->len_allocated = prealloc * 2;
        this->data = malloc(data_size * this->len_allocated);
    }

}

void cvec_delete(struct cvec* this)
{
    if (this->data != NULL) {
        free(this->data);
    }
    this->realloc_locked = 0;
    this->data = NULL;
    this->len_allocated = 0;
    this->len = 0;
}

/*u32 cvec_len(struct cvec* this) {
    return this->len;
}*/

u32 cvec_index_of(struct cvec* this, void* ptr)
{
    u64 index = (u64)((u8 *)ptr - (u8 *)this->data) / this->data_sz;
    assert(index <= 0xFFFFFFFF);
    return (u32)index;
}

static void cvec_int_grow(struct cvec* this)
{
    if (this->len_allocated == 0) {
        assert(!this->realloc_locked);
        this->len_allocated = 20;
        this->data = malloc(this->data_sz * this->len_allocated);
    }
    else {
        assert(!this->realloc_locked);
        this->len_allocated = this->len_allocated * 2;
        u32 sz = this->data_sz * this->len_allocated;
        if (sz > (16*1024*1024)) {
            assert(1==2);
            printf("\nWARNING REALLOC ARRAY TO SIZE %d", sz);
        }
        void *new = realloc(this->data, sz);
        assert(new!=NULL);
        this->data = new;
    }
}

void *cvec_push_back(struct cvec* this)
{
    //printf("\nPUSH BACK! %d", this->allocated_len);
    if (this->len >= this->len_allocated) {
        cvec_int_grow(this);
    }
    void *ret = (char *)this->data + (this->len * this->data_sz);
    this->len++;
    return ret;
}

void cvec_grow_by(struct cvec *this, u32 num)
{
    // TODO: make this one op
    while (this->len < num) {
        cvec_push_back(this);
    }
}

void cvec_alloc_atleast(struct cvec* this, u64 howmuch)
{
    while(this->len_allocated < howmuch) {
        cvec_int_grow(this);
    }
}

void *cvec_pop_back(struct cvec* this)
{
    if (this->len == 0) return NULL;
    return (char *)this->data + (this->len-- * this->data_sz);
}

void cvec_clear(struct cvec* this)
{
    this->len = 0;
}

void *cvec_get(struct cvec* this, u32 index)
{
    assert(index < this->len);
    return (char *)this->data + (this->data_sz * index);
}

void *cvec_get_unsafe(struct cvec* this, u32 index)
{
    assert(index < this->len_allocated);
    return (char *)this->data + (this->data_sz * index);
}

void cvec_push_back_copy(struct cvec* this, void *src)
{
    void *dst = cvec_push_back(this);
    memcpy(dst, src, this->data_sz);
}

void cvec_ptr_init(struct cvec_ptr *vec)
{
    vec->vec = NULL;
    vec->index = 0;
}

void cvec_ptr_delete(struct cvec_ptr *vec)
{
    vec->vec = NULL;
    vec->index = 0;
}

struct cvec_ptr make_cvec_ptr(struct cvec *vec, u32 idx)
{
    return (struct cvec_ptr) { .vec=vec, .index=idx};
}

void *cpg(struct cvec_ptr p)
{
    assert(p.vec != NULL);
    return cvec_get(p.vec, p.index);
}
