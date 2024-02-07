//
// Created by Dave on 1/24/2024.
//

#ifndef JSMOOCH_EMUS_DEBUG_H
#define JSMOOCH_EMUS_DEBUG_H

#include "helpers/int.h"

#define MAX_DEBUG_MSG 1000000

struct jsm_debug_struct {
    u32 do_break;
    u32 brk_on_NMIRQ;

    u32 msg_len;
    u32 msg_head;
    char *msg[MAX_DEBUG_MSG];
};

struct jsm_string {
    char *start;
    char *current;
    u32 len;
};

struct jsm_debug_read_trace {
    void *ptr;
    u32 (*read_trace)(void *,u32);
};

extern struct jsm_debug_struct dbg;

void dbg_init();
void dbg_add_msg(char *what);
char *dbg_get_msg();
void dbg_clear_msg();
void dbg_delete();
void dbg_break();

void jsm_string_init(struct jsm_string *str);
void jsm_copy_read_trace (struct jsm_debug_read_trace *dst, struct jsm_debug_read_trace *src);

#endif //JSMOOCH_EMUS_DEBUG_H
