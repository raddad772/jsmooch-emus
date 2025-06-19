//
// Created by . on 6/19/25.
//

#ifndef JSMOOCH_EMUS_HUC6260_H
#define JSMOOCH_EMUS_HUC6260_H

#include "helpers/int.h"
#include "helpers/cvec.h"

struct HUC6260 {
    u16 *cur_output;
    u32 cur_pixel;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;

    u16 CRAM[512]; // 256 bg, 256 sprite
};


void HUC6260_init(struct HUC6260 *);
void HUC6260_delete(struct HUC6260 *);
void HUC6260_reset(struct HUC6260 *);

#endif //JSMOOCH_EMUS_HUC6260_H
