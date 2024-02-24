//
// Created by Dave on 1/24/2024.
//
#include "stdio.h"
#include "string.h"
#include "malloc.h"
#include "assert.h"
#include "stdarg.h"

#include "debug.h"


struct jsm_debug_struct dbg;
static u32 init_already = 0;

void dbg_init()
{
    if (init_already) {
        printf("\nINIT ALREADY!");
        return;
    }
    init_already = 1;
    dbg.watch = 0;
    printf("\nDBG INIT!");
    dbg.do_break = false;
    dbg.trace_on = 0;
    dbg.msg_last_newline = 0;
    jsm_string_init(&dbg.msg, 5*1024*1024);
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void dbg_clear_msg()
{
    jsm_string_empty(&dbg.msg);
    dbg.msg_last_newline = dbg.msg.cur;
}

void dbg_printf(char *format, ...)
{
    if (!dbg.trace_on) return;
    struct jsm_string*this = &dbg.msg;
    // do jsm_sprintf, basically, minus one line
    if (this->ptr == NULL) {
        assert(1==0);
        return;
    }
    va_list va;
    va_start(va, format);
    vsnprintf(this->cur, this->len - (this->cur - this->ptr), format, va);
    va_end(va);

    // Scan for newlines
    while(*(this->cur)!=0) {
        if (*(this->cur) == '\n')
            dbg.msg_last_newline = this->cur;
        this->cur++;
    }
    if ((this->len - (this->cur - this->ptr)) < 1000) {
        dbg_flush();
    }

}

void dbg_seek_in_line(u32 pos)
{
    if (!dbg.trace_on) return;
    i64 current_line_pos = dbg.msg.cur - dbg.msg_last_newline;
    i64 number_to_move = (i32)pos - current_line_pos;
    if (number_to_move > 0) {
        while(number_to_move > 0) {
            *dbg.msg.cur = ' ';
            dbg.msg.cur++;
            number_to_move--;
        }
        *dbg.msg.cur = 0;
    }
}

void dbg_flush()
{
    if (!dbg.trace_on) return;
    printf("%s", dbg.msg.ptr);
    fflush(stdout);
#ifdef DBG_LOG_TO_FILE
    FILE *w = fopen("c:\\dev\\sh4.log", "a");
    fprintf(w, "%s", dbg.msg.ptr);
    fflush(w);
    fclose(w);
#endif
    dbg_clear_msg();
}

void dbg_delete()
{
    dbg_clear_msg();
    dbg.msg_last_newline = 0;
}

void jsm_copy_read_trace (struct jsm_debug_read_trace *dst, struct jsm_debug_read_trace *src)
{
    dst->ptr = src->ptr;
    dst->read_trace = src->read_trace;
}

void dbg_unbreak()
{
    dbg.do_break = false;
}

void dbg_break()
{
    printf("\nBREAK!");
    fflush(stdout);
    dbg.do_break = true;
    fflush(stdout);
}

void dbg_enable_trace()
{
    dbg.trace_on = 1;
}

void dbg_disable_trace()
{
    dbg.trace_on = 0;
}
