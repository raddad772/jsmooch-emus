//
// Created by . on 11/10/24.
//

#pragma once

#include "int.h"
#include "buf.h"

enum JSI_format {
    JSIF_plain
};

enum JSI_pixel_format {
    JSIF_ARGB
};

struct jsimg {
    buf data{};
    u32 width, height{};
    JSI_format format{};
    JSI_pixel_format pixel_format{};

    void allocate(u32 mwidth, u32 mheight);
    void clear();
};
