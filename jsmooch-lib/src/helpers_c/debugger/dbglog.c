//
// Created by . on 3/9/25.
//

#include <cstring>
#include <cstdio>
#include "dbglog.h"

struct dbglog_category_node *dbglog_category_get_root(dbglog_view *this)
{
    return &this->category_root;
}

struct dbglog_category_node *dbglog_category_add_node(dbglog_view *dv, dbglog_category_node *this, const char *name, const char *short_name, u32 id, u32 color)
{
    struct dbglog_category_node *d = cvec_push_back(&this->children);
    category_node_init(d);
    snprintf(d->name, sizeof(d->name), "%s", name);
    if (short_name)
        snprintf(d->short_name, sizeof(d->short_name), "%s", short_name);
    d->category_id = id;
    dv->id_to_category[id] = d;
    dv->id_to_color[id] = color;
    return d;
}

void dbglog_view_add_item(dbglog_view *this, u32 id, u64 timecode, enum dbglog_severity severity, const char *txt)
{
    if (!this->ids_enabled[id]) return;
    if (this->items.len >= MAX_DBGLOG_LINES) {
        // Move first entry forward
        this->items.first_entry = (this->items.first_entry + 1) % MAX_DBGLOG_LINES;
    }
    dbglog_entry *e = &this->items.data[this->items.next_entry];
    e->category_id = id;
    e->timecode = timecode;
    e->severity = severity;
    jsm_string_quickempty(&e->text);
    jsm_string_sprintf(&e->text, "%s", txt);

    this->items.next_entry = (this->items.next_entry + 1) % MAX_DBGLOG_LINES;

    if (this->items.len < MAX_DBGLOG_LINES) {
        this->items.len++;
    }
    this->updated = 3;
}


void dbglog_view_extra_printf(dbglog_view *this, const char *format, ...) {
    dbglog_entry *e = this->last_added;
    if (!e) {
        printf("\nWARN IT IS NULL!?");
        return;
    }
    jsm_string_quickempty(&e->extra);
    va_list va;
    va_start(va, format);
    jsm_string_vsprintf(&e->extra, format, va);
    va_end(va);
    this->updated = 3;

}

void dbglog_view_add_printf(dbglog_view *this, u32 id, u64 timecode, enum dbglog_severity severity, const char *format, ...) {
    if (!this->ids_enabled[id]) return;
    if (this->items.len >= MAX_DBGLOG_LINES) {
        // Move first entry forward
        this->items.first_entry = (this->items.first_entry + 1) % MAX_DBGLOG_LINES;
    }
    dbglog_entry *e = &this->items.data[this->items.next_entry];
    this->last_added = e;
    e->category_id = id;
    e->timecode = timecode;
    e->severity = severity;
    jsm_string_quickempty(&e->text);

    this->items.next_entry = (this->items.next_entry + 1) % MAX_DBGLOG_LINES;

    if (this->items.len < MAX_DBGLOG_LINES) {
        this->items.len++;
    }

    va_list va;
    va_start(va, format);
    jsm_string_vsprintf(&e->text, format, va);
    va_end(va);
    this->updated = 3;
}

void dbglog_view_render_to_buffer(dbglog_view *this, char *output, u64 sz)
{
    u32 total_line = 0;
    u32 total_len = 0;
    char *ptr = output;
    *ptr = 0;
    u32 idx = this->items.first_entry;
    while (total_line<this->items.len) {
        dbglog_entry *item = &this->items.data[idx];
        idx = (idx + 1) % this->items.len;
        total_line++;
        if (!this->ids_enabled[item->category_id]) continue;

        if (total_len >= sz) {
            printf("\nERROR EXCEED VIEW RENDER BUFFER SIZE!");
            return;
        }
        u32 len = snprintf(ptr, sz - total_len, "\n%s", item->text.ptr);
        total_len += len;
        ptr += len;
    }
}

u32 dbglog_count_visible_lines(dbglog_view *this)
{
    u32 n = 0;
    u32 idx = this->items.first_entry;
    for (u32 i = 0; i < this->items.len; i++) {
        dbglog_entry *e = &this->items.data[idx];
        if (this->ids_enabled[e->category_id]) n++;

        idx = (idx + 1) % this->items.len;
    }
    return n;
}

u32 dbglog_get_nth_visible(dbglog_view *this, u32 n)
{
    u32 nth = 0;
    u32 idx = this->items.first_entry;
    for (u32 i = 0; i < this->items.len; i++) {
        dbglog_entry *e = &this->items.data[idx];
        if (this->ids_enabled[e->category_id]) {
            if (nth == n) return idx;
            nth++;
        }

        idx = (idx + 1) % this->items.len;
    }
    return 0;
}

u32 dbglog_get_next_visible(dbglog_view *this, u32 start)
{
    u32 idx = (start + 1) % this->items.len;
    for (u32 i = 0; i < this->items.len; i++) {
        dbglog_entry *e = &this->items.data[idx];
        if (this->ids_enabled[e->category_id]) {
            return idx;
        }

        idx = (idx + 1) % this->items.len;
    }
    return start;
}
