//
// Created by . on 3/1/25.
//

#include <string.h>
#include "console.h"

void console_view::add_char(u8 c)
{
    jsm_string *cb = &buffer[cur_buf];
    u64 len = cb->cur - cb->ptr;
    if ((len + 2) >= cb->allocated_len) {
        cur_buf ^= 1;
        cb = &buffer[cur_buf];
        cb->quickempty();
    }
    cb->sprintf("%c", c);
    updated = 1;
}

void console_view::add_cstr(char *s)
{
    u32 l = strlen(s);
    jsm_string *cb = &buffer[cur_buf];
    u64 len = cb->cur - cb->ptr;
    if ((len + l + 1) >= cb->allocated_len) {
        cur_buf ^= 1;
        cb = &buffer[cur_buf];
        cb->quickempty();
    }
    cb->sprintf("%s", s);
    updated = 1;
}

void console_view::render_to_buffer(char *output, u64 sz)
{
    jsm_string *s = &buffer[cur_buf ^ 1];
    char *v = output;
    u64 num_chars = snprintf(v, sz, "%s", s->ptr);
    sz -= num_chars;
    v += num_chars;

    s = &buffer[cur_buf];
    snprintf(v, sz, "%s", s->ptr);
}
