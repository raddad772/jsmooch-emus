//
// Created by Dave on 1/24/2024.
//
#if !defined(_MSC_VER)
#include <unistd.h>
#include <pwd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>

#include "debug.h"
#include "user.h"


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
    dbg.first_flush = 1;
    dbg.do_break = false;
    dbg.trace_on = 0;
    dbg.msg_last_newline = 0;
    jsm_string_init(&dbg.msg, 5*1024*1024);
    LT_init(&dbg.last_traces);
    dbg.var = 0;
}

#ifndef MIN
#define MIN(x,y) (((x) <= (y)) ? (x) : (y));
#endif

void dbg_clear_msg()
{
    jsm_string_empty(&dbg.msg);
    dbg.msg_last_newline = dbg.msg.cur;
}

static int did_DFT = 0;
static int DFT_line = 0;
static char DFT_buf[4096];

#define DFT_file "/Users/Dave/js_sonic3_save.log"

static FILE* DFTf = NULL;

void DFT(char *format, ...)
{
    if (DFTf == NULL) {
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

int dbg_printf(char *format, ...)
{
    if (!dbg.trace_on) return 0;
    struct jsm_string*this = &dbg.msg;
    // do jsm_sprintf, basically, minus one line
    if (this->ptr == NULL) {
        assert(1==0);
        return 0;
    }
    va_list va;
    va_start(va, format);
    int a = vsnprintf(this->cur, this->allocated_len - (this->cur - this->ptr), format, va);
    va_end(va);

    // Scan for newlines
    while(*(this->cur)!=0) {
        if (*(this->cur) == '\n')
            dbg.msg_last_newline = this->cur;
        this->cur++;
    }
    if ((this->allocated_len - (this->cur - this->ptr)) < 1000) {
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
    tp += snprintf(tp, sz, "%s/dev/%s", homeDir, who);
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
    construct_path(fpath, sizeof(fpath), "my_cpu.txt");
    if (dbg.first_flush) {
        dbg.first_flush = 0;
        remove(fpath);
    }
    FILE *w = fopen(fpath, "a");
    fprintf(w, "%s", dbg.msg.ptr);
    fflush(w);
    fclose(w);
    dbg_break("DID IT!", 0);
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
void LT_init(struct last_traces_t* this)
{
    for (u32 i = 0; i < LAST_TRACES_LEN; i++) {
        memset(this->entries[i], 0, LAST_TRACES_MSG_LEN);
    }
    this->head = 0;
    this->len = 0;
    this->curptr = &this->entries[0][0];
}

void LT_printf(struct last_traces_t* this, char *format, ...)
{
    u32 cur_entry = (this->head + this->len) % LAST_TRACES_LEN;
    u32 cur_len = this->curptr - this->entries[cur_entry];
    u32 len_left = LAST_TRACES_MSG_LEN - cur_len;

    va_list va;
    va_start(va, format);

    this->curptr += vsnprintf(this->curptr, len_left, format, va);
    va_end(va);
}

void LT_endline(struct last_traces_t* this)
{
    this->len = (this->len + 1);
    if (this->len == LAST_TRACES_LEN) {
        this->len--;
        this->head = (this->head + 1) % LAST_TRACES_LEN;
    }
    u32 cur_entry = (this->head + this->len) % LAST_TRACES_LEN;
    memset(&this->entries[cur_entry][0], 0, LAST_TRACES_MSG_LEN);
    this->curptr = &this->entries[cur_entry][0];
}

void LT_seek_in_line(struct last_traces_t* this, u32 where)
{
    u32 cur_entry = (this->head + this->len) % LAST_TRACES_LEN;
    i64 current_line_pos = this->curptr - &this->entries[cur_entry][0];
    i64 number_to_move = (i32)where - current_line_pos;
    while(number_to_move > 0) {
        *this->curptr = ' ';
        this->curptr++;
        number_to_move--;
    }
    *this->curptr = 0;
}

void LT_dump_to_dbg(struct last_traces_t* this)
{
    u32 old_dbg = dbg.trace_on;
    dbg.trace_on = 1;
    u32 len = this->len;
    u32 index = this->head;
    printf("\n-------------LAST TRACES");
    while(len > 0) {
        printf("%s", this->entries[index]);
        //printf("%s", this->entries[index]);
        index = (index + 1) % LAST_TRACES_LEN;
        len--;
    }

    LT_init(this);
    dbg.trace_on = old_dbg;
}
