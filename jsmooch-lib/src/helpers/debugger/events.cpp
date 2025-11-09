//
// Created by . on 8/11/24.
//
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "helpers/cvec.h"
#include "helpers/physical_io.h"
#include "debugger.h"
#include "events.h"

void events_view::add_category(debugger_interface *dbgr, const char *name, u32 color, u32 id)
{
    assert(id==categories.size());
    event_category &ec = categories.emplace_back();
    snprintf(ec.name, sizeof(ec.name), "%s", name);
    ec.color = color;
    ec.id = id;
}

void events_view::add_event(u32 category_id, const char *name, u32 color, debugger_event_kind display_kind, u32 default_enable, u32 order, const char* context, u32 id)
{
    if (events.size() <= id) {
        assert(1==0);
    }

    debugger_event &event = events.at(id);

    snprintf(event.name, sizeof(event.name), "%s", name);
    event.category = cvec_ptr(categories, category_id);;
    event.color = color;
    event.category_id = category_id;
    event.display_enabled = default_enable;
}

void events_view::report_frame()
{
    last_frame = current_frame;
    current_frame++;
    index_in_use ^= 1;
    for (auto &ev : events) {
        ev.updates_index ^= 1;
        ev.updates[ev.updates_index].clear();
    }
    master_clocks.cur_line = 0;
    master_clocks.back_buffer ^= 1;
    master_clocks.front_buffer ^= 1;
}

void events_view::report_event(i32 event_id) {
    debugger_event &ev = events.at(event_id);
    JSM_DISPLAY &d = associated_display.get().display;
    debugger_event_update &upd = ev.updates[ev.updates_index].emplace_back();
    upd.frame_num = current_frame;
    switch(timing) {
        case ev_timing_scanxy:
            upd.scan_x = d.scan_x;
            upd.scan_y = d.scan_y;
            return;
        case ev_timing_master_clock:
            upd.mclks = *master_clocks.ptr;
            upd.scan_y = master_clocks.cur_line;
            return;
        default:
            NOGOHERE;
    }

}

void debugger_report_event(cvec_ptr<debugger_view> &viewptr, i32 event_id)
{
    if (viewptr.vec == nullptr) {
        return;
    }
    if (event_id == -1) {
        return;
    }
    events_view *th = &viewptr.get().events;
    th->report_event(event_id);
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

void events_view::report_draw_start()
{
    switch(timing) {
        case ev_timing_master_clock:
            master_clocks.draw_starts[master_clocks.back_buffer][master_clocks.cur_line] = *master_clocks.ptr;
            //printf("\nLine %d Draw start %lld", master_clocks.cur_line, master_clocks.draw_starts[master_clocks.back_buffer][master_clocks.cur_line] - master_clocks.lines[master_clocks.back_buffer][master_clocks.cur_line]);
            break;
        case ev_timing_scanxy:
            printf("OHNO");
            break;
        default:
            NOGOHERE;
    }
}

void events_view::report_line(i32 line_num)
{
    switch(timing) {
        case ev_timing_master_clock: {
            /*i32 last_line = line_num - 1;
            if (last_line > 0) {
                u64 line_len = *master_clocks.ptr - master_clocks.lines[master_clocks.back_buffer][last_line];
                printf("\nLINE LEN %lld", line_len);
            }*/
            master_clocks.lines[master_clocks.back_buffer][line_num] = *master_clocks.ptr;
            master_clocks.draw_starts[master_clocks.back_buffer][line_num] = *master_clocks.ptr;
            master_clocks.cur_line = line_num;
            break; }
        case ev_timing_scanxy:
            break;
        default:
        NOGOHERE;
    }
}

void events_view::render(u32 *buf, u32 out_width, u32 out_height)
{
    // Render our current events to buf
    for (auto &ev : events) {
        if (!ev.display_enabled) continue;
        for (auto &upd : ev.updates[ev.updates_index ^ 1]) {
            if (upd.scan_y >= display[0].height) {
                upd.scan_y = display[0].height - 1;
            }
            switch(timing) {
                case ev_timing_scanxy:
                    draw_box_3x3(buf, upd.scan_x, upd.scan_y, out_width, out_height, ev.color);
                    break;
                case ev_timing_master_clock: {
                    i64 mc_per_line = master_clocks.per_line;
                    i64 mc_cur = master_clocks.lines[master_clocks.front_buffer][upd.scan_y];
                    i64 clk = static_cast<i64>(upd.mclks) - mc_cur;
                    u32 sx;
                    float perc = static_cast<float>(clk) / static_cast<float>(mc_per_line);
                    if (clk < 0) {
                        //printf("\nOH NO ITS OFF THE LEFT");
                        sx = 1;
                    }
                    else if (clk > mc_per_line) {
                        sx = display[0].width - 1;
                    }
                    else {
                        sx = static_cast<u32>(perc * static_cast<float>(display[0].width));
                    }
                    draw_box_3x3(buf, sx, upd.scan_y, out_width, out_height, ev.color);
                    break; }
                default:
                    NOGOHERE;

            }
        }
    }
}