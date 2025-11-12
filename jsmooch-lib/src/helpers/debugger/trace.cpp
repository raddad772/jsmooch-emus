//
// Created by . on 12/28/24.
//

#include <cassert>
#include <cstdarg>

#include "debugger.h"
#include "trace.h"

#define DEFAULT_STRING_SZ 80

void trace_line::clear(u32 num_cols)
{
    empty = 1;
    for (u32 i = 0; i < num_cols; i++) {
        cols[i].quickempty();
    }
}

trace_view::trace_view()
{
    lines.reserve(max_trace_lines);
    lines.emplace_back();
    lptr = &lines[0];
    current_output_line = 0;
    lptr->source_id = -1;
    columns.reserve(MAX_TRACE_COLS);
}

void trace_view::add_col(const char *inname, const i32 default_size)
{
    assert(columns.size() < MAX_TRACE_COLS);
    trace_view_col &new_col = columns.emplace_back();
    snprintf(new_col.name, sizeof(new_col.name), "%s", inname);
    new_col.default_size = default_size;
}

void trace_view::clear()
{
    num_trace_lines = 0;
    waiting_for_endline = 1;
    current_output_line = 0;
    lptr = &lines.at(0);
}

void trace_view::printf(u32 col, char *format, ...)
{
    if (waiting_for_startline) {
        assert(1==2);
        return;
    }
    assert(!waiting_for_startline);
    assert(col < columns.size());
    va_list va;
    va_start(va, format);
    lptr->cols[col].vsprintf(format, va);
    va_end(va);
}

void trace_view::startline(i32 source)
{
    trace_line *l;
    if (first_line) {
        first_line = 0;
        l = lptr;
    }
    else {
        if (waiting_for_endline) {
            endline();
        }

        l = &lines.at(current_output_line);
        current_output_line = (current_output_line + 1) % max_trace_lines;
        l->clear(columns.size());
    }

    waiting_for_startline = 0;
    l->source_id = source;
    lptr = l;
}

void trace_view::endline()
{
    waiting_for_endline = 0;
    waiting_for_startline = 1;
    num_trace_lines++;
    if (num_trace_lines > max_trace_lines) num_trace_lines--;
}

trace_line *trace_view::get_line(int row)
{
    if (row >= num_trace_lines - 1)
      row = static_cast<int>(num_trace_lines) - 1;
    u32 n = (current_output_line + row) % max_trace_lines;
    trace_line *l = &lines.at(n);
    return l;
}