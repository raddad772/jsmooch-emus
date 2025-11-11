//
// Created by . on 11/10/24.
//

#include <cstring>

#include "img.h"

void jsimg_allocate(jsimg *this, u32 width, u32 height)
{
    data.allocate(width*height*4);
}

void jsimg_clear(jsimg *this)
{
    if (data.ptr) {
        memset(data.ptr, 0, data.size);
    }
}