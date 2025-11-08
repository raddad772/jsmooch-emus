//
// Created by . on 8/11/24.
//
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "helpers/cvec.h"
#include "helpers/ooc.h"
#include "helpers/physical_io.h"
#include "events.h"

void debugger_event_init(struct debugger_event *this)
{
    cvec_init(&this->updates[0], sizeof(struct debugger_event_update), 100);
    cvec_init(&this->updates[1], sizeof(struct debugger_event_update), 100);
    this->updates_index = 1;
    cvec_ptr_init(&this->category);
    this->tag.context[0] = 0;
}

void debugger_event_delete(struct debugger_event *this)
{
    cvec_ptr_delete(&this->category);
    cvec_delete(&this->updates[0]);
    cvec_delete(&this->updates[1]);
    this->tag.context[0] = 0;
}

void event_category_init(struct event_category *this)
{
    this->name[0] = 0;
}


void event_category_delete(struct event_category *this)
{
    this->name[0] = 0;
}

void events_view_add_category(struct debugger_interface *dbgr, struct events_view *ev, const char *name, u32 color, u32 id)
{
    assert(id==cvec_len(&ev->categories));
    struct event_category *ec = cvec_push_back(&ev->categories);
    event_category_init(ec);
    snprintf(ec->name, sizeof(ec->name), "%s", name);
    ec->color = color;
    ec->id = id;
}

void events_view_add_event(struct debugger_interface *dbgr, struct events_view *ev, u32 category_id, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id)
{
    if (cvec_len(&ev->events) <= id) {
        assert(1==0);
    }

    struct debugger_event *event = cvec_get(&ev->events, id);
    debugger_event_init(event);

    snprintf(event->name, sizeof(event->name), "%s", name);
    event->category = make_cvec_ptr(&ev->categories, category_id);
    event->color = color;
    event->category_id = category_id;
    event->display_enabled = default_enable;
}

void events_view_report_frame(struct events_view *this)
{
    this->last_frame = this->current_frame;
    this->current_frame++;
    this->index_in_use ^= 1;
    for (u32 i = 0; i < cvec_len(&this->events); i++) {
        struct debugger_event *ev = cvec_get(&this->events, i);
        ev->updates_index ^= 1;
        cvec_clear(&ev->updates[ev->updates_index]);
    }
    this->master_clocks.cur_line = 0;
    this->master_clocks.back_buffer ^= 1;
    this->master_clocks.front_buffer ^= 1;
}

void events_view_init(struct events_view *this)
{
    for (u32 i = 0; i < 2; i++) {
        this->display[i].buf = NULL;
        this->display[i].frame_num = -2;
        this->display[i].width = this->display[i].height = 0;
    }
    this->index_in_use = 0;

    this->current_frame = -1;
    this->last_frame = -1;

    cvec_init(&this->events, sizeof(struct debugger_event), 1000);
    cvec_init(&this->categories, sizeof(struct event_category), 100);

    this->master_clocks.back_buffer = 1;
    this->master_clocks.front_buffer = 0;
}

void events_view_delete(struct events_view *this)
{
    DTOR_child_cvec(events, debugger_event);
    DTOR_child_cvec(categories, event_category);
    DTOR_child(associated_display, cvec_ptr);
}

u64 events_view_get_current_line(struct cvec_ptr viewptr)
{
    if (viewptr.vec == NULL) {
        return 0;
    }
    struct events_view *this = &((struct debugger_view *)cpg(viewptr))->events;
    return this->master_clocks.cur_line;
}

u64 events_view_get_current_line_start(struct cvec_ptr viewptr)
{
    if (viewptr.vec == NULL) {
        return 0;
    }
    struct events_view *this = &((struct debugger_view *)cpg(viewptr))->events;
    u32 line_num = this->master_clocks.cur_line;
    return this->master_clocks.lines[this->master_clocks.back_buffer][line_num];
}

u64 events_view_get_current_line_pos(struct cvec_ptr viewptr)
{
    if (viewptr.vec == NULL) {
        return 0;
    }
    struct events_view *this = &((struct debugger_view *)cpg(viewptr))->events;
    u32 line_num = this->master_clocks.cur_line;
    return (*this->master_clocks.ptr) - this->master_clocks.lines[this->master_clocks.back_buffer][line_num];
}

