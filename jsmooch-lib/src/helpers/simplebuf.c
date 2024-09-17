//
// Created by . on 8/29/24.
//
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "simplebuf.h"


void simplebuf8_clear(struct simplebuf8 *this)
{
    if (!this->ptr) return;
    memset(this, 0, this->sz);
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
    if (this->ptr != NULL) free(this->ptr);
    printf("\nALLOCATING SIZE %lld  %llx", sz, (u64)this->ptr);
    this->ptr = malloc(sz);
    printf("\nNEW PTR %llx", (u64)this->ptr);
    this->sz = sz;
    this->mask = sz - 1; // only use if we're a power of 2
}

