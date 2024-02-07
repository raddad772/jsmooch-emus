//
// Created by Dave on 1/24/2024.
//
#include "stdio.h"
#include "string.h"
#include "malloc.h"

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
    printf("\nDBG INIT!");
    dbg.do_break = false;
    for (u32 i = 0; i < MAX_DEBUG_MSG; i++) {
        dbg.msg[i] = NULL;
    }
    dbg.msg_head = 0;
    dbg.msg_len = 0;
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void dbg_add_msg(char *what)
{
    if (dbg.msg_len > MAX_DEBUG_MSG) {
        printf("ERROR, DBG replacing an existing debug message!");
    }
    u32 r = (dbg.msg_head + dbg.msg_len) % MAX_DEBUG_MSG;
    u32 strl = strlen(what);
    if (dbg.msg[r] != NULL) {
        free(dbg.msg[r]);
        dbg.msg[r] = NULL;
    }
    dbg.msg[r] = malloc(strl+1);
    strcpy(dbg.msg[r], what);
    dbg.msg_len++;
    dbg.msg_len = MIN(dbg.msg_len, MAX_DEBUG_MSG);
}

char *dbg_get_msg()
{
    if (dbg.msg_len == 0) return NULL;
    u32 strl = strlen(dbg.msg[dbg.msg_head]);
    char *out = malloc(strl+1);
    strcpy(out, dbg.msg[dbg.msg_head]);

    free(dbg.msg[dbg.msg_head]);
    dbg.msg[dbg.msg_head] = NULL;

    dbg.msg_head = (dbg.msg_head + 1) % MAX_DEBUG_MSG;
    dbg.msg_len--;

    return out;
}

void dbg_clear_msg()
{
    for (u32 i = 0; i < MAX_DEBUG_MSG; i++) {
        if (dbg.msg[i] != NULL) {
            free(dbg.msg[i]);
            dbg.msg[i] = NULL;
        }
    }
    dbg.msg_len = 0;
    dbg.msg_head = 0;
}

void dbg_delete()
{
    dbg_clear_msg();
}

void jsm_copy_read_trace (struct jsm_debug_read_trace *dst, struct jsm_debug_read_trace *src)
{
    dst->ptr = src->ptr;
    dst->read_trace = src->read_trace;
}

void dbg_break()
{
    printf("Implement dbg_break()");
    fflush(stdout);
}