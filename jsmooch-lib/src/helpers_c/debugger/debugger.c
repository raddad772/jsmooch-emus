//
// Created by . on 8/7/24.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>

#include "helpers/ooc.h"

#include "debugger.h"
#include "disassembly.h"
#include "events.h"
#include "image.h"
#include "waveform.h"
#include "trace.h"
#include "console.h"
#include "dbglog.h"
#include "memory.h"

enum dvur {
    dvur_frame,
    dvur_on_line,
};

static void debugger_view_do_update(debugger_interface *dbgr, debugger_view *dview, enum dvur reason)
{
    // TODO: this
}

void debugger_report_frame(debugger_interface *dbgr)
{
    if (dbgr == NULL) return;
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dview = cvec_get(&dbgr->views, i);
        if (dview->kind == dview_events) {
            events_view_report_frame(&dview->events);
        }
    }
}

void debugger_report_line(debugger_interface *dbgr, i32 line_num)
{
    assert(line_num>=0);
    if (!dbgr) return;
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dview = cvec_get(&dbgr->views, i);
        if (dview->kind == dview_events) {
            events_view_report_line(&dview->events, line_num);
        }
        if ((dview->update.on_line.enabled) && (dview->update.on_line.line_num == line_num))
            debugger_view_do_update(dbgr, dview, dvur_on_line);
    }
}

struct cvec_ptr debugger_view_new(debugger_interface *this, enum debugger_view_kinds kind)
{
    struct debugger_view *n = cvec_push_back(&this->views);
    debugger_view_init(n, kind);
    return make_cvec_ptr(&this->views, cvec_len(&this->views)-1);
}

