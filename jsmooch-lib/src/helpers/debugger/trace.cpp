//
// Created by . on 12/28/24.
//

#include <stdio.h>
#include <stdarg.h>

#include "trace.h"

#define DEFAULT_STRING_SZ 80


static void trace_view_line_init(struct trace_line *this)
{
    source_id = -1;
    empty = 1;
}

static void trace_view_line_delete(struct trace_line *this)
{
    for (u32 i = 0; i < MAX_TRACE_COLS; i++) {
        jsm_string_delete(&cols[i]);
    }
}

static void trace_view_line_clear(struct trace_line *this, u32 num_cols)
{
    empty = 1;
    for (u32 i = 0; i < num_cols; i++) {
        jsm_string_quickempty(&cols[i]);
    }
}

trace_view::trace_view()
{
    lptr = &lines[0];
    current_output_line = 0;
    lptr->source_id = -1;
}

static void trace_view_category_init(struct trace_view_col *this)
{
    name[0] = 0;
    default_size = -1;
}

static void trace_view_category_delete(struct trace_view_col *this)
{
    name[0] = 0;
    default_size = -1;
}

void trace_view_delete(struct trace_view *this) {
    for (u32 i = 0; i < cvec_len(&lines); i++) {
        trace_view_line_delete(cvec_get(&lines, i));
    }
    for (u32 i = 0; i < cvec_len(&columns); i++) {
        trace_view_category_delete(cvec_get(&columns, i));
    }
    cvec_delete(&lines);
    cvec_delete(&columns);
}

void trace_view_add_col(struct trace_view *this, const char *name, i32 default_size)
{
    assert(cvec_len(&columns) < MAX_TRACE_COLS);
    struct trace_view_col *new_col = cvec_push_back(&columns);
    snprintf(new_col->name, sizeof(new_col->name), "%s", name);
    new_col->default_size = default_size;
}

void trace_view_clear(struct trace_view *this)
{
    num_trace_lines = 0;
    waiting_for_endline = 1;
    current_output_line = 0;
    lptr = cvec_get(&lines, 0);
}

void trace_view_printf(struct trace_view *this, u32 col, char *format, ...)
{
    if (waiting_for_startline) {
        assert(1==2);
        return;
    }
    assert(!waiting_for_startline);
    assert(col < cvec_len(&columns));
    va_list va;
    va_start(va, format);
    jsm_string_vsprintf(&lptr->cols[col], format, va);
    va_end(va);
}

void trace_view_startline(struct trace_view *this, i32 source)
{
    struct trace_line *l;
    if (first_line) {
        first_line = 0;
        l = lptr;
    }
    else {
        if (waiting_for_endline) {
            trace_view_endline(this);
        }

        l = cvec_get(&lines, current_output_line);
        current_output_line = (current_output_line + 1) % max_trace_lines;
        trace_view_line_clear(l, cvec_len(&columns));
    }

    waiting_for_startline = 0;
    l->source_id = source;
    lptr = l;
}

void trace_view_endline(struct trace_view *this)
{
    waiting_for_endline = 0;
    waiting_for_startline = 1;
    num_trace_lines++;
    if (num_trace_lines > max_trace_lines) num_trace_lines--;
}

struct trace_line *trace_view_get_line(struct trace_view *this, int row)
{
    if (row >= num_trace_lines - 1)
      row = (int)num_trace_lines - 1;
    u32 n = (current_output_line + row) % max_trace_lines;
    struct trace_line *l = cvec_get(&lines, n);
    return l;
}