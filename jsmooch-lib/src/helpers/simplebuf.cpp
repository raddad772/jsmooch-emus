//
// Created by . on 8/29/24.
//
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "simplebuf.h"
#include "buf.h"

simplebuf8::~simplebuf8()
{
    if (ptr) free(ptr);
    ptr = nullptr;
    sz = 0;
    mask = 0;
}

void simplebuf8::clear()
{
    if (this->ptr == NULL) return;
    memset(this->ptr, 0, this->sz);
}

int popcount(u64 n)
{
    int c = 0;
    for (; n; ++c)
        n &= n - 1;
    return c;
}

void simplebuf8::allocate(u64 sz)
{
    if (ptr != NULL) {
        free(ptr);
    }
    this->ptr = malloc(sz);
    this->sz = sz;
    this->mask = sz - 1; // only use if we're a power of 2
}

void simplebuf8::copy_from_buf(struct buf *src)
{
    if (src->ptr == NULL) {
        if (ptr) free(ptr);
        ptr = nullptr;
        sz = 0;
        mask = 0;
        return;
    }

    allocate(src->size);
    if (src->size > 0)
        memcpy(ptr, src->ptr, src->size);
    dest->sz = src->size;
    dest->mask = dest->sz - 1; // only use if we're definitely a power of 2
}