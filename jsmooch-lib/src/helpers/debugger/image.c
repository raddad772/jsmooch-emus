//
// Created by . on 8/26/24.
//
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "image.h"

void image_view_init(struct image_view *this)
{
    memset(this, 0, sizeof(*this));
    buf_init(&this->img_buf[0]);
    buf_init(&this->img_buf[1]);
}

void image_view_delete(struct image_view *this)
{
    buf_delete(&this->img_buf[0]);
    buf_delete(&this->img_buf[1]);
}