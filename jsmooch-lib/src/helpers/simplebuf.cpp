//
// Created by . on 8/29/24.
//
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
    if (ptr == nullptr) return;
    memset(ptr, 0, sz);
}

int popcount(u64 n)
{
    int c = 0;
    for (; n; ++c)
        n &= n - 1;
    return c;
}

void simplebuf8::allocate(u64 insz)
{
    if (ptr != nullptr) {
        free(ptr);
    }
    ptr = static_cast<u8 *>(malloc(insz));
    sz = insz;
    mask = sz - 1; // only use if we're a power of 2
}

void simplebuf8::copy_from_buf(buf &src)
{
    if (src.ptr == nullptr) {
        if (ptr) free(ptr);
        ptr = nullptr;
        sz = 0;
        mask = 0;
        return;
    }

    allocate(src.size);
    if (src.size > 0)
        memcpy(ptr, src.ptr, src.size);
    sz = src.size;
    mask = sz - 1; // only use if we're definitely a power of 2
}