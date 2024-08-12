//
// Created by . on 8/11/24.
//
#include <string.h>
#include <stdio.h>
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

struct cvec_ptr events_view_add_category(struct debugger_interface *dbgr, struct events_view *ev, const char *name, u32 color, u32 id)
{
    struct event_category *ec = cvec_push_back(&ev->categories);
    event_category_init(ec);
    snprintf(ec->name, sizeof(ec->name), "%s", name);
    ec->color = color;
    ec->id = id;
    return make_cvec_ptr(&ev->categories, cvec_len(&ev->categories)-1);
}

struct cvec_ptr events_view_add_event(struct debugger_interface *dbgr, struct events_view *ev, struct cvec_ptr category, const char *name, u32 color, enum debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context)
{
    struct debugger_event *event = cvec_push_back(&ev->events);
    debugger_event_init(event);

    snprintf(event->name, sizeof(event->name), "%s", name);
    event->category = category;
    event->color = color;
    event->category_id = ((struct event_category *)cpg(category))->id;

    return make_cvec_ptr(&ev->events, cvec_len(&ev->events)-1);
}

void event_view_begin_frame(struct cvec_ptr event_view)
{
    struct events_view *eview = &((struct debugger_view *)cpg(event_view))->events;
    eview->last_frame = eview->current_frame;
    eview->current_frame++;
    eview->index_in_use ^= 1;
    for (u32 i = 0; i < cvec_len(&eview->events); i++) {
        struct debugger_event *ev = cvec_get(&eview->events, i);
        ev->updates_index ^= 1;
        cvec_clear(&ev->updates[ev->updates_index]);
    }
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
}

void events_view_delete(struct events_view *this)
{
    DTOR_child_cvec(events, debugger_event);
    DTOR_child_cvec(categories, event_category);
    DTOR_child(associated_display, cvec_ptr);
}

void debugger_report_event(struct cvec_ptr viewptr, struct cvec_ptr event)
{
    struct events_view *this = &((struct debugger_view *)cpg(viewptr))->events;
    struct debugger_event *ev = cpg(event);
    struct JSM_DISPLAY *d = &((struct physical_io_device *)cpg(this->associated_display))->display;
    struct debugger_event_update *upd = cvec_push_back(&ev->updates[ev->updates_index]);
    upd->frame_num = this->current_frame;
    upd->scan_x = d->scan_x;
    upd->scan_y = d->scan_y;
}

static void draw_box_3x3(u32 *buf, u32 x_center, u32 y_center, u32 out_width, u32 out_height, u32 color)
{
    if (y_center < 1) y_center = 1;
    if (x_center < 1) x_center = 1;
    buf += ((y_center - 1) * out_width) + (x_center - 1);
    for (u32 y = 0; y < 3; y++) {
        for (u32 x = 0; x < 3; x++) {
            *buf = 0xFF000000 | color;
            buf++;
        }
        buf = (buf + out_width) - 3;
        assert((y+y_center) < (out_height - 1));
    }
}


void events_view_render(struct debugger_interface *dbgr, struct events_view *this, u32 *buf, u32 out_width, u32 out_height)
{
    // Render our current events to buf
    u32 frame_to_use = this->current_frame - 1;

    for (u32 i = 0; i < cvec_len(&this->events); i++) {
        struct debugger_event *ev = cvec_get(&this->events, i);
        for (u32 j = 0; j < cvec_len(&ev->updates[ev->updates_index ^ 1]); j++) {
            struct debugger_event_update *upd = cvec_get(&ev->updates[ev->updates_index ^ 1], j);
            draw_box_3x3(buf, upd->scan_x, upd->scan_y, out_width, out_height, ev->color);
        }
    }
}