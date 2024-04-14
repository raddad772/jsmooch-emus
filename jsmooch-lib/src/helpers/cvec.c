//
// Created by RadDad772 on 3/20/24.
//

#include <string.h>
#include "stdlib.h"
#include "stdio.h"
#include "assert.h"

#include "cvec.h"

void cvec_init(struct cvec* this, u32 data_size, u32 prealloc)
{
    this->data = NULL;
    this->data_sz = data_size;
    this->len_allocated = 0;
    this->len = 0;

    if (prealloc != 0) {
        this->len_allocated = prealloc * 2;
        this->data = malloc(data_size * this->len_allocated);
    }
}

void cvec_delete(struct cvec* this)
{
    if (this->data != NULL)
        free(this->data);
    this->data = NULL;
    this->len_allocated = 0;
    this->len = 0;
}

u32 cvec_len(struct cvec* this) {
    return this->len;
}

void *cvec_push_back(struct cvec* this)
{
    printf("\nPUSH BACK! %d", this->len);
    if (this->len >= this->len_allocated) {
        if (this->len_allocated == 0) {
            this->len_allocated = 20;
            this->data = malloc(this->data_sz * this->len_allocated);
        }
        else {
            this->len_allocated = this->len_allocated * 2;
            u32 sz = this->data_sz * this->len_allocated;
            if (sz > (16*1024*1024)) {
                printf("\nWARNING REALLOC ARRAY TO SIZE %d", sz);
            }
            void *new = realloc(this->data, sz);
            assert(new!=NULL);
            this->data = new;
        }
    }
    void *ret = this->data + (this->len * this->data_sz);
    this->len++;
    return ret;
}

void *cvec_pop_back(struct cvec* this)
{
    if (this->len == 0) return NULL;
    return this->data + (this->len-- * this->data_sz);
}

void cvec_clear(struct cvec* this)
{
    this->len = 0;
}

void *cvec_get(struct cvec* this, u32 index)
{
#ifdef JSDEBUG
    assert(index < this->len);
#endif
    return this->data + (this->data_sz * index);
}

void cvec_push_back_copy(struct cvec* this, void *src)
{
    void *dst = cvec_push_back(this);
    memcpy(dst, src, this->data_sz);
}
