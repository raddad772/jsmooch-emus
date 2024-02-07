//
// Created by Dave on 2/6/2024.
//

#include "string.h"
#include "malloc.h"

#include "buf.h"

void buf_init(struct buf* this)
{
    this->ptr = NULL;
    this->size = 0;
}

void buf_allocate(struct buf* this, size_t size)
{
    if (this->ptr != NULL) {
        free(this->ptr);
    }
    if (size == 0) {
        this->ptr = NULL;
        this->size = 0;
        return;
    }
    this->ptr = malloc(size);
    this->size = size;
}

void buf_delete(struct buf* this)
{
    if (this->ptr != NULL)
        free(this->ptr);
    this->ptr = NULL;
    this->size = 0;
}

void buf_copy(struct buf* dst, struct buf* src) {
    if (src->ptr == NULL) {
        buf_delete(dst);
        return;
    }
    buf_allocate(dst, src->size);
    if (src->size > 0)
        memcpy(dst->ptr, src->ptr, src->size);
}
