//
// Created by . on 12/28/24.
//

#include <stdio.h>
#include <stdarg.h>

#include "trace.h"

#define DEFAULT_STRING_SZ 50


static void trace_view_line_init(struct trace_line *this)
{
    for (u32 i = 0; i < MAX_TRACE_COLS; i++) {
        jsm_string_init(&this->cols[i], DEFAULT_STRING_SZ);
    }
    this->source_id = -1;
    this->empty = 1;
}

static void trace_view_line_delete(struct trace_line *this)
{
    for (u32 i = 0; i < MAX_TRACE_COLS; i++) {
        jsm_string_delete(&this->cols[i]);
    }
}

static void trace_view_line_clear(struct trace_line *this, u32 num_cols)
{
    this->empty = 1;
    for (u32 i = 0; i < num_cols; i++) {
        jsm_string_quickempty(&this->cols[i]);
    }
}

void trace_view_init(struct trace_view *this)
{
    this->name[0] = 0;
    this->max_trace_lines = 100;
    cvec_init(&this->lines, sizeof(struct trace_line), this->max_trace_lines);
    cvec_init(&this->columns, sizeof(struct trace_view_col), MAX_TRACE_COLS);
    for (u32 i = 0; i < this->max_trace_lines; i++) {
        trace_view_line_init(cvec_push_back(&this->lines));
    }
    printf("\nTRACE VIEW INIT...");
    this->num_trace_lines = 0;
    this->waiting_for_startline = 0;
    this->waiting_for_endline = 1;
    this->current_output_line = 0;
    this->lptr = cvec_get(&this->lines, 0);
    this->lptr->source_id = -1;
    this->first_line = 1;
}

static void trace_view_category_init(struct trace_view_col *this)
{
    this->name[0] = 0;
    this->default_size = -1;
}

static void trace_view_category_delete(struct trace_view_col *this)
{
    this->name[0] = 0;
    this->default_size = -1;
}

void trace_view_delete(struct trace_view *this) {
    for (u32 i = 0; i < cvec_len(&this->lines); i++) {
        trace_view_line_delete(cvec_get(&this->lines, i));
    }
    for (u32 i = 0; i < cvec_len(&this->columns); i++) {
        trace_view_category_delete(cvec_get(&this->columns, i));
    }
    cvec_delete(&this->lines);
    cvec_delete(&this->columns);
}

void trace_view_add_col(struct trace_view *this, const char *name, i32 default_size, u32 color)
{
    assert(cvec_len(&this->columns) < MAX_TRACE_COLS);
    struct trace_view_col *new_col = cvec_push_back(&this->columns);
    snprintf(new_col->name, sizeof(new_col->name), "%s", name);
    new_col->default_size = default_size;
    new_col->color = color;
}

void trace_view_clear(struct trace_view *this)
{
    this->num_trace_lines = 0;
    this->waiting_for_endline = 1;
    this->current_output_line = 0;
    this->lptr = cvec_get(&this->lines, 0);
}

void trace_view_printf(struct trace_view *this, u32 col, char *format, ...)
{
    if (this->waiting_for_startline) {
        assert(1==2);
        return;
    }
    assert(!this->waiting_for_startline);
    assert(col < cvec_len(&this->columns));
    va_list va;
    va_start(va, format);
    jsm_string_vsprintf(&this->lptr->cols[col], format, va);
    va_end(va);
}

void trace_view_startline(struct trace_view *this, i32 source)
{
    struct trace_line *l;
    if (this->first_line) {
        this->first_line = 0;
        l = this->lptr;
    }
    else {
        if (this->waiting_for_endline) {
            trace_view_endline(this);
        }

        l = cvec_get(&this->lines, this->current_output_line);
        this->current_output_line = (this->current_output_line + 1) % this->max_trace_lines;
        trace_view_line_clear(l, cvec_len(&this->columns));
    }

    this->waiting_for_startline = 0;
    l->source_id = source;
    this->lptr = l;
}

void trace_view_endline(struct trace_view *this)
{
    this->waiting_for_endline = 0;
    this->waiting_for_startline = 1;
    this->num_trace_lines++;
    if (this->num_trace_lines > this->max_trace_lines) this->num_trace_lines--;
}

struct trace_line *trace_view_get_line(struct trace_view *this, int row)
{
    if (row >= this->num_trace_lines - 1)
      row = (int)this->num_trace_lines - 1;
    u32 n = (this->current_output_line + row) % this->max_trace_lines;
    struct trace_line *l = cvec_get(&this->lines, n);
    return l;
}