void debugger_interface_dirty_mem(debugger_interface *dbgr, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    if (dbgr == NULL) return;
    for (u32 i = 0; i < cvec_len(&dbgr->views); i++) {
        struct debugger_view *dv = cvec_get(&dbgr->views, i);
        switch(dv->kind) {
            case dview_null:
            case dview_events:
            case dview_image:
            case dview_trace:
            case dview_waveforms:
            case dview_console:
            case dview_dbglog:
                break;
            case dview_memory: {
                struct memory_view *mv = &dv->memory;
                mv->force_refresh = 1;
                break; }
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

void debugger_view_init(debugger_view *this, enum debugger_view_kinds kind)
{
    CTOR_ZERO_SELF;
    this->kind = kind;
    this->update.on_break = UPDATE_ON_BREAK_DEFAULT;
    this->update.on_line.enabled = UPDATE_ON_LINE_ENABLED_DEFAULT;
    this->update.on_line.line_num = UPDATE_ON_LINE_NUM_DEFAULT;
    this->update.on_pause = UPDATE_ON_PAUSE_DEFAULT;
    this->update.on_step = UPDATE_ON_STEP_DEFAULT;
    cvec_init(&this->options, sizeof(debugger_widget), 5);
    switch(this->kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_dbglog:
            dbglog_view_init(&this->dbglog);
            break;
        case dview_console:
            console_view_init(&this->console);
            break;
        case dview_image:
            image_view_init(&this->image);
            break;
        case dview_disassembly:
            disassembly_view_init(&this->disassembly);
            break;
        case dview_memory:
            memory_view_init(&this->memory);
            break;

        case dview_events:
            events_view_init(&this->events);
            break;
        case dview_waveforms:
            waveform_view_init(&this->waveform);
            break;
        case dview_trace:
            trace_view_init(&this->trace);
            break;
        default:
            assert(1==2);
    }
}

void debugger_widget_init(debugger_widget *this, enum JSMD_widgets kind)
{
    memset(this, 0, sizeof(debugger_widget));
    this->kind = kind;

    switch(this->kind) {
        case JSMD_checkbox:
            break;
        case JSMD_radiogroup:
            cvec_init(&this->radiogroup.buttons, sizeof(debugger_widget), 5);
            break;
        case JSMD_colorkey:
            this->colorkey.num_items = 0;
            this->colorkey.title[0] = 0;
            break;
        case JSMD_textbox:
            jsm_string_init(&this->textbox.contents, 4000);
            break;
        default:
            NOGOHERE;
    }
}

void debugger_widget_delete(debugger_widget *this)
{
    if (!this) return;
    switch(this->kind) {
        case JSMD_colorkey:
        case JSMD_checkbox:
            break;
        case JSMD_radiogroup:
            cvec_delete(&this->radiogroup.buttons);
            break;
        case JSMD_textbox:
            jsm_string_delete(&this->textbox.contents);
            break;
        default:
            NOGOHERE;
    }
}

void debugger_widgets_add_textbox(cvec *widgets, char *text, u32 same_line)
{
    struct debugger_widget *w = cvec_push_back(widgets);
    debugger_widget_init(w, JSMD_textbox);
    w->same_line = same_line;
    jsm_string_sprintf(&w->textbox.contents, "%s", text);
    w->enabled = 1;
    w->visible = 1;
}

void debugger_widgets_textbox_clear(debugger_widget_textbox *tb)
{
    jsm_string_quickempty(&tb->contents);
}

int debugger_widgets_textbox_sprintf(debugger_widget_textbox *tb, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    int num = jsm_string_vsprintf(&tb->contents, format, va);
    va_end(va);
    return num;
}

void debugger_widgets_add_checkbox(cvec *widgets, const char *text, u32 enabled, u32 default_value, u32 same_line)
{
    struct debugger_widget *w = cvec_push_back(widgets);
    debugger_widget_init(w, JSMD_checkbox);
    w->same_line = same_line;
    snprintf(w->checkbox.text, sizeof(w->checkbox.text), "%s", text);
    w->enabled = enabled;
    w->checkbox.value = default_value;
    w->visible = 1;
}

struct debugger_widget *debugger_widgets_add_color_key(cvec *widgets, const char *default_text, u32 default_visible)
{
    struct debugger_widget *w = cvec_push_back(widgets);
    debugger_widget_init(w, JSMD_colorkey);
    w->enabled = 1;
    w->same_line = 1;
    snprintf(w->colorkey.title, sizeof(w->colorkey.title), "%s", default_text);
    w->visible = default_visible;
    return w;
}

struct debugger_widget *debugger_widgets_add_radiogroup(cvec* widgets, const char *text, u32 enabled, u32 default_value, u32 same_line)
{
    struct debugger_widget *w = cvec_push_back(widgets);
    debugger_widget_init(w, JSMD_radiogroup);
    w->same_line = same_line;
    w->enabled = enabled;
    snprintf(w->radiogroup.title, sizeof(w->radiogroup.title), "%s", text);
    w->radiogroup.value = default_value;
    w->visible = 1;
    return w;
}

void debugger_widgets_colorkey_set_title(debugger_widget_colorkey *this, const char *str)
{
    snprintf(this->title, sizeof(this->title), "%s", str);
    this->num_items = 0;
}

void debugger_widgets_colorkey_add_item(debugger_widget_colorkey *this, const char *str, u32 color)
{
    u32 num = this->num_items++;
    assert(this->num_items < 50);
    struct debugger_widget_colorkey_item *item = &this->items[num];
    snprintf(item->name, sizeof(item->name), "%s", str);
    item->color = color;
    item->hovered = 0;
}


void debugger_widget_radiogroup_add_button(debugger_widget *radiogroup, const char *text, u32 value, u32 same_line)
{
    struct debugger_widget *w = cvec_push_back(&radiogroup->radiogroup.buttons);
    debugger_widget_init(w, JSMD_checkbox);
    w->same_line = same_line;
    w->enabled = 1;
    snprintf(w->checkbox.text, sizeof(w->checkbox.text), "%s", text);
    w->checkbox.value = value;
    w->visible = 1;
}

void debugger_view_delete(debugger_view *this)
{
    for (u32 i = 0; i < cvec_len(&this->options); i++) {
        struct debugger_widget *p = cvec_get(&this->options, i);
        debugger_widget_delete(p);
    }
    cvec_delete(&this->options);
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
        case dview_memory:
            memory_view_delete(&this->memory);
            break;
        case dview_dbglog:
            dbglog_view_delete(&this->dbglog);
            break;
        case dview_console:
            console_view_delete(&this->console);
            break;
        case dview_image:
            image_view_delete(&this->image);
            break;
        case dview_waveforms:
            waveform_view_delete(&this->waveform);
            break;
        case dview_trace:
            trace_view_delete(&this->trace);
            break;
        default:
            assert(1==2);
    }
}

void debugger_interface_init(debugger_interface *this)
{
    CTOR_ZERO_SELF;
    cvec_init(&this->views, sizeof(debugger_view), 10);
}

void debugger_interface_delete(debugger_interface *this)
{
    DTOR_child_cvec(views, debugger_view);
}
