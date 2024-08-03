//
// Created by . on 7/19/24.
//

#ifndef JSMOOCH_EMUS_YM2612_H
#define JSMOOCH_EMUS_YM2612_H

#include "helpers/int.h"

struct ym2612 {
    struct {

    } pins;

    struct {
        u32 line;
    } timer_a;

    struct {
        u32 line;
    } timer_b;
};

void ym2612_init(struct ym2612*);
void ym2612_delete(struct ym2612*);

void ym2612_write(struct ym2612*, u32 addr, u8 val);
u8 ym2612_read(struct ym2612*, u32 addr, u32 old, u32 has_effect);
void ym2612_reset(struct ym2612*);

#endif //JSMOOCH_EMUS_YM2612_H
