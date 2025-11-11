//
// Created by . on 11/10/24.
//

#include <string.h>

#include "img.h"

void jsimg_init(jsimg *this)
{
    memset(this, 0, sizeof(*this));
    buf_init(&this->data);
}

void jsimg_delete(jsimg *this)
{
    buf_delete(&this->data);
}

void jsimg_allocate(jsimg *this, u32 width, u32 height)
{
    buf_allocate(&this->data, width*height*4);
}

void jsimg_clear(jsimg *this)
{
    if (this->data.ptr) {
        memset(this->data.ptr, 0, this->data.size);
    }
}