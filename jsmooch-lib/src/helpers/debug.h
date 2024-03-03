//
// Created by Dave on 1/24/2024.
//

#ifndef JSMOOCH_EMUS_DEBUG_H
#define JSMOOCH_EMUS_DEBUG_H

//#define SH4_BRK 0x8c009090
//#define DC_MEM_W_BRK 0x8cf70ff4

#define TRACE_ON_BRK     // Enable tracing on break
//#define DBG_LOG_TO_FILE // log debug to file
//#define LYCODER        // lycoder-format traces for easy winmerge
#define DO_LAST_TRACES   // keeps last X traces, slows down emulation
#define DUMP_LAST_TRACES_ON_BREAK

#define SH4_DBG_SUPPORT
//#define Z80_DBG_SUPPORT
#define M6502_DBG_SUPPORT
#define SM83_DBG_SUPPORT

#include "helpers/int.h"
#include "helpers/jsm_string.h"

#define MAX_DEBUG_MSG 2000000
#define LAST_TRACES_LEN 100
#define LAST_TRACES_MSG_LEN 200

struct last_traces_t {
    char entries[LAST_TRACES_LEN][LAST_TRACES_MSG_LEN];
    u32 head;
    u32 len;

    char *curptr;
};

struct jsm_debug_struct {
    u32 do_break;
    u32 brk_on_NMIRQ;

    u32 trace_on;
    u32 watch;
    struct jsm_string msg;
    char *msg_last_newline;

    struct last_traces_t last_traces;
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

void LT_init(struct last_traces_t* this);
void LT_printf(struct last_traces_t* this, char *format, ...);
void LT_endline(struct last_traces_t* this);
void LT_seek_in_line(struct last_traces_t* this, u32 where);
void LT_dump_to_dbg(struct last_traces_t* this);

#ifdef DO_LAST_TRACES
#define dbg_LT_printf(...) LT_printf(&dbg.last_traces, __VA_ARGS__)
#define dbg_LT_endline() LT_endline(&dbg.last_traces)
#define dbg_LT_seek_in_line(where) LT_seek_in_line(&dbg.last_traces, where)
#define dbg_LT_dump() LT_dump_to_dbg(&dbg.last_traces)
#define dbg_LT_clear() LT_init(&dbg.last_traces)
#else
#define dbg_LT_printf(...) (void)0
#define dbg_LT_endline() (void)0
#define dbg_LT_seek_in_line(where) (void)0
#define dbg_LT_dump() (void)0
#define dbg_LT_clear() (void)0
#endif
void jsm_copy_read_trace(struct jsm_debug_read_trace *dst, struct jsm_debug_read_trace *src);

#endif //JSMOOCH_EMUS_DEBUG_H
