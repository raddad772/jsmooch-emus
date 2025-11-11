//
// Created by . on 11/10/24.
//

#include <cstring>

#include "img.h"

void jsimg::allocate(u32 mwidth, u32 mheight)
{
    data.allocate(mwidth*mheight*4);
    width = mwidth;
    height = mheight;
}

void jsimg::clear()
{
    if (data.ptr) {
        memset(data.ptr, 0, data.size);
    }
}