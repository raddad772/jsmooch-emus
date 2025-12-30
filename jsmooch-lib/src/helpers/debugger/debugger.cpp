//
// Created by . on 8/7/24.
//

#include <cstdlib>
#include <cstdio>
#include <cassert>

#include "debugger.h"
#include "disassembly.h"
#include "console.h"

enum dvur {
    dvur_frame,
    dvur_on_line,
};

static void debugger_view_do_update(debugger_interface *dbgr, debugger_view &dview, dvur reason)
{
    // TODO: this
}

void debugger_report_frame(debugger_interface *dbgr)
{
    if (dbgr == nullptr) return;
    for (auto &dview : dbgr->views) {
        if (dview.kind == dview_events) {
            dview.events.report_frame();
        }
    }
}

void debugger_report_line(debugger_interface *dbgr, i32 line_num)
{
    assert(line_num>=0);
    if (!dbgr) return;
    for (auto &dview : dbgr->views) {
        if (dview.kind == dview_events) {
            dview.events.report_line(line_num);
        }
        if ((dview.update.on_line.enabled) && (dview.update.on_line.line_num == line_num))
            debugger_view_do_update(dbgr, dview, dvur_on_line);
    }
}

cvec_ptr<debugger_view> debugger_interface::make_view(debugger_view_kinds kind)
{
    views.emplace_back(kind);
    return cvec_ptr(views, views.size() - 1);
}

