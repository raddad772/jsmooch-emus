//
// Created by Dave on 2/14/2024.
//

#include "assert.h"
#include "stdlib.h"
#include <cstring>
#include "stdio.h"
#include "stdarg.h"

#include "jsm_string.h"
#include "debug.h"

void jsm_string_init(jsm_string *this, u32 sz)
{
    this->ptr = this->cur = NULL;
    this->allocated_len = sz;
    this->ptr = this->cur = malloc(sz);
    memset(this->ptr, 0, sz);
}

void jsm_string_delete(jsm_string *this)
{
    if (this->ptr != NULL) {
        free(this->ptr);
        this->ptr = NULL;
    }
    this->cur = NULL;
    this->allocated_len = 0;
}

void jsm_string_seek(jsm_string *this, i32 pos)
{
    assert(pos < this->allocated_len);
    this->cur = this->ptr + pos;
}

int jsm_string_vsprintf(jsm_string *this, const char* format, va_list va)
{
    int num;
    num = vsnprintf(this->cur, this->allocated_len - (this->cur - this->ptr), format, va);
    this->cur += num;
    assert((this->cur - this->ptr) < this->allocated_len);
    return num;
}

int jsm_string_sprintf(jsm_string *this, const char* format, ...)
{
    if (this->ptr == NULL) {
        assert(1==0);
        return 0;
    }
    int num;
    va_list va;
    va_start(va, format);
    num = jsm_string_vsprintf(this, format, va);
    va_end(va);
    return num;
}

void jsm_string_empty(jsm_string *this)
{
    if (this->ptr == NULL) {
        assert(1==0);
        return;
    }
    this->cur = this->ptr;
    memset(this->ptr, 0, this->allocated_len);
}

void jsm_string_quickempty(jsm_string *this)
{
    if (this->ptr == NULL) {
        assert(1==0);
        return;
    }
    this->cur = this->ptr;
    *this->cur = 0;
}

// thanks https://stackoverflow.com/questions/744766/how-to-compare-ends-of-strings-in-c
u32 ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

