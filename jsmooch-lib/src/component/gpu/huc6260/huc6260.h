//
// Created by . on 6/19/25.
//

#ifndef JSMOOCH_EMUS_HUC6260_H
#define JSMOOCH_EMUS_HUC6260_H

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"

struct HUC6270;



struct HUC6260 {
    u16 *cur_output;
    u16 *cur_line;
    u64 *master_clock;
    struct cvec_ptr display_ptr;
    struct JSM_DISPLAY *display;
    struct scheduler_t *scheduler;

    struct HUC6270 *vdc0, *vdc1;
    u16 CRAM[512]; // 256 bg, 256 sprite
    u64 master_frame;

    struct {
        u32 clock_div;
        u32 num_lines; // 262
        u32 y;

        u64 line_start;
        u32 hblank, vblank;
    } regs;

    struct {
        u32 DCC;
        union UN16 CTA, CTW;
    } io;
};


void HUC6260_init(struct HUC6260 *, u64 *master_clock, struct HUC6270 *vdc0, struct HUC6270 *vdc1);
void HUC6260_delete(struct HUC6260 *);
void HUC6260_reset(struct HUC6260 *);
void HUC6260_write(struct HUC6260 *, u32 addr, u32 val);
u32 HUC6260_read(struct HUC6260 *, u32 addr, u32 old);
void HUC6260_cycle(struct HUC6260 *, u64 clock);
void HUC6260_schedule_first(struct HUC6260 *);
#endif //JSMOOCH_EMUS_HUC6260_H
