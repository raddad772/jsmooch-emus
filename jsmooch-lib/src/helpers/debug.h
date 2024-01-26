//
// Created by Dave on 1/24/2024.
//

#ifndef JSMOOCH_EMUS_DEBUG_H
#define JSMOOCH_EMUS_DEBUG_H

#include "helpers/int.h"

#define MAX_DEBUG_MSG 10000

struct jsm_debug_struct {
    u32 do_break;

    u32 msg_len;
    u32 msg_head;
    char *msg[MAX_DEBUG_MSG];
};

extern struct jsm_debug_struct dbg;

void dbg_init();
void dbg_add_msg(char *what);
char *dbg_get_msg();
void dbg_clear_msg();
void dbg_delete();

#endif //JSMOOCH_EMUS_DEBUG_H
