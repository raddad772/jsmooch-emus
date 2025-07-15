//
// Created by . on 6/19/25.
//

#ifndef JSMOOCH_EMUS_HUC6260_H
#define JSMOOCH_EMUS_HUC6260_H

#include "helpers/int.h"
#include "helpers/cvec.h"
#include "helpers/scheduler.h"
#include "helpers/debugger/debuggerdefs.h"

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
        u32 hsync, vsync;
        u32 frame_height;
        u32 cycles_per_line;
    } regs;

    struct {
        u32 DCC;
        union UN16 CTA, CTW;
    } io;

    DBG_START
        DBG_EVENT_VIEW_START
        HSYNC_UP, VSYNC_UP, WRITE_CRAM
        DBG_EVENT_VIEW_END

    DBG_END
};

#define HUC6260_CYCLE_PER_LINE 1365
#define HUC6260_DRAW_CYCLES 1129
#define HUC6260_DRAW_START 200
#define HUC6260_HSYNC_END 100
#define HUC6260_DRAW_END 1329
#define HUC6260_DRAW_LINES 242
#define HUC6260_LINE_VSYNC_START 246
#define HUC6260_LINE_VSYNC_END 249 // line to term on, so 246 and 249 = 3 lines


void HUC6260_init(struct HUC6260 *, struct scheduler_t *scheduler, struct HUC6270 *vdc0, struct HUC6270 *vdc1);
void HUC6260_delete(struct HUC6260 *);
void HUC6260_reset(struct HUC6260 *);
void HUC6260_write(struct HUC6260 *, u32 addr, u32 val);
u32 HUC6260_read(struct HUC6260 *, u32 addr, u32 old);
void HUC6260_pixel_clock(void *ptr, u64 key, u64 clock, u32 jitter);
void HUC6260_schedule_first(struct HUC6260 *);
#endif //JSMOOCH_EMUS_HUC6260_H
