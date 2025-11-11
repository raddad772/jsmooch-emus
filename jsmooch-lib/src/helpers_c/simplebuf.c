//
// Created by . on 8/29/24.
//
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "simplebuf.h"
#include "buf.h"


void simplebuf8_clear(struct simplebuf8 *this)
{
    if (this->ptr == NULL) return;
    memset(this->ptr, 0, this->sz);
}

void simplebuf8_init(struct simplebuf8 *this)
{
    this->ptr = NULL;
    this->sz = this->mask = 0;
}

void simplebuf8_delete(struct simplebuf8 *this)
{
    if (this->ptr) free(this->ptr);
    this->ptr = NULL;
    this->sz = 0;
    this->mask = 0;
}

int popcount(u64 n)
{
    int c = 0;
    for (; n; ++c)
        n &= n - 1;
    return c;
}

void simplebuf8_allocate(struct simplebuf8 *this, u64 sz)
{
    //assert(popcount(sz) == 1); // Assert size is a power of 2
    if (this->ptr != NULL) {
        free(this->ptr);
    }
    this->ptr = malloc(sz);
    this->sz = sz;
    this->mask = sz - 1; // only use if we're a power of 2
}

void simplebuf8_copy_from_buf(struct simplebuf8 *dest, buf *src)
{
    if (src->ptr == NULL) {
        simplebuf8_delete(dest);
        return;
    }

    simplebuf8_allocate(dest, src->size);
    if (src->size > 0)
        memcpy(dest->ptr, src->ptr, src->size);
    dest->sz = src->size;
    dest->mask = dest->sz - 1; // only use if we're definitely a power of 2
}