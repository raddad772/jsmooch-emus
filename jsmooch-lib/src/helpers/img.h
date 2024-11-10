//
// Created by . on 11/10/24.
//

#ifndef JSMOOCH_EMUS_IMG_H
#define JSMOOCH_EMUS_IMG_H

#include "int.h"
#include "buf.h"

#ifdef __cplusplus
extern "C" {
#endif

enum JSI_format {
    JSIF_plain
};

enum JSI_pixel_format {
    JSIF_ARGB
};

struct jsimg {
    struct buf data;
    u32 width, height;
    enum JSI_format format;
    enum JSI_pixel_format pixel_format;
};

void jsimg_init(struct jsimg *);
void jsimg_delete(struct jsimg *);
void jsimg_allocate(struct jsimg *, u32 width, u32 height);
void jsimg_clear(struct jsimg *);

#ifdef __cplusplus
}
#endif

#endif //JSMOOCH_EMUS_IMG_H
