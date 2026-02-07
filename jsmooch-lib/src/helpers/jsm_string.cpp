//
// Created by Dave on 2/14/2024.
//

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cstdio>

#include "jsm_string.h"
#include "helpers/debug.h"

jsm_string::jsm_string(u32 sz)
{
    ptr = cur = nullptr;
    allocated_len = sz;
    ptr = cur = static_cast<char *>(malloc(sz));
    memset(ptr, 0, sz);
}

jsm_string::~jsm_string()
{
    if (ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
    cur = nullptr;
    allocated_len = 0;
}

void jsm_string::seek(i32 pos)
{
    assert(pos < allocated_len);
    cur = ptr + pos;
}

void jsm_string::trim_right() {
    return;
    char *p = cur - 1;
    while (p>ptr) {
        switch (*p) {
            case ' ':
            case '\n':
            case 0x0D:
            case '\t':
                *p = 0;
                p--;
                continue;
            default:
                p++;
                cur = p;
                break;
        }
    }

}

int jsm_string::vsprintf(const char* format, va_list va)
{
    int num;
    num = vsnprintf(cur, allocated_len - (cur - ptr), format, va);
    cur += num;
    u32 len = (cur - ptr);
    assert((cur - ptr) < allocated_len);
    return num;
}

int jsm_string::sprintf(const char* format, ...)
{
    if (ptr == nullptr) {
        assert(1==0);
        return 0;
    }
    va_list va;
    va_start(va, format);
    int num = this->vsprintf(format, va);
    va_end(va);
    return num;
}

void jsm_string::empty()
{
    if (ptr == nullptr) {
        assert(1==0);
        return;
    }
    cur = ptr;
    memset(ptr, 0, allocated_len);
}

void jsm_string::quickempty()
{
    if (ptr == nullptr) {
        assert(1==0);
        return;
    }
    cur = ptr;
    *cur = 0;
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

