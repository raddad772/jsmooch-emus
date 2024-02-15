//
// Created by Dave on 1/24/2024.
//

#ifndef JSMOOCH_EMUS_DEBUG_H
#define JSMOOCH_EMUS_DEBUG_H

#include "helpers/int.h"
#include "helpers/jsm_string.h"

#define MAX_DEBUG_MSG 2000000

struct jsm_debug_struct {
    u32 do_break;
    u32 brk_on_NMIRQ;

    u32 trace_on;
    u32 watch;
    struct jsm_string msg;
    char *msg_last_newline;
};

struct jsm_debug_read_trace {
    void *ptr;
    u32 (*read_trace)(void *,u32);
};

extern struct jsm_debug_struct dbg;

void dbg_printf(char *format, ...);
void dbg_seek_in_line(u32 pos);
void dbg_flush();
void dbg_clear_msg();

void dbg_enable_trace();
void dbg_disable_trace();

void dbg_init();
void dbg_add_msg(char *what);
char *dbg_get_msg();
void dbg_clear_msg();
void dbg_delete();
void dbg_break();
void dbg_unbreak();



void jsm_copy_read_trace(struct jsm_debug_read_trace *dst, struct jsm_debug_read_trace *src);

#endif //JSMOOCH_EMUS_DEBUG_H
