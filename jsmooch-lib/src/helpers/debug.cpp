//
// Created by Dave on 1/24/2024.
//
#if !defined(_MSC_VER)
#include <unistd.h>
#include <pwd.h>
#endif
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdarg>

#define DEBUG_IMPL
#include "debug.h"

#include "user.h"

jsm_debug_struct dbg{};
static u32 init_already = 0;

void dbg_init()
{
    if (init_already) {
        printf("\nINIT ALREADY!");
        return;
    }
    init_already = 1;
    dbg.watch = 0;
    dbg.first_flush = 1;
    dbg.do_break = false;
    dbg.trace_on = 0;
    dbg.msg_last_newline = nullptr;
    //dbg.msg., 5*1024*1024);
    LT_init(&dbg.last_traces);
    dbg.var = 0;
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void dbg_clear_msg()
{
    dbg.msg.empty();
    dbg.msg_last_newline = dbg.msg.cur;
}

static int did_DFT = 0;
static int DFT_line = 0;
static char DFT_buf[4096];

#define DFT_file "/Users/Dave/js_sonic3_save.log"

static FILE* DFTf = nullptr;

void DFT(const char *format, ...)
{
    if (DFTf == nullptr) {
        DFTf = fopen(DFT_file, "w");
    }
    va_list va;
    va_start(va, format);
    int a = vsnprintf(DFT_buf, 4096, format, va);
    va_end(va);
    fwrite(DFT_buf, 1, a, DFTf);
    DFT_line++;
    if ((DFT_line % 1000) == 0)
        fflush(DFTf);
}

int dbg_printf(const char *format, ...)
{
    //if (!dbg.trace_on) return 0;
    jsm_string *th = &dbg.msg;
    // do jsm_sprintf, basically, minus one line
    if (th->ptr == nullptr) {
        assert(1==0);
        return 0;
    }
    va_list va;
    va_start(va, format);
    int a = vsnprintf(th->cur, th->allocated_len - (th->cur - th->ptr), format, va);
    va_end(va);

    // Scan for newlines
    while(*(th->cur)!=0) {
        if (*(th->cur) == '\n')
            dbg.msg_last_newline = th->cur;
        th->cur++;
    }
    if ((th->allocated_len - (th->cur - th->ptr)) < 1000) {
        dbg_flush();
    }
    return a;
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

static void construct_path(char* w, size_t sz, const char* who)
{
    const char *homeDir = get_user_dir();
    char *tp = w;
#if defined(_MSC_VER)
    tp += sprintf(tp, "%s\\dev\\%s", homeDir, who);
#else
    tp += snprintf(tp, sz, "%s/%s", homeDir, who);
#endif
}

void dbg_flush()
{
    if (!dbg.trace_on) return;
#if !defined(LYCODER) && !defined(REICAST_DIFF)
    printf("%s", dbg.msg.ptr);
    fflush(stdout);
#endif
#ifdef DBG_LOG_TO_FILE
    char fpath[250];
    printf("\nOUTPUTTING...");
    construct_path(fpath, sizeof(fpath), "my_spu.txt");
    if (dbg.first_flush) {
        dbg.first_flush = 0;
        remove(fpath);
    }
    FILE *w = fopen(fpath, "a");
    fprintf(w, "%s", dbg.msg.ptr);
    fflush(w);
    fclose(w);
#endif
    dbg_clear_msg();
}

void dbg_delete()
{
    dbg_clear_msg();
    dbg.msg_last_newline = nullptr;
}

void jsm_copy_read_trace (jsm_debug_read_trace *dst, jsm_debug_read_trace *src)
{
    dst->ptr = src->ptr;
    dst->read_trace = src->read_trace;
}

void dbg_unbreak()
{
    dbg.do_break = false;
}

void dbg_break(const char *reason, u64 cycles)
{
#ifdef DISABLE_BREAK
    printf("\nBREAK IGNORED!");
#else
    dbg.do_break = true;
    //assert(1==2);
    if (cycles != 0)
        printf("\nBREAK! %s cyc:%lld", reason, cycles);
    else
        printf("\nBREAK! %s", reason);
#endif
    fflush(stdout);
#ifdef DUMP_LAST_TRACES_ON_BREAK
    if (!dbg.trace_on) // If traces are on, we don't want doubles
        dbg_LT_dump();
    else
        dbg_LT_clear();
#endif
#ifdef TRACE_ON_BRK
    dbg_enable_trace();
    dbg.var = 0;
#endif
}

void dbg_enable_trace()
{
#ifndef TRACE_FORCE_OFF
    dbg.trace_on = 1;
#endif
}

void dbg_disable_trace()
{
    dbg.trace_on = 0;
}

void dbg_enable_cpu_trace(enum debug_sources source)
{
    switch(source) {
        case DS_z80:
            dbg.traces.z80.instruction = 1;
            break;
        case DS_m68000:
            dbg.traces.m68000.instruction = 1;
            break;
        default:
            printf("\nFORGOT THIS80");
            break;
    }
}

void dbg_disable_cpu_trace(enum debug_sources source)
{
    switch(source) {
        case DS_z80:
            dbg.traces.z80.instruction = 0;
            break;
        case DS_m68000:
            dbg.traces.m68000.instruction = 0;
            break;
        default:
            printf("\nFORGOT THIS87");
            break;
    }
}


// Last Traces, a rolling buffer of the last traces
void LT_init(last_traces_t* th)
{
    for (auto & entrie : th->entries) {
        memset(entrie, 0, LAST_TRACES_MSG_LEN);
    }
    th->head = 0;
    th->len = 0;
    th->curptr = &th->entries[0][0];
}

void LT_printf(last_traces_t* th, char *format, ...)
{
    u32 cur_entry = (th->head + th->len) % LAST_TRACES_LEN;
    u32 cur_len = th->curptr - th->entries[cur_entry];
    u32 len_left = LAST_TRACES_MSG_LEN - cur_len;

    va_list va;
    va_start(va, format);

    th->curptr += vsnprintf(th->curptr, len_left, format, va);
    va_end(va);
}

void LT_endline(last_traces_t* th)
{
    th->len = (th->len + 1);
    if (th->len == LAST_TRACES_LEN) {
        th->len--;
        th->head = (th->head + 1) % LAST_TRACES_LEN;
    }
    u32 cur_entry = (th->head + th->len) % LAST_TRACES_LEN;
    memset(&th->entries[cur_entry][0], 0, LAST_TRACES_MSG_LEN);
    th->curptr = &th->entries[cur_entry][0];
}

void LT_seek_in_line(last_traces_t* th, u32 where)
{
    u32 cur_entry = (th->head + th->len) % LAST_TRACES_LEN;
    i64 current_line_pos = th->curptr - &th->entries[cur_entry][0];
    i64 number_to_move = (i32)where - current_line_pos;
    while(number_to_move > 0) {
        *th->curptr = ' ';
        th->curptr++;
        number_to_move--;
    }
    *th->curptr = 0;
}

void LT_dump_to_dbg(last_traces_t* th)
{
    u32 old_dbg = dbg.trace_on;
    dbg.trace_on = 1;
    u32 len = th->len;
    u32 index = th->head;
    printf("\n-------------LAST TRACES");
    while(len > 0) {
        printf("%s", th->entries[index]);
        //printf("%s", th->entries[index]);
        index = (index + 1) % LAST_TRACES_LEN;
        len--;
    }

    LT_init(th);
    dbg.trace_on = old_dbg;
}
