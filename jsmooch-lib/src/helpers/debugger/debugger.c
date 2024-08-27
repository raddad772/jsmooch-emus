//
// Created by . on 8/7/24.
//

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "helpers/ooc.h"

#include "debugger.h"
#include "disassembly.h"
#include "events.h"
#include "image.h"

enum dvur {
    dvur_frame,
    dvur_on_line,
};

static void debugger_view_do_update(struct debugger_interface *dbgr, struct debugger_view *dview, enum dvur reason)
{
    // TODO: this
}

void debugger_report_line(struct debugger_interface *dbgr, i32 line_num)
{
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dview = cvec_get(&dbgr->views, i);
        if ((dview->update.on_line.enabled) && (dview->update.on_line.line_num == line_num))
            debugger_view_do_update(dbgr, dview, dvur_on_line);
    }
}

struct cvec_ptr debugger_view_new(struct debugger_interface *this, enum debugger_view_kinds kind)
{
    struct debugger_view *n = cvec_push_back(&this->views);
    debugger_view_init(n, kind);
    return make_cvec_ptr(&this->views, cvec_len(&this->views)-1);
}

void debugger_interface_dirty_mem(struct debugger_interface *dbgr, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    if (dbgr == NULL) return;
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dv = cvec_get(&dbgr->views, i);
        switch(dv->kind) {
            case dview_null:
            case dview_events:
            case dview_image:
                break;
            case dview_disassembly: {
                struct disassembly_view *dview = &dv->disassembly;
                disassembly_view_dirty_mem(dbgr, dview, mem_bus, addr_start, addr_end);
                break; }
            default:
                assert(1==0);
        }
    }
}


#define UPDATE_ON_BREAK_DEFAULT 1
#define UPDATE_ON_LINE_ENABLED_DEFAULT 1
#define UPDATE_ON_LINE_NUM_DEFAULT 0
#define UPDATE_ON_PAUSE_DEFAULT 1
#define UPDATE_ON_STEP_DEFAULT 1

void debugger_view_init(struct debugger_view *this, enum debugger_view_kinds kind)
{
    CTOR_ZERO_SELF;
    this->kind = kind;
    this->update.on_break = UPDATE_ON_BREAK_DEFAULT;
    this->update.on_line.enabled = UPDATE_ON_LINE_ENABLED_DEFAULT;
    this->update.on_line.line_num = UPDATE_ON_LINE_NUM_DEFAULT;
    this->update.on_pause = UPDATE_ON_PAUSE_DEFAULT;
    this->update.on_step = UPDATE_ON_STEP_DEFAULT;
    switch(this->kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_image:
            image_view_init(&this->image);
            break;
        case dview_disassembly:
            disassembly_view_init(&this->disassembly);
            break;
        case dview_events:
            events_view_init(&this->events);
            break;
        case dview_memory:
            assert(1==2);
            break;
        default:
            assert(1==2);
    }
}

void debugger_view_delete(struct debugger_view *this)
{
    switch(this->kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_disassembly:
            disassembly_view_delete(&this->disassembly);
            break;
        case dview_events:
            events_view_delete(&this->events);
            break;
        case dview_image:
            image_view_delete(&this->image);
            break;
        case dview_memory:
            assert(1==2);
            break;
        default:
            assert(1==2);
    }
}

void debugger_interface_init(struct debugger_interface *this)
{
    CTOR_ZERO_SELF;
    cvec_init(&this->views, sizeof(struct debugger_view), 5);
}

void debugger_interface_delete(struct debugger_interface *this)
{
    DTOR_child_cvec(views, debugger_view);
}
