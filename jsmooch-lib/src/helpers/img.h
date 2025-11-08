//
// Created by . on 11/10/24.
//

#ifndef JSMOOCH_EMUS_IMG_H
#define JSMOOCH_EMUS_IMG_H

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

    void allocate(u32 width, u32 height);;
    void clear();
};

#endif //JSMOOCH_EMUS_IMG_H
