//
// Created by Dave on 2/14/2024.
//

#include "assert.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "stdarg.h"

#include "jsm_string.h"
#include "debug.h"

void jsm_string_init(struct jsm_string *this, u32 sz)
{
    this->ptr = this->cur = NULL;
    this->len = sz;
    this->ptr = this->cur = malloc(sz);
    memset(this->ptr, 0, sz);
}

void jsm_string_delete(struct jsm_string *this)
{
    if (this->ptr != NULL) {
        free(this->ptr);
        this->ptr = NULL;
    }
    this->cur = NULL;
    this->len = 0;
}

void jsm_string_seek(struct jsm_string *this, u32 pos)
{
    assert(pos < this->len);
    this->cur = this->ptr + pos;
}

void jsm_string_sprintf(struct jsm_string *this, const char* format, ...)
{
    if (this->ptr == NULL) {
        assert(1==0);
        return;
    }
    va_list va;
    va_start(va, format);
    vsnprintf(this->cur, this->len - (this->cur - this->ptr), format, va);
    va_end(va);
    this->cur += strlen(this->cur);
}

void jsm_string_empty(struct jsm_string *this)
{
    if (this->ptr == NULL) {
        assert(1==0);
        return;
    }
    this->cur = this->ptr;
    memset(this->ptr, 0, this->len);
}