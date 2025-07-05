//
// Created by . on 6/18/25.
// HUC6270 GPU for TurboGrafx-16 and later consoles too

#ifndef JSMOOCH_EMUS_HUC6270_H
#define JSMOOCH_EMUS_HUC6270_H

#include "helpers/int.h"

struct HUC6270 {
    u16 line_output[512]; // 512x240 max

    struct {
        u16 BYR;
    } io;

#ifndef _MSC_VER // error C2016: C requires that a struct or union have at least one member
    struct {

    } latch;
#endif
    struct {
        u32 y;

        i32 yscroll, next_yscroll;
    } regs;
};


void HUC6270_init(struct HUC6270 *);
void HUC6270_delete(struct HUC6270 *);
void HUC6270_reset(struct HUC6270 *);

#endif //JSMOOCH_EMUS_HUC6270_H
