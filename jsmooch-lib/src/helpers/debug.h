//
// Created by Dave on 1/24/2024.
//

#ifndef JSMOOCH_EMUS_DEBUG_H
#define JSMOOCH_EMUS_DEBUG_H

#if !defined(NDEBUG)
#define JSDEBUG
#endif

//#define SH4_BRK 0x8c000100
//#define DC_MEM_W_BRK 0x8cf70ff4

//#define TRACE_ON_BRK     // Enable tracing on break
#define DBG_LOG_TO_FILE // log debug to file
#define DISABLE_BREAK
//#define LYCODER        // lycoder-format traces for easy winmerge
#define REICAST_DIFF
//#define DO_LAST_TRACES   // keeps last X traces, slows down emulation
//#define DUMP_LAST_TRACES_ON_BREAK
//#define TRACE_COLORS
//#define SH4_TRACE_INS_FETCH
//#define SH4_IRQ_DBG

#ifdef SH4_IRQ_DBG
#define SH4_IRQ_DBG_printf(...) printf(__VA_ARGS__)
#else
#define SH4_IRQ_DBG_printf(...) (void)0;
#endif

#define SH4_DBG_SUPPORT
//#define Z80_DBG_SUPPORT
#define M6502_DBG_SUPPORT
#define SM83_DBG_SUPPORT

#include "helpers/int.h"
#include "helpers/jsm_string.h"

#define MAX_DEBUG_MSG 2000000
#define LAST_TRACES_LEN 800
#define LAST_TRACES_MSG_LEN 200


struct DC;
struct last_traces_t {
    char entries[LAST_TRACES_LEN][LAST_TRACES_MSG_LEN];
    u32 head;
    u32 len;

    char *curptr;
};

#define MAX_DEBUG_MSG 2000000
struct jsm_debug_struct {
    u32 do_break;
    u32 brk_on_NMIRQ;

    u32 first_flush;

    u32 trace_on;
    u32 watch;
    struct jsm_string msg;
    char *msg_last_newline;

    struct last_traces_t last_traces;
    u32 var;
    struct DC* dcptr;
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


#ifdef TRACE_COLORS
#define DBGC_BLACK "\e[0;30m"
#define DBGC_RED "\e[0;31m"
#define DBGC_GREEN "\e[0;32m"
#define DBGC_YELLOW "\e[0;33m"
#define DBGC_BLUE "\e[0;34m"
#define DBGC_PURPLE "\e[0;35m"
#define DBGC_CYAN "\e[0;36m"
#define DBGC_WHITE "\e[0;37m"
#define DBGC_RST "\e[0m"

#else

#define DBGC_BLACK ""
#define DBGC_RED ""
#define DBGC_GREEN ""
#define DBGC_YELLOW ""
#define DBGC_BLUE ""
#define DBGC_PURPLE ""
#define DBGC_CYAN ""
#define DBGC_WHITE ""
#define DBGC_RST ""

#endif

#define DBGC_TRACE DBGC_RST
#define DBGC_READ DBGC_GREEN
#define DBGC_WRITE DBGC_RED

#endif //JSMOOCH_EMUS_DEBUG_H
