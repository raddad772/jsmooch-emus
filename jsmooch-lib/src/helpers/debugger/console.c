//
// Created by . on 3/1/25.
//

#include <stdio.h>
#include <string.h>
#include "console.h"

void console_view_init(struct console_view *this)
{
    jsm_string_init(&this->buffer[0], 1024*1024);
    jsm_string_init(&this->buffer[1], 1024*1024);
    this->cur_buf = 0;
    memset(this->name, 0, sizeof(this->name));
}

void console_view_delete(struct console_view *this)
{
    jsm_string_delete(&this->buffer[0]);
    jsm_string_delete(&this->buffer[1]);
}

void console_view_add_char(struct console_view *this, u8 c)
{
    struct jsm_string *cb = &this->buffer[this->cur_buf];
    u64 len = cb->cur - cb->ptr;
    if ((len + 2) >= cb->allocated_len) {
        this->cur_buf ^= 1;
        cb = &this->buffer[this->cur_buf];
        jsm_string_quickempty(cb);
    }
    jsm_string_sprintf(cb, "%c", c);
    this->updated = 1;
}

void console_view_add_cstr(struct console_view *this, char *s)
{
    u32 l = strlen(s);
    struct jsm_string *cb = &this->buffer[this->cur_buf];
    u64 len = cb->cur - cb->ptr;
    if ((len + l + 1) >= cb->allocated_len) {
        this->cur_buf ^= 1;
        cb = &this->buffer[this->cur_buf];
        jsm_string_quickempty(cb);
    }
    jsm_string_sprintf(cb, "%s", s);
    this->updated = 1;
}

void console_view_render_to_buffer(struct console_view *tv, char *output, u64 sz)
{
    struct jsm_string *s = &tv->buffer[tv->cur_buf ^ 1];
    char *v = output;
    u64 num_chars = snprintf(v, sz, "%s", s->ptr);
    sz -= num_chars;
    v += num_chars;

    s = &tv->buffer[tv->cur_buf];
    snprintf(v, sz, "%s", s->ptr);
}