void debugger_report_event(struct cvec_ptr viewptr, i32 event_id)
{
    if (viewptr.vec == NULL) {
        return;
    }
    if (event_id == -1) {
        return;
    }
    struct events_view *this = &((struct debugger_view *)cpg(viewptr))->events;
    struct debugger_event *ev = cvec_get(&this->events, event_id);
    struct JSM_DISPLAY *d = &((struct physical_io_device *)cpg(this->associated_display))->display;
    struct debugger_event_update *upd = cvec_push_back(&ev->updates[ev->updates_index]);
    upd->frame_num = this->current_frame;
    switch(this->timing) {
        case ev_timing_scanxy:
            upd->scan_x = d->scan_x;
            upd->scan_y = d->scan_y;
            return;
        case ev_timing_master_clock:
            upd->mclks = *this->master_clocks.ptr;
            upd->scan_y = this->master_clocks.cur_line;
            return;
        default:
            NOGOHERE;
    }
}

static void draw_box_3x3(u32 *buf, u32 x_center, u32 y_center, u32 out_width, u32 out_height, u32 color)
{
    u32 max_ptr = (out_width * out_height);
    if (y_center < 1) y_center = 1;
    if (x_center < 1) x_center = 1;
    u32 buf_ptr = ((y_center - 1) * out_width) + (x_center - 1);
    for (u32 y = 0; y < 3; y++) {
        for (u32 x = 0; x < 3; x++) {
            if (buf_ptr<max_ptr)
                buf[buf_ptr] = 0xFF000000 | color;
            buf_ptr++;
        }
        buf_ptr = (buf_ptr + out_width) - 3;
        //assert(buf_ptr<max_ptr);
        //assert((y+y_center) < (out_height - 1));
    }
}

void events_view_report_draw_start(struct events_view *this)
{
    switch(this->timing) {
        case ev_timing_master_clock:
            this->master_clocks.draw_starts[this->master_clocks.back_buffer][this->master_clocks.cur_line] = *this->master_clocks.ptr;
            //printf("\nLine %d Draw start %lld", this->master_clocks.cur_line, this->master_clocks.draw_starts[this->master_clocks.back_buffer][this->master_clocks.cur_line] - this->master_clocks.lines[this->master_clocks.back_buffer][this->master_clocks.cur_line]);
            break;
        case ev_timing_scanxy:
            printf("OHNO");
            break;
        default:
            NOGOHERE;
    }
}

void events_view_report_line(struct events_view *this, i32 line_num)
{
    switch(this->timing) {
        case ev_timing_master_clock: {
            /*i32 last_line = line_num - 1;
            if (last_line > 0) {
                u64 line_len = *this->master_clocks.ptr - this->master_clocks.lines[this->master_clocks.back_buffer][last_line];
                printf("\nLINE LEN %lld", line_len);
            }*/
            this->master_clocks.lines[this->master_clocks.back_buffer][line_num] = *this->master_clocks.ptr;
            this->master_clocks.draw_starts[this->master_clocks.back_buffer][line_num] = *this->master_clocks.ptr;
            this->master_clocks.cur_line = line_num;
            break; }
        case ev_timing_scanxy:
            break;
        default:
        NOGOHERE;
    }
}

void events_view_render(struct debugger_interface *dbgr, struct events_view *this, u32 *buf, u32 out_width, u32 out_height)
{
    // Render our current events to buf
    u32 frame_to_use = this->current_frame - 1;
    for (u32 i = 0; i < cvec_len(&this->events); i++) {
        struct debugger_event *ev = cvec_get(&this->events, i);
        if (!ev->display_enabled) continue;
        for (u32 j = 0; j < cvec_len(&ev->updates[ev->updates_index ^ 1]); j++) {
            struct debugger_event_update *upd = cvec_get(&ev->updates[ev->updates_index ^ 1], j);
            //assert(upd->scan_y < this->display[0].height);
            if (upd->scan_y >= this->display[0].height) {
                printf("\nUPD OOB %d", upd->scan_y);
                upd->scan_y = this->display[0].height - 1;
            }
            switch(this->timing) {
                case ev_timing_scanxy:
                    draw_box_3x3(buf, upd->scan_x, upd->scan_y, out_width, out_height, ev->color);
                    break;
                case ev_timing_master_clock: {
                    i64 mc_per_line = this->master_clocks.per_line;
                    i64 mc_cur = this->master_clocks.lines[this->master_clocks.front_buffer][upd->scan_y];
                    i64 clk = (i64)upd->mclks - mc_cur;
                    u32 sx;
                    float perc = (float)clk / ((float)mc_per_line);
                    if (clk < 0) {
                        //printf("\nOH NO ITS OFF THE LEFT");
                        sx = 1;
                    }
                    else if (clk > mc_per_line) {
                        sx = this->display[0].width - 1;
                    }
                    else {
                        sx = (u32)(perc * ((float)this->display[0].width));
                    }
                    draw_box_3x3(buf, sx, upd->scan_y, out_width, out_height, ev->color);
                    break; }
                default:
                    NOGOHERE;

            }
        }
    }
}