void debugger_interface_dirty_mem(debugger_interface *dbgr, u32 mem_bus, u32 addr_start, u32 addr_end)
{
    if (dbgr == nullptr) return;
    for (auto &dv : dbgr->views) {
        switch(dv.kind) {
            case dview_null:
            case dview_events:
            case dview_image:
            case dview_trace:
            case dview_waveforms:
            case dview_console:
            case dview_dbglog:
            case dview_source_listing:
                break;
            case dview_memory: {
                dv.memory.force_refresh = 1;
                break; }
            case dview_disassembly: {
                disassembly_view &dview = dv.disassembly;
                dview.dirty_mem(mem_bus, addr_start, addr_end);
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

debugger_view::debugger_view(debugger_view_kinds kind) : kind(kind), memory{}
{
    update.on_break = UPDATE_ON_BREAK_DEFAULT;
    update.on_line.enabled = UPDATE_ON_LINE_ENABLED_DEFAULT;
    update.on_line.line_num = UPDATE_ON_LINE_NUM_DEFAULT;
    update.on_pause = UPDATE_ON_PAUSE_DEFAULT;
    update.on_step = UPDATE_ON_STEP_DEFAULT;
    switch(kind) {
        case dview_null:
            assert(1==2);
            break;
        case dview_dbglog:
            new (&dbglog) dbglog_view();
            break;
        case dview_console:
            new (&console) console_view();
            break;
        case dview_image:
            new (&image) image_view();
            break;
        case dview_disassembly:
            new (&disassembly) disassembly_view();
            break;
        case dview_memory:
            new (&memory) memory_view();
            break;
        case dview_events:
            new (&events) events_view();
            break;
        case dview_source_listing:
            new (&source_listing) source_listing::view();
            break;
        case dview_waveforms:
            new (&waveform) waveform_view();
            break;
        case dview_trace:
            new (&trace) trace_view();
            break;
        default:
            assert(1==2);
    }
}

void debugger_widget::move_from(debugger_widget&& other) noexcept {
    kind = other.kind;
    same_line = other.same_line;
    enabled = other.enabled;
    visible = other.visible;

    // Move construct the active member
    switch (kind) {
        case JSMD_button:
            new (&button) debugger_widget_button(std::move(other.button));
            break;
        case JSMD_checkbox:
            new (&checkbox) debugger_widget_checkbox(std::move(other.checkbox));
            break;
        case JSMD_radiogroup:
            new (&radiogroup) debugger_widget_radiogroup(std::move(other.radiogroup));
            break;
        case JSMD_textbox:
            new (&textbox) debugger_widget_textbox(std::move(other.textbox));
            break;
        case JSMD_colorkey:
            new (&colorkey) debugger_widget_colorkey(std::move(other.colorkey));
            break;
        default:
            break;
    }
}

void debugger_widget::make(JSMD_widgets mkind)
{
    destroy_active();
    kind = mkind;
    switch(mkind) {
        case JSMD_button:
            new (&button) debugger_widget_button();
            break;
        case JSMD_checkbox:
            new (&checkbox) debugger_widget_checkbox();
            break;
        case JSMD_radiogroup:
            new (&radiogroup) debugger_widget_radiogroup();
            break;
        case JSMD_colorkey:
            new (&colorkey) debugger_widget_colorkey;
            break;
        case JSMD_textbox:
            new (&textbox) debugger_widget_textbox();
            break;
        default:
            NOGOHERE;
    }
}

void debugger_widget::destroy_active() {
    switch(kind) {
        case JSMD_none:
            break;
        case JSMD_button:
            button.~debugger_widget_button();
            break;
        case JSMD_colorkey:
            colorkey.~debugger_widget_colorkey();
            break;
        case JSMD_checkbox:
            checkbox.~debugger_widget_checkbox();
            break;
        case JSMD_radiogroup:
            radiogroup.~debugger_widget_radiogroup();
            break;
        case JSMD_textbox:
            textbox.~debugger_widget_textbox();
            break;
        default:
            NOGOHERE;
    }
    kind = JSMD_none;
}

debugger_widget::~debugger_widget()
{
    destroy_active();
}

void debugger_widgets_add_textbox(std::vector<debugger_widget> &widgets, const char *text, u32 same_line)
{
    debugger_widget &w = widgets.emplace_back();
    w.make(JSMD_textbox);
    w.same_line = same_line;
    w.textbox.contents.sprintf("%s", text);
    w.enabled = 1;
    w.visible = 1;
}

void debugger_widget_textbox::clear()
{
    contents.quickempty();
}

int debugger_widget_textbox::sprintf(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    int num = contents.vsprintf(format, va);
    va_end(va);
    return num;
}

void debugger_widgets_add_button(std::vector<debugger_widget> &widgets, const char *text, u32 enabled, u32 same_line)
{
    debugger_widget &w = widgets.emplace_back();
    w.make(JSMD_button);
    w.same_line = same_line;
    snprintf(w.button.text, sizeof(w.button.text), "%s", text);
    w.enabled = enabled;
    w.button.value = 0;
    w.visible = 1;
}


void debugger_widgets_add_checkbox(std::vector<debugger_widget> &widgets, const char *text, bool enabled, bool default_value, bool same_line)
{
    debugger_widget &w = widgets.emplace_back();
    w.make(JSMD_checkbox);
    w.same_line = same_line;
    snprintf(w.checkbox.text, sizeof(w.checkbox.text), "%s", text);
    w.enabled = enabled;
    w.checkbox.value = default_value;
    w.visible = true;
}

debugger_widget &debugger_widgets_add_color_key(std::vector<debugger_widget> &widgets, const char *default_text, u32 default_visible)
{
    debugger_widget &w = widgets.emplace_back();
    w.make(JSMD_colorkey);
    w.enabled = 1;
    w.same_line = 1;
    snprintf(w.colorkey.title, sizeof(w.colorkey.title), "%s", default_text);
    w.visible = default_visible;
    return w;
}

debugger_widget &debugger_widgets_add_radiogroup(std::vector<debugger_widget> &widgets, const char *text, u32 enabled, u32 default_value, u32 same_line)
{
    debugger_widget &w = widgets.emplace_back();
    w.make(JSMD_radiogroup);
    w.same_line = same_line;
    w.enabled = enabled;
    snprintf(w.radiogroup.title, sizeof(w.radiogroup.title), "%s", text);
    w.radiogroup.value = default_value;
    w.visible = 1;
    return w;
}

void debugger_widget_colorkey::set_title(const char *str)
{
    snprintf(title, sizeof(title), "%s", str);
    num_items = 0;
}

void debugger_widget_colorkey::add_item(const char *str, u32 color)
{
    u32 num = num_items++;
    assert(num_items < 50);
    debugger_widget_colorkey_item *item = &items[num];
    snprintf(item->name, sizeof(item->name), "%s", str);
    item->color = color;
    item->hovered = 0;
}


void debugger_widget_radiogroup::add_button(const char *text, u32 invalue, u32 same_line)
{
    debugger_widget &w = buttons.emplace_back();
    w.make(JSMD_checkbox);
    w.same_line = same_line;
    w.enabled = 1;
    snprintf(w.checkbox.text, sizeof(w.checkbox.text), "%s", text);
    w.checkbox.value = invalue;
    w.visible = 1;
}

void debugger_view::move_union_from(debugger_view&& other) {
    switch (other.kind) {
        case dview_disassembly:
            new (&disassembly) disassembly_view(std::move(other.disassembly)); break;
        case dview_source_listing:
            new (&source_listing) source_listing::view(std::move(other.source_listing)); break;
        case dview_events:
            new (&events) events_view(std::move(other.events)); break;
        case dview_image:
            new (&image) image_view(std::move(other.image)); break;
        case dview_waveforms:
            new (&waveform) waveform_view(std::move(other.waveform)); break;
        case dview_trace:
            new (&trace) trace_view(std::move(other.trace)); break;
        case dview_console:
            new (&console) console_view(std::move(other.console)); break;
        case dview_dbglog:
            new (&dbglog) dbglog_view(std::move(other.dbglog)); break;
        case dview_memory:
            new (&memory) memory_view(std::move(other.memory)); break;
        default:
            break;
    }
}

debugger_view::~debugger_view() {
    destroy_active();
}

void debugger_view::destroy_active()
{
    options.clear();
    switch(kind) {
        case dview_null:
            break;
        case dview_disassembly:
            disassembly.~disassembly_view();
            break;
        case dview_source_listing:
            source_listing.~view();
            break;
        case dview_events:
            events.~events_view();
            break;
        case dview_memory:
            memory.~memory_view();
            break;
        case dview_dbglog:
            dbglog.~dbglog_view();
            break;
        case dview_console:
            console.~console_view();
            break;
        case dview_image:
            image.~image_view();
            break;
        case dview_waveforms:
            waveform.~waveform_view();
            break;
        case dview_trace:
            trace.~trace_view();
            break;
        default:
            assert(1==2);
    }